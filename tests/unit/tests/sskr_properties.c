/*
 * Randomised properties of SSKR generation and recombination.
 *
 * Every other test in this suite checks a particular case: a frozen vector, a
 * named configuration, one guard at a time. That catches a regression on that
 * case. This file checks invariants instead -- statements that must hold for
 * every valid input -- over many inputs drawn at random, which catches a
 * regression anywhere in the domain.
 *
 * Four properties, all of them self-checking: each compares the code to itself
 * across a round trip and needs no independent implementation of Shamir or of
 * GF(2^8) to compare against. That is a deliberate choice, not a shortcut. A
 * hand-written oracle that is subtly wrong reports divergences that do not
 * exist -- a log/antilog table built on a non-primitive generator is enough to
 * do it -- and the published vectors that do hold this code against an outside
 * specification are already covered by tests/sskr_interop_bc128.c and
 * tests/sskr.c. The frozen vectors anchor the behaviour to published values;
 * these sweep the domain around them. Neither replaces the other.
 *
 *   1. Round trip. For every valid (threshold, share count) and every valid
 *      secret length, generating then recombining returns exactly the secret.
 *   2. Any subset reaching the threshold works, not just the first shards.
 *   3. Below the threshold the failure is clean: an error is reported and the
 *      output buffer holds no partial result.
 *   4. A single flipped bit in an otherwise valid shard is rejected by the
 *      SLIP-39 digest.
 *
 * Reproducibility. Each iteration seeds the generator from a fixed root plus
 * its own index, so a case depends on nothing but a 64-bit number written in
 * this file. Every failure message prints that number along with the
 * configuration, which is the whole point: a property test that fails without
 * saying on which input is unusable.
 *
 * The generator is tests/unit/lib/testprng.h, passed as the `random_generator`
 * parameter these entry points already take. The suite's own
 * cx_rng_no_throw() writes 0, 1, 2, ... and restarts at every call; running a
 * property against it would visit one degenerate point while looking like a
 * sweep. It is left exactly as it is -- nine test files depend on the shards it
 * produces.
 *
 * Bounds are covered explicitly rather than left to the draw, which visits them
 * rarely: the boundary table below pairs the extreme thresholds and share
 * counts with all nine valid secret lengths, and the random iterations cover
 * the rest. SSS_MAX_SHARE_COUNT is used symbolically throughout, so this file
 * stays correct in a build where it is 10 (TARGET_NANOS) rather than 16.
 */

#include <cmocka.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sskr.h"
#include "sss-constants.h"
#include "testprng.h"
#include "testutils.h"

/* Roots of the four streams. Fixed, never derived from the clock: changing one
 * re-rolls that property's cases, and a case that failed is replayed by running
 * the same binary again. */
#define SEED_ROUND_TRIP UINT64_C(0x53B1A9C304D71E60)
#define SEED_ANY_SUBSET UINT64_C(0x53B1A9C304D71E61)
#define SEED_UNDER_COUNT UINT64_C(0x53B1A9C304D71E62)
#define SEED_ONE_BIT UINT64_C(0x53B1A9C304D71E63)

/* Measured, not guessed: this file runs in well under a second, and the three
 * property targets together cost a fraction of the suite's total. A property
 * test that gets disabled for being slow protects nothing. */
#define RANDOM_ITERATIONS 200

#define SECRET_LEN_MIN SSKR_MIN_STRENGTH_BYTES
#define SECRET_LEN_MAX SSKR_MAX_STRENGTH_BYTES
#define SECRET_LEN_STEPS (((SECRET_LEN_MAX - SECRET_LEN_MIN) / 2) + 1)

#define MAX_SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SECRET_LEN_MAX)
#define MAX_SHARD_BUFFER (SSS_MAX_SHARE_COUNT * MAX_SHARD_LEN)

/* Sentinel written into every output buffer before a call that must fail, so
 * that "the buffer was left alone" and "the buffer was wiped" are told apart.
 */
#define SENTINEL 0xA5

typedef struct {
    uint8_t threshold;
    uint8_t count;
    uint8_t secret_len;
} config_t;

