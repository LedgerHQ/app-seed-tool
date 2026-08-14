/*
 * Randomised properties of the SSKR wire form: one flipped bit is always
 * rejected, wherever it lands.
 *
 * tests/sskr_hex_check_guards.c already holds bolos_ux_sskr_hex_check() against
 * two hand-picked corruptions of one frozen share -- one byte of the CRC, one
 * byte of the payload -- which is what proves the CRC is both compared and
 * recomputed. This sweeps the same guard over the whole domain instead: every
 * single bit of every byte of a share set, across both CBOR length encodings,
 * several share counts and many different secrets.
 *
 * The frames are not assembled here. They come out of
 * bolos_ux_bip39_to_sskr_convert(), which is what the device runs, and are
 * turned back into bytes with bolos_ux_sskr_byteword_to_hex(), which is what
 * the share entry screens run. So the accepting half of the property is not
 * circular: nothing in this file knows how a CBOR header is laid out or how the
 * CRC-32 is placed. The property is self-checking in the same sense as the
 * others -- production code encodes, production code decodes, and the test only
 * corrupts what sits in between.
 *
 * Two properties:
 *
 *   1. A share set that has been through the encoder and back is accepted.
 *      Without this the second property would be satisfied by a function that
 *      rejects everything.
 *   2. Flipping any single bit of the byte form makes it rejected. Every byte
 *      is covered either by the CRC-32 (the leading bytes) or is the CRC-32
 *      itself, so there is no position where a flip may legitimately pass.
 *
 * The boundary configurations below are swept exhaustively -- all 9568 bit
 * positions of the five frames -- since restoring a frame is a memcpy and the
 * expensive half (generating the shares) happens once per configuration.
 * Random configurations then vary the secret and the share layout.
 *
 * A note on randomness: the shard values inside these frames come from the
 * suite's cx_rng_no_throw(), because bolos_ux_sskr_generate() calls cx_rng
 * directly rather than taking a generator as a parameter. That is not a gap
 * here -- what varies across cases is the secret, drawn from the seeded
 * generator in tests/unit/lib/testprng.h, and what is under test is the framing
 * and its checksum, not the secret sharing underneath. The Shamir layer is
 * swept with a real generator in tests/sskr_properties.c.
 */

#include <cmocka.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "testprng.h"
#include "bip39/common_bip39.h"
#include "sskr/common_sskr.h"
// clang-format on

#define SEED_EXHAUSTIVE UINT64_C(0x53B1A9C304D71E70)
#define SEED_RANDOM UINT64_C(0x53B1A9C304D71E71)

#define RANDOM_ITERATIONS 100

#define MAX_SHARES SSS_MAX_SHARE_COUNT
#define MAX_WIRE_BYTES SSKR_SHARE_MAX_WIRE_LENGTH
#define MAX_WORDS_PER_SHARE (MAX_WIRE_BYTES * (SSKR_BYTEWORD_LENGTH + 1))
#define MAX_MNEMONIC_LEN (24 * 9)

typedef struct {
    uint8_t threshold;
    uint8_t count;
    unsigned int words; /* BIP-39 word count: 12, 18 or 24 */
} config_t;

/* Both CBOR length encodings and both ends of the share-count range. A shard is
 * SSKR_METADATA_LENGTH_BYTES + the secret, so 12 words give a 21-byte shard
 * (short-form byte string header) while 18 and 24 words give 29 and 37
 * (long-form header, one extra length byte). */
static const config_t boundary_configs[] = {
    {1, 1, 12}, {2, 3, 12}, {2, 3, 18}, {3, 5, 24}, {2, MAX_SHARES, 24},
};

/* Runs one secret through the production encoder and hands back the byte form
 * the share entry screens reconstruct from typed ByteWords. Returns the length
 * of one share in bytes. */
