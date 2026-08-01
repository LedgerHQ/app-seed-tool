/*
 * Coverage for what sskr.h says sskr_generate_shards() returns:
 *
 *     @return Number of shards generated on success, or a negative error
 *             code on failure.
 *
 * The other test files reach this function to cover a particular guard, a
 * particular buffer erasure or a particular round trip, and each asserts the
 * one code its own case produces. Nothing held the shape of the contract
 * itself: that a caller can tell *why* a generation failed, and that both
 * outputs -- the return value and *shard_len -- are defined on every path
 * out.
 *
 * That is what this file pins, in three parts:
 *
 *   - one representative per error family, each with the exact code it must
 *     produce, gathered in a single table so the families are read side by
 *     side rather than one per file;
 *   - the codes are all distinct, checked pairwise rather than asserted in
 *     prose -- a table whose entries silently collapsed onto one value would
 *     otherwise still pass every individual assertion above;
 *   - *shard_len is 0 after every one of them, and the real shard length
 *     after a successful call.
 *
 * The two families at the bottom of the table are the ones that used to be
 * unreachable through the return value: they surface from sss_split_secret()
 * inside sskr_generate_shards_internal(), and were collapsed into a plain 0
 * on the way out. SSS_* and SSKR_* codes come from two disjoint ranges
 * (-101.. and -1..), so propagating one through an int16_t neither truncates
 * it nor makes it collide with the other family's meanings.
 */

#include <cmocka.h>
#include <cx.h>
#include <lcx_rng.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sskr-constants.h"
#include "sskr.h"
#include "sss-constants.h"
#include "testutils.h"

#define SECRET_LEN (SSKR_MIN_STRENGTH_BYTES)
#define SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SECRET_LEN)

/* One shard more than sss_split_secret() can produce, so that the
 * too-many-shares case clears the insufficient-space check and fails in the
 * split instead. Sized symbolically: SSS_MAX_SHARE_COUNT is smaller on
 * TARGET_NANOS. */
#define OVERSIZED_COUNT (SSS_MAX_SHARE_COUNT + 1)
#define BUFFER_LEN (SHARD_LEN * OVERSIZED_COUNT)

/* A value no successful call could leave behind, so that "shard_len is 0"
 * means it was written rather than merely never touched. */
#define SHARD_LEN_SENTINEL (0xFF)

/* One descriptor is enough for every row: the only one that passes a
 * groups_len above 1 is the group-threshold rejection, and sskr_count_shards()
 * returns on that before it reads any element. */
typedef struct {
    const char* name;
    uint8_t group_threshold;
    uint8_t groups_len;
    sskr_group_descriptor_t group;
    uint16_t master_secret_len;
    uint16_t buffer_size;
    /* Locks the BN context before the call, which makes interpolate() fail
     * on its own cx_bn_lock(). */
    bool lock_bn;
    int16_t expected;
} error_case_t;

static const error_case_t error_cases[] = {
    {"secret shorter than the minimum",
     1,
     1,
     {.threshold = 2, .count = 3},
     SSKR_MIN_STRENGTH_BYTES - 2,
     BUFFER_LEN,
     false,
     SSKR_ERROR_SECRET_TOO_SHORT},
    {"secret longer than the maximum",
     1,
     1,
     {.threshold = 2, .count = 3},
     SSKR_MAX_STRENGTH_BYTES + 2,
     BUFFER_LEN,
     false,
     SSKR_ERROR_SECRET_TOO_LONG},
    {"secret of odd length",
     1,
     1,
     {.threshold = 2, .count = 3},
     SSKR_MIN_STRENGTH_BYTES + 1,
     BUFFER_LEN,
     false,
     SSKR_ERROR_SECRET_LENGTH_NOT_EVEN},
    {"no group at all",
     1,
     0,
     {.threshold = 2, .count = 3},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSKR_ERROR_INVALID_GROUP_LENGTH},
    {"group threshold above the group count",
     SSKR_MAX_GROUP_COUNT + 1,
     SSKR_MAX_GROUP_COUNT,
     {.threshold = 2, .count = 3},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSKR_ERROR_INVALID_GROUP_THRESHOLD},
    {"empty group",
     1,
     1,
     {.threshold = 1, .count = 0},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSKR_ERROR_INVALID_GROUP_COUNT},
    {"member threshold above the member count",
     1,
     1,
     {.threshold = 3, .count = 2},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSKR_ERROR_INVALID_MEMBER_THRESHOLD},
    {"1-of-n group",
     1,
     1,
     {.threshold = 1, .count = 2},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSKR_ERROR_INVALID_SINGLETON_MEMBER},
    {"output buffer one byte short",
     1,
     1,
     {.threshold = 2, .count = 3},
     SECRET_LEN,
     (SHARD_LEN * 3) - 1,
     false,
     SSKR_ERROR_INSUFFICIENT_SPACE},
    {"more members than the split can produce",
     1,
     1,
     {.threshold = 2, .count = OVERSIZED_COUNT},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSS_ERROR_TOO_MANY_SHARES},
    {"group threshold of zero",
     0,
     1,
     {.threshold = 1, .count = 1},
     SECRET_LEN,
     BUFFER_LEN,
     false,
     SSS_ERROR_INVALID_THRESHOLD},
    {"member split that cannot interpolate",
     1,
     1,
     {.threshold = 2, .count = 2},
     SECRET_LEN,
     BUFFER_LEN,
     true,
     SSS_ERROR_INTERPOLATION_FAILURE},
};