/* The corners a uniform draw almost never visits: the degenerate 1-of-1, the
 * smallest real split, the widest one, the all-shards-needed one, and one share
 * short of it. Each is run against all nine valid secret lengths. */
static const uint8_t boundary_pairs[][2] = {
    {1, 1},
    {2, 2},
    {2, SSS_MAX_SHARE_COUNT},
    {SSS_MAX_SHARE_COUNT - 1, SSS_MAX_SHARE_COUNT},
    {SSS_MAX_SHARE_COUNT, SSS_MAX_SHARE_COUNT},
};

/* Draws a valid configuration. sskr_count_shards() rejects a threshold of 1
 * with more than one share (SSKR_ERROR_INVALID_SINGLETON_MEMBER), so the only
 * configuration with threshold 1 is 1-of-1; everything else has a threshold
 * between 2 and its share count. `min_threshold` is 2 for the properties that
 * need a real split -- below the threshold, and digest verification, are both
 * meaningless at 1-of-1. */
static config_t draw_config(uint8_t min_threshold) {
    config_t config;

    if (min_threshold < 2) {
        config.count = (uint8_t)(1 + test_prng_below(SSS_MAX_SHARE_COUNT));
        config.threshold =
            (config.count == 1)
                ? 1
                : (uint8_t)(2 + test_prng_below(config.count - 1));
    } else {
        config.count = (uint8_t)(2 + test_prng_below(SSS_MAX_SHARE_COUNT - 1));
        config.threshold = (uint8_t)(2 + test_prng_below(config.count - 1));
    }
    config.secret_len =
        (uint8_t)(SECRET_LEN_MIN + 2 * test_prng_below(SECRET_LEN_STEPS));

    return config;
}

/* Fisher-Yates over the first `pick` positions: a uniformly chosen subset of
 * `count` shard indexes, in a random order. */
static void draw_subset(uint8_t* indexes, uint8_t count, uint8_t pick) {
    for (uint8_t i = 0; i < count; i++) {
        indexes[i] = i;
    }
    for (uint8_t i = 0; i < pick; i++) {
        const uint8_t j = (uint8_t)(i + test_prng_below((uint32_t)(count - i)));
        const uint8_t swap = indexes[i];
        indexes[i] = indexes[j];
        indexes[j] = swap;
    }
}

static void describe_subset(char* out, size_t out_len, const uint8_t* indexes,
                            uint8_t pick) {
    size_t offset = 0;

    out[0] = '\0';
    for (uint8_t i = 0; i < pick && offset + 5 < out_len; i++) {
        offset += (size_t)snprintf(out + offset, out_len - offset, "%s%u",
                                   (i == 0) ? "" : ",", indexes[i]);
    }
}

/* Generates one backup and hands back the serialized shards. Failure here is
 * reported as a failure of the property that called it, with the seed, so that
 * a broken generation is not mistaken for a broken round trip. */
static void generate(const char* property, uint64_t case_seed,
                     const config_t* config, const uint8_t* secret,
                     uint8_t* shards, uint8_t* shard_len) {
    const sskr_group_descriptor_t groups[] = {
        {.threshold = config->threshold, .count = config->count}};

    const int16_t shard_count = sskr_generate_shards(
        1, groups, 1, secret, config->secret_len, shard_len, shards,
        (uint16_t)(config->count *
                   (SSKR_METADATA_LENGTH_BYTES + config->secret_len)),
        test_prng_fill);

    if (shard_count != (int16_t)config->count) {
        fail_msg("%s: seed 0x%016" PRIx64
                 ", %u-of-%u, secret %u bytes: generation "
                 "returned %d, expected %u",
                 property, case_seed, config->threshold, config->count,
                 config->secret_len, shard_count, config->count);
    }
    if (*shard_len != SSKR_METADATA_LENGTH_BYTES + config->secret_len) {
        fail_msg("%s: seed 0x%016" PRIx64
                 ", %u-of-%u, secret %u bytes: shard length %u, expected %u",
                 property, case_seed, config->threshold, config->count,
                 config->secret_len, *shard_len,
                 SSKR_METADATA_LENGTH_BYTES + config->secret_len);
    }
}

/* Property 1: generate then recombine returns exactly the secret, for every
 * valid configuration and every valid secret length. The caller has already
 * seeded the stream from `case_seed`. */
