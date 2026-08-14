/*
 * What sskr_generate_shards() does when a randomness draw fails.
 *
 * Share generation consumes randomness at three points: the 16-bit share-set
 * identifier (sskr_generate_shards_internal()), the threshold - 2 leading
 * shares of each split, and the random half of the integrity digest
 * (sss_split_secret()). None of the three used to be checked, and none of them
 * could be: the generator was typed `unsigned char *(*)(uint8_t *, size_t)` and
 * had no way to report a failure, while the BOLOS entry point it was given --
 * cx_rng() -- calls cx_rng_no_throw(), which returns void. A failed draw was
 * therefore invisible from top to bottom.
 *
 * What that produced was worse than a wrong answer. The identifier stayed at
 * its zero initialiser, the coefficients and the digest padding kept whatever
 * the previous stack frame had left in those buffers, and the shares built on
 * them still recombined to the correct secret -- so generation reported
 * success, the user wrote down a backup that works, and the secrecy Shamir is
 * there to provide was gone. Nothing on the device, in the return value or in
 * a trace said so.
 *
 * This file pins the opposite behaviour, one draw at a time. `fail_on_call`
 * walks the whole sequence of draws a 3-of-5 split makes, so each consumer in
 * turn is the one that fails, and every case has to come back:
 *
 *   - a negative error code, not a share count;
 *   - *shard_len at 0, the same contract every other failure path holds
 *     (tests/unit/tests/sskr_generate_error_contract.c);
 *   - an output buffer erased in full, so that no partly-built share survives
 *     a refusal.
 *
 * The generator is a parameter of the public API, so none of this needs a hook
 * into production code -- which is also why the gap could have been closed
 * before now.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "group.h"
#include "sskr-constants.h"
#include "sskr.h"
#include "sss-constants.h"
#include "sss.h"

#define SECRET_LEN    16
#define MEMBER_THRESHOLD 3
#define MEMBER_COUNT     5

// A 3-of-5 split over a 16-byte secret draws: 1 (identifier) + 1 (the single
// leading share of the group split, threshold 1 -> none) ... rather than count
// them by hand, sweep past the largest number any of these calls can make. A
// draw index beyond the last one simply never fires, and the success case at
// the end of the sweep is asserted separately below.
#define DRAW_SWEEP 12

static const uint8_t master_secret[SECRET_LEN] = {0x7d, 0xaa, 0x85, 0x12, 0x51, 0x00,
                                                  0x28, 0x74, 0xe1, 0xa1, 0x99, 0x5f,
                                                  0x08, 0x97, 0xe6, 0xb1};

static unsigned fail_on_call;
static unsigned call_count;

// Succeeds with the suite's usual 0,1,2,... pattern except on the draw named by
// `fail_on_call`, where it reports failure without writing anything -- which is
// what a real failed draw leaves behind: the buffer's previous contents.
static bool flaky_rng(uint8_t *buffer, size_t len) {
    call_count++;
    if (call_count == fail_on_call) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buffer[i] = (uint8_t) i;
    }
    return true;
}

static void test_rng_failure_is_fail_closed(void **state) {
    (void) state;
    sskr_group_descriptor_t groups[1] = {{MEMBER_THRESHOLD, MEMBER_COUNT}};
    unsigned failures_seen = 0;

    for (unsigned draw = 1; draw <= DRAW_SWEEP; draw++) {
        uint8_t output[(SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * MEMBER_COUNT];
        uint8_t shard_len = 0xFF;

        // Poisoned, so that "erased" is distinguishable from "never written".
        memset(output, 0xEE, sizeof(output));
        fail_on_call = draw;
        call_count = 0;

        int16_t result =
            sskr_generate_shards(1, groups, 1, master_secret, SECRET_LEN, &shard_len,
                                 output, sizeof(output), flaky_rng);

        if (call_count < draw) {
            // That draw does not exist: this split made fewer. Nothing failed,
            // so generation must have succeeded.
            assert_int_equal(result, MEMBER_COUNT);
            continue;
        }

        failures_seen++;
        assert_true(result < 0);
        assert_int_equal(shard_len, 0);
        for (size_t i = 0; i < sizeof(output); i++) {
            assert_int_equal(output[i], 0);
        }
    }

    // The sweep has to have exercised the failure path, or the assertions above
    // are vacuous and this test would stay green against the original defect.
    assert_true(failures_seen > 0);
}

// The identifier is the first draw, and the one whose failure used to be
// invisible in the most direct way: it left the field at 0x0000 in every shard
// of the set, which is also what a second failed backup would produce -- so two
// unrelated sets would have looked like one.
static void test_identifier_draw_failure_refuses(void **state) {
    (void) state;
    sskr_group_descriptor_t groups[1] = {{MEMBER_THRESHOLD, MEMBER_COUNT}};
    uint8_t output[(SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * MEMBER_COUNT];
    uint8_t shard_len = 0xFF;

    memset(output, 0xEE, sizeof(output));
    fail_on_call = 1;
    call_count = 0;

    int16_t result = sskr_generate_shards(1, groups, 1, master_secret, SECRET_LEN,
                                          &shard_len, output, sizeof(output), flaky_rng);

    assert_int_equal(result, SSKR_ERROR_RNG_FAILURE);
    assert_int_equal(shard_len, 0);
    assert_int_equal(call_count, 1);  // stops at the first failed draw
}

// sss_split_secret() reports its own code, from the disjoint SSS_* range, and
// sskr_generate_shards() has to propagate it rather than collapse it -- the
// property tests/unit/tests/sskr_generate_error_contract.c holds for the other
// families.
static void test_split_draw_failure_propagates_sss_code(void **state) {
    (void) state;
    uint8_t result_buffer[SECRET_LEN * MEMBER_COUNT];
    uint8_t poisoned[SECRET_LEN * MEMBER_COUNT];

    memset(result_buffer, 0xEE, sizeof(result_buffer));
    memset(poisoned, 0, sizeof(poisoned));

    // Fail the first draw sss_split_secret() itself makes.
    fail_on_call = 1;
    call_count = 0;

    int16_t result = sss_split_secret(MEMBER_THRESHOLD, MEMBER_COUNT, master_secret,
                                      SECRET_LEN, result_buffer, flaky_rng);

    assert_int_equal(result, SSS_ERROR_RNG_FAILURE);
    assert_memory_equal(result_buffer, poisoned, sizeof(result_buffer));
}

// The control: the same generator, never failing, still produces a full set.
// Without this, every assertion above would also pass against a function that
// refused unconditionally.
static void test_working_rng_still_generates(void **state) {
    (void) state;
    sskr_group_descriptor_t groups[1] = {{MEMBER_THRESHOLD, MEMBER_COUNT}};
    uint8_t output[(SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * MEMBER_COUNT];
    uint8_t shard_len = 0;

    fail_on_call = 0;  // never fires
    call_count = 0;

    int16_t result = sskr_generate_shards(1, groups, 1, master_secret, SECRET_LEN,
                                          &shard_len, output, sizeof(output), flaky_rng);

    assert_int_equal(result, MEMBER_COUNT);
    assert_int_equal(shard_len, SECRET_LEN + SSKR_METADATA_LENGTH_BYTES);
    assert_true(call_count > 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rng_failure_is_fail_closed),
        cmocka_unit_test(test_identifier_draw_failure_refuses),
        cmocka_unit_test(test_split_draw_failure_propagates_sss_code),
        cmocka_unit_test(test_working_rng_still_generates),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