static unsigned int encode_share_set(uint64_t case_seed, const config_t* config,
                                     uint8_t* hex, uint8_t* share_count) {
    uint8_t secret[SSKR_MAX_STRENGTH_BYTES];
    unsigned char mnemonic[MAX_MNEMONIC_LEN];
    unsigned char words[MAX_SHARES * MAX_WORDS_PER_SHARE];
    unsigned int group_descriptor[2];
    unsigned int words_len = 0;

    const uint8_t seed_len = (uint8_t)(config->words * 4 / 3);

    test_prng_fill(secret, seed_len);

    const unsigned int mnemonic_len = bolos_ux_bip39_mnemonic_encode(
        secret, seed_len, mnemonic, sizeof(mnemonic));
    if (mnemonic_len == 0) {
        fail_msg("wire form: seed 0x%016" PRIx64
                 ", %u-of-%u, %u words: BIP-39 encoding failed",
                 case_seed, config->threshold, config->count, config->words);
    }

    group_descriptor[0] = config->threshold;
    group_descriptor[1] = config->count;
    *share_count = 0;

    bolos_ux_bip39_to_sskr_convert(mnemonic, mnemonic_len, config->words,
                                   group_descriptor, share_count, words,
                                   &words_len);

    if (*share_count != config->count || words_len == 0) {
        fail_msg("wire form: seed 0x%016" PRIx64
                 ", %u-of-%u, %u words: SSKR conversion produced %u shares "
                 "(%u ByteWord characters)",
                 case_seed, config->threshold, config->count, config->words,
                 *share_count, words_len);
    }

    /* Space-separated ByteWords: one share is n * (length + 1) - 1 characters
     * long, so n words. */
    const unsigned int chars_per_share = words_len / *share_count;
    const unsigned int bytes_per_share =
        (chars_per_share + 1) / (SSKR_BYTEWORD_LENGTH + 1);

    for (unsigned int share = 0; share < *share_count; share++) {
        for (unsigned int word = 0; word < bytes_per_share; word++) {
            const unsigned char* byteword =
                &words[share * chars_per_share +
                       word * (SSKR_BYTEWORD_LENGTH + 1)];
            if (!bolos_ux_sskr_byteword_to_hex(
                    byteword, &hex[share * bytes_per_share + word])) {
                fail_msg("wire form: seed 0x%016" PRIx64
                         ", %u-of-%u, %u words: share %u word %u is not in the "
                         "ByteWords list",
                         case_seed, config->threshold, config->count,
                         config->words, share, word);
            }
        }
    }

    return bytes_per_share;
}

/* Property: the frame as produced is accepted, and flipping bit `bit` of it is
 * not. bolos_ux_sskr_hex_check() wipes what it rejects, so each attempt works
 * on a fresh copy. */
static void check_bit_is_rejected(uint64_t case_seed, const config_t* config,
                                  const uint8_t* pristine, unsigned int total,
                                  uint8_t share_count, unsigned int bit) {
    uint8_t corrupted[MAX_SHARES * MAX_WIRE_BYTES];

    memcpy(corrupted, pristine, total);
    corrupted[bit / 8] ^= (uint8_t)(1u << (bit % 8));

    if (bolos_ux_sskr_hex_check(corrupted, total, share_count) != 0) {
        fail_msg("wire form: seed 0x%016" PRIx64
                 ", %u-of-%u, %u words: bit %u (byte %u) flipped and the share "
                 "set was still accepted",
                 case_seed, config->threshold, config->count, config->words,
                 bit, bit / 8);
    }
}

static void run_case(uint64_t case_seed, const config_t* config,
                     bool exhaustive) {
    uint8_t pristine[MAX_SHARES * MAX_WIRE_BYTES];
    uint8_t accepted[MAX_SHARES * MAX_WIRE_BYTES];
    uint8_t share_count = 0;

    const unsigned int bytes_per_share =
        encode_share_set(case_seed, config, pristine, &share_count);
    const unsigned int total = bytes_per_share * share_count;

    /* Control: an encoder output must be accepted, otherwise every rejection
     * below is vacuous. */
    memcpy(accepted, pristine, total);
    if (bolos_ux_sskr_hex_check(accepted, total, share_count) != 1) {
        fail_msg("wire form: seed 0x%016" PRIx64
                 ", %u-of-%u, %u words: an unmodified share set was rejected",
                 case_seed, config->threshold, config->count, config->words);
    }

    if (exhaustive) {
        for (unsigned int bit = 0; bit < total * 8; bit++) {
            check_bit_is_rejected(case_seed, config, pristine, total,
                                  share_count, bit);
        }
    } else {
        check_bit_is_rejected(case_seed, config, pristine, total, share_count,
                              test_prng_below(total * 8));
    }
}

/* Every bit position of five representative share sets. */
static void test_every_single_bit_flip_is_rejected(void** state) {
    (void)state;

    for (size_t i = 0;
         i < sizeof(boundary_configs) / sizeof(boundary_configs[0]); i++) {
        test_prng_seed(SEED_EXHAUSTIVE + i);
        run_case(SEED_EXHAUSTIVE + i, &boundary_configs[i], true);
    }
}

/* And one bit position of many different share sets, so the property is not
 * pinned to five secrets. */
static void test_random_share_sets_reject_a_flipped_bit(void** state) {
    (void)state;

    for (unsigned int i = 0; i < RANDOM_ITERATIONS; i++) {
        const uint64_t case_seed = SEED_RANDOM + i;
        static const unsigned int word_counts[] = {12, 18, 24};
        config_t config;

        test_prng_seed(case_seed);
        config.count = (uint8_t)(1 + test_prng_below(MAX_SHARES));
        config.threshold =
            (config.count == 1)
                ? 1
                : (uint8_t)(2 + test_prng_below((uint32_t)config.count - 1));
        config.words = word_counts[test_prng_below(3)];

        run_case(case_seed, &config, false);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_every_single_bit_flip_is_rejected),
        cmocka_unit_test(test_random_share_sets_reject_a_flipped_bit),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