static void round_trip_case(uint64_t case_seed, const config_t* config) {
    uint8_t secret[SECRET_LEN_MAX];
    uint8_t shards[MAX_SHARD_BUFFER];
    uint8_t recovered[SECRET_LEN_MAX];
    const uint8_t* subset[SSS_MAX_SHARE_COUNT];
    uint8_t shard_len = 0;

    test_prng_fill(secret, config->secret_len);
    generate("round trip", case_seed, config, secret, shards, &shard_len);

    for (uint8_t i = 0; i < config->threshold; i++) {
        subset[i] = &shards[i * shard_len];
    }
    memset(recovered, SENTINEL, sizeof(recovered));

    const int16_t recovered_len = sskr_combine_shards(
        subset, shard_len, config->threshold, recovered, sizeof(recovered));

    if (recovered_len != (int16_t)config->secret_len) {
        fail_msg("round trip: seed 0x%016" PRIx64
                 ", %u-of-%u, secret %u bytes: recombination returned %d",
                 case_seed, config->threshold, config->count,
                 config->secret_len, recovered_len);
    }
    if (memcmp(recovered, secret, config->secret_len) != 0) {
        fail_msg("round trip: seed 0x%016" PRIx64
                 ", %u-of-%u, secret %u bytes: recovered secret differs from "
                 "the original",
                 case_seed, config->threshold, config->count,
                 config->secret_len);
    }
}

static void test_round_trip(void** state) {
    (void)state;

    uint64_t case_seed = SEED_ROUND_TRIP;

    /* The bounds first, exhaustively: every extreme (threshold, count) pair
     * against every valid secret length. */
    for (size_t p = 0; p < sizeof(boundary_pairs) / sizeof(boundary_pairs[0]);
         p++) {
        for (uint8_t step = 0; step < SECRET_LEN_STEPS; step++) {
            const config_t config = {
                .threshold = boundary_pairs[p][0],
                .count = boundary_pairs[p][1],
                .secret_len = (uint8_t)(SECRET_LEN_MIN + 2 * step)};
            test_prng_seed(case_seed);
            round_trip_case(case_seed++, &config);
        }
    }

    /* Then the interior, at random. */
    for (unsigned int i = 0; i < RANDOM_ITERATIONS; i++) {
        test_prng_seed(case_seed);
        const config_t config = draw_config(1);
        round_trip_case(case_seed++, &config);
    }
}

/* Property 2: any subset of `threshold` shards recovers the secret, not only
 * the first ones. */
static void test_any_threshold_subset_recovers(void** state) {
    (void)state;

    unsigned int non_prefix_subsets = 0;

    for (unsigned int i = 0; i < RANDOM_ITERATIONS; i++) {
        const uint64_t case_seed = SEED_ANY_SUBSET + i;
        uint8_t secret[SECRET_LEN_MAX];
        uint8_t shards[MAX_SHARD_BUFFER];
        uint8_t recovered[SECRET_LEN_MAX];
        uint8_t indexes[SSS_MAX_SHARE_COUNT];
        const uint8_t* subset[SSS_MAX_SHARE_COUNT];
        char picked[4 * SSS_MAX_SHARE_COUNT];
        uint8_t shard_len = 0;

        test_prng_seed(case_seed);
        const config_t config = draw_config(2);
        test_prng_fill(secret, config.secret_len);
        generate("any subset", case_seed, &config, secret, shards, &shard_len);

        draw_subset(indexes, config.count, config.threshold);
        for (uint8_t k = 0; k < config.threshold; k++) {
            subset[k] = &shards[indexes[k] * shard_len];
            if (indexes[k] != k) {
                non_prefix_subsets++;
            }
        }
        describe_subset(picked, sizeof(picked), indexes, config.threshold);
        memset(recovered, SENTINEL, sizeof(recovered));

        const int16_t recovered_len = sskr_combine_shards(
            subset, shard_len, config.threshold, recovered, sizeof(recovered));

        if (recovered_len != (int16_t)config.secret_len) {
            fail_msg("any subset: seed 0x%016" PRIx64
                     ", %u-of-%u, secret %u bytes, shards {%s}: recombination "
                     "returned %d",
                     case_seed, config.threshold, config.count,
                     config.secret_len, picked, recovered_len);
        }
        if (memcmp(recovered, secret, config.secret_len) != 0) {
            fail_msg("any subset: seed 0x%016" PRIx64
                     ", %u-of-%u, secret %u bytes, shards {%s}: recovered "
                     "secret differs from the original",
                     case_seed, config.threshold, config.count,
                     config.secret_len, picked);
        }
    }

    /* Guards the property itself: if the draw ever degenerated into "the first
     * `threshold` shards", every case above would still pass while testing
     * nothing this file does not already test. */
    assert_true(non_prefix_subsets > 0);
}

