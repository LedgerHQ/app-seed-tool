/*
 * Coverage for the output-buffer erasure sskr_generate_shards() already
 * performs when sss_split_secret() fails inside
 * sskr_generate_shards_internal() (src/common/sskr/sskr.c):
 *
 *     int16_t split_error = sss_split_secret(...);
 *     if (split_error < 0) {
 *         memzero(member_shares/group_shares, ...);
 *         return split_error;
 *     }
 *     ...
 *     if (error) { memzero(output, buffer_size); *shard_len = 0;
 *                  return error; }
 *
 * Both tests below also pin the code that comes back out, which is the
 * point of reaching this cleanup through two genuinely different failures:
 * the two triggers are told apart by SSS_ERROR_INTERPOLATION_FAILURE versus
 * SSS_ERROR_INVALID_THRESHOLD.
 *
 * test_sskr_generate_split_error.c already covers this for a
 * SSS_ERROR_TOO_MANY_SHARES failure (member-level split, an oversized
 * group). This file adds two triggers that reach the same cleanup through
 * a genuinely different failure inside sss_split_secret():
 *
 *   - member level: a real Shamir interpolation failure, forced the same
 *     way as sss_recover_erase_length.c and the fix-2 combine test --
 *     interpolate()'s own cx_bn_lock() fails immediately if a BN context
 *     is already locked.
 *   - group level: SSKR_MAX_GROUP_COUNT == 1 in this port, so the group
 *     split's share_count is always 1, and sskr_count_shards() only
 *     rejects group_threshold > groups_len -- group_threshold == 0 slips
 *     through both that check and sskr_generate_shards_internal()'s own
 *     `group_threshold > groups_len` guard (0 is not greater than
 *     anything), reaching sss_split_secret(0, 1, ...), whose parameter
 *     validation rejects threshold 0 outright (SSS_ERROR_INVALID_THRESHOLD).
 *     This is the only way to make the *group*-level split fail in this
 *     port: with groups_len fixed at 1, group_threshold <= groups_len
 *     forces group_threshold to 1 for any threshold >= 1, which takes
 *     sss_split_secret()'s threshold-1 shortcut (a plain copy) and never
 *     reaches interpolate() at all, so cx_bn_lock() cannot force a failure
 *     there. group_threshold == 0 is not reachable through the UI (the
 *     only real caller, bolos_ux_bip39_to_sskr_convert(), hard-codes 1)
 *     and sskr_count_shards() does not reject it either -- a minor,
 *     already-benign validation gap (sss_split_secret() itself refuses it
 *     safely), used here only as a trigger, not something this file fixes.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sskr-constants.h"
#include "sskr.h"
#include "sss-constants.h"
#include "testutils.h"

#define SECRET_LEN (SSKR_MIN_STRENGTH_BYTES)
#define BUFFER_LEN (64)

static void test_generate_erases_output_on_member_level_interpolation_failure(
    void** state) {
    (void)state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 2}};
    uint8_t share_buffer[BUFFER_LEN];
    uint8_t share_len = 0xFF;

    memset(share_buffer, 0xFF, sizeof(share_buffer));

    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result = sskr_generate_shards(1, groups, 1, master_secret,
                                          SECRET_LEN, &share_len, share_buffer,
                                          sizeof(share_buffer), cx_rng);

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);
    assert_int_equal(share_len, 0);

    uint8_t expected[BUFFER_LEN];
    memset(expected, 0x00, sizeof(expected));
    assert_memory_equal(share_buffer, expected, sizeof(share_buffer));
}

static void test_generate_erases_output_on_group_level_split_failure(
    void** state) {
    (void)state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {{.threshold = 1, .count = 1}};
    uint8_t share_buffer[BUFFER_LEN];
    uint8_t share_len = 0xFF;

    memset(share_buffer, 0xFF, sizeof(share_buffer));

    int16_t result = sskr_generate_shards(0, groups, 1, master_secret,
                                          SECRET_LEN, &share_len, share_buffer,
                                          sizeof(share_buffer), cx_rng);

    assert_int_equal(result, SSS_ERROR_INVALID_THRESHOLD);
    assert_int_equal(share_len, 0);

    uint8_t expected[BUFFER_LEN];
    memset(expected, 0x00, sizeof(expected));
    assert_memory_equal(share_buffer, expected, sizeof(share_buffer));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_generate_erases_output_on_member_level_interpolation_failure),
        cmocka_unit_test(
            test_generate_erases_output_on_group_level_split_failure),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