#define ERROR_CASE_COUNT (sizeof(error_cases) / sizeof(error_cases[0]))

/* Runs one row of the table and returns what came back, so the two tests
 * below can assert different things about the same call. */
static int16_t run_error_case(const error_case_t* c, uint8_t* shard_len_out) {
    uint8_t master_secret[SSKR_MAX_STRENGTH_BYTES + 2];
    uint8_t output[BUFFER_LEN];
    uint8_t shard_len = SHARD_LEN_SENTINEL;

    memset(master_secret, 0x5A, sizeof(master_secret));
    memset(output, 0xFF, sizeof(output));

    if (c->lock_bn) {
        assert_int_equal(cx_bn_lock(1, 0), CX_OK);
    }

    int16_t result = sskr_generate_shards(
        c->group_threshold, &c->group, c->groups_len, master_secret,
        c->master_secret_len, &shard_len, output, c->buffer_size, cx_rng);

    if (c->lock_bn) {
        cx_bn_unlock();
    }

    *shard_len_out = shard_len;
    return result;
}

static void test_every_family_returns_its_own_error_code(void** state) {
    (void)state;

    for (size_t i = 0; i < ERROR_CASE_COUNT; ++i) {
        uint8_t shard_len = SHARD_LEN_SENTINEL;
        int16_t result = run_error_case(&error_cases[i], &shard_len);

        /* Names the failing row, so a red run says which family broke. */
        if (result != error_cases[i].expected) {
            fail_msg("%s: expected %d, got %d", error_cases[i].name,
                     error_cases[i].expected, result);
        }
        assert_true(result < 0);
    }
}

static void test_error_codes_are_distinct(void** state) {
    (void)state;

    for (size_t i = 0; i < ERROR_CASE_COUNT; ++i) {
        for (size_t j = i + 1; j < ERROR_CASE_COUNT; ++j) {
            if (error_cases[i].expected == error_cases[j].expected) {
                fail_msg("%s and %s both expect %d", error_cases[i].name,
                         error_cases[j].name, error_cases[i].expected);
            }
        }
    }
}

static void test_shard_len_is_zeroed_on_every_error(void** state) {
    (void)state;

    for (size_t i = 0; i < ERROR_CASE_COUNT; ++i) {
        uint8_t shard_len = SHARD_LEN_SENTINEL;
        (void)run_error_case(&error_cases[i], &shard_len);

        if (shard_len != 0) {
            fail_msg("%s: shard_len left at %u", error_cases[i].name,
                     shard_len);
        }
    }
}

static void test_success_returns_the_shard_count_and_length(void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 3}};
    uint8_t master_secret[SECRET_LEN];
    uint8_t output[SHARD_LEN * 3];
    uint8_t shard_len = SHARD_LEN_SENTINEL;

    memset(master_secret, 0x5A, sizeof(master_secret));
    memset(output, 0x00, sizeof(output));

    int16_t result =
        sskr_generate_shards(1, groups, 1, master_secret, sizeof(master_secret),
                             &shard_len, output, sizeof(output), cx_rng);

    assert_int_equal(result, 3);
    assert_int_equal(shard_len, SHARD_LEN);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_every_family_returns_its_own_error_code),
        cmocka_unit_test(test_error_codes_are_distinct),
        cmocka_unit_test(test_shard_len_is_zeroed_on_every_error),
        cmocka_unit_test(test_success_returns_the_shard_count_and_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