/* Property 3: one shard short of the threshold, recombination fails and leaves
 * nothing behind. The buffer is pre-filled with a sentinel, so the assertion
 * distinguishes "wiped" from "never written" -- sskr_combine_shards_internal()
 * memzero()s the caller's buffer on every error path, and that erasure is the
 * observable half of the property. */
static void test_under_threshold_fails_without_partial_result(void** state) {
    (void)state;

    for (unsigned int i = 0; i < RANDOM_ITERATIONS; i++) {
        const uint64_t case_seed = SEED_UNDER_COUNT + i;
        uint8_t secret[SECRET_LEN_MAX];
        uint8_t shards[MAX_SHARD_BUFFER];
        uint8_t recovered[SECRET_LEN_MAX];
        uint8_t indexes[SSS_MAX_SHARE_COUNT];
        const uint8_t* subset[SSS_MAX_SHARE_COUNT];
        char picked[4 * SSS_MAX_SHARE_COUNT];
        uint8_t shard_len = 0;

        test_prng_seed(case_seed);
        const config_t config = draw_config(2);
        test_prng_fill(secret, config.secret_len);
        generate("under threshold", case_seed, &config, secret, shards,
                 &shard_len);

        /* Anywhere from one shard to one short of the threshold. */
        const uint8_t pick =
            (uint8_t)(1 + test_prng_below((uint32_t)(config.threshold - 1)));
        draw_subset(indexes, config.count, pick);
        for (uint8_t k = 0; k < pick; k++) {
            subset[k] = &shards[indexes[k] * shard_len];
        }
        describe_subset(picked, sizeof(picked), indexes, pick);
        memset(recovered, SENTINEL, sizeof(recovered));

        const int16_t recovered_len = sskr_combine_shards(
            subset, shard_len, pick, recovered, sizeof(recovered));

        if (recovered_len >= 0) {
            fail_msg("under threshold: seed 0x%016" PRIx64
                     ", %u-of-%u, secret %u bytes, %u shards {%s}: "
                     "recombination returned %d instead of an error",
                     case_seed, config.threshold, config.count,
                     config.secret_len, pick, picked, recovered_len);
        }
        for (size_t byte = 0; byte < sizeof(recovered); byte++) {
            if (recovered[byte] != 0) {
                fail_msg("under threshold: seed 0x%016" PRIx64
                         ", %u-of-%u, secret %u bytes, %u shards {%s}: output "
                         "buffer not wiped, byte %zu is 0x%02x",
                         case_seed, config.threshold, config.count,
                         config.secret_len, pick, picked, byte,
                         recovered[byte]);
            }
        }
    }
}

/* Property 4, Shamir level: flipping one bit of a shard's value makes the
 * SLIP-39 digest disagree, and recombination refuses rather than handing back a
 * wrong secret. Only the value is touched: corrupting the metadata is a
 * different rejection path, already covered by tests/sskr_combine_invariants.c.
 * A threshold of at least 2 is required -- 1-of-1 shards carry the secret
 * itself and no digest to check it against. */
static void test_single_bit_corruption_is_rejected(void** state) {
    (void)state;

    for (unsigned int i = 0; i < RANDOM_ITERATIONS; i++) {
        const uint64_t case_seed = SEED_ONE_BIT + i;
        uint8_t secret[SECRET_LEN_MAX];
        uint8_t shards[MAX_SHARD_BUFFER];
        uint8_t recovered[SECRET_LEN_MAX];
        uint8_t indexes[SSS_MAX_SHARE_COUNT];
        const uint8_t* subset[SSS_MAX_SHARE_COUNT];
        char picked[4 * SSS_MAX_SHARE_COUNT];
        uint8_t shard_len = 0;

        test_prng_seed(case_seed);
        const config_t config = draw_config(2);
        test_prng_fill(secret, config.secret_len);
        generate("one flipped bit", case_seed, &config, secret, shards,
                 &shard_len);

        draw_subset(indexes, config.count, config.threshold);
        for (uint8_t k = 0; k < config.threshold; k++) {
            subset[k] = &shards[indexes[k] * shard_len];
        }
        describe_subset(picked, sizeof(picked), indexes, config.threshold);

        /* Corrupt a shard that is actually part of the set handed over. */
        const uint8_t victim = indexes[test_prng_below(config.threshold)];
        const uint32_t bit = test_prng_below((uint32_t)config.secret_len * 8);
        uint8_t* value =
            &shards[victim * shard_len + SSKR_METADATA_LENGTH_BYTES];
        value[bit / 8] ^= (uint8_t)(1u << (bit % 8));

        memset(recovered, SENTINEL, sizeof(recovered));

        const int16_t recovered_len = sskr_combine_shards(
            subset, shard_len, config.threshold, recovered, sizeof(recovered));

        if (recovered_len != SSS_ERROR_CHECKSUM_FAILURE) {
            fail_msg("one flipped bit: seed 0x%016" PRIx64
                     ", %u-of-%u, secret %u bytes, shards {%s}, bit %u of "
                     "shard %u: recombination returned %d, expected %d",
                     case_seed, config.threshold, config.count,
                     config.secret_len, picked, bit, victim, recovered_len,
                     SSS_ERROR_CHECKSUM_FAILURE);
        }
        for (size_t byte = 0; byte < sizeof(recovered); byte++) {
            if (recovered[byte] != 0) {
                fail_msg(
                    "one flipped bit: seed 0x%016" PRIx64
                    ", %u-of-%u, secret %u bytes, shards {%s}, bit %u of "
                    "shard %u: output buffer not wiped, byte %zu is 0x%02x",
                    case_seed, config.threshold, config.count,
                    config.secret_len, picked, bit, victim, byte,
                    recovered[byte]);
            }
        }
    }
}

/* Not a property of the application: a property of the generator the three
 * property files depend on. The failure this guards against is the one the
 * suite's cx_rng_no_throw() would produce if it were reused here -- a stream
 * that is the same on every call and independent of the seed, under which every
 * property above would stay green while sweeping a single point. */
static void test_generator_is_not_degenerate(void** state) {
    (void)state;

    uint8_t first[64];
    uint8_t second[64];
    uint8_t other_seed[64];

    test_prng_seed(SEED_ROUND_TRIP);
    test_prng_fill(first, sizeof(first));
    test_prng_fill(second, sizeof(second));

    test_prng_seed(SEED_ROUND_TRIP + 1);
    test_prng_fill(other_seed, sizeof(other_seed));

    /* Successive calls differ: this is what cx_rng_no_throw() fails, since it
     * restarts from zero every time. */
    assert_memory_not_equal(first, second, sizeof(first));

    /* A different seed gives a different stream. */
    assert_memory_not_equal(first, other_seed, sizeof(first));

    /* And the stream is not the 0, 1, 2, ... ramp either. */
    uint8_t ramp[64];
    for (size_t i = 0; i < sizeof(ramp); i++) {
        ramp[i] = (uint8_t)i;
    }
    assert_memory_not_equal(first, ramp, sizeof(first));

    /* Re-seeding reproduces the stream exactly -- the property that makes a
     * red run replayable from the number in its failure message. */
    uint8_t replay[64];
    test_prng_seed(SEED_ROUND_TRIP);
    test_prng_fill(replay, sizeof(replay));
    assert_memory_equal(first, replay, sizeof(first));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_generator_is_not_degenerate),
        cmocka_unit_test(test_round_trip),
        cmocka_unit_test(test_any_threshold_subset_recovers),
        cmocka_unit_test(test_under_threshold_fails_without_partial_result),
        cmocka_unit_test(test_single_bit_corruption_is_rejected),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
