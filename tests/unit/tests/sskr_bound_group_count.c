/*
 * Regression test for the missing upper bound on `groups_len` in
 * sskr_count_shards() (src/common/sskr/sskr.c).
 *
 * Every buffer in this file that is sized from a group count is dimensioned
 * on SSKR_MAX_GROUP_COUNT (1 in this port):
 *
 *     uint8_t group_shares[SSS_MAX_SECRET_SIZE * SSKR_MAX_GROUP_COUNT];
 *     sskr_shard_t shards[SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT];
 *
 * but sskr_count_shards(), the one validator every generation entry point
 * runs first, only rejected `groups_len < 1`. Nothing checked the other end.
 * sskr_generate_shards_internal() then passes groups_len straight to
 * sss_split_secret() as its share_count, which writes groups_len shares of
 * master_secret_len bytes each into a 32-byte group_shares -- 64 bytes for
 * two groups at maximum strength.
 *
 * Not reachable from the application: bolos_ux_sskr_size_get() and
 * bolos_ux_sskr_generate() (seed_sskr.c) already reject groups_len above
 * SSKR_MAX_GROUP_COUNT before filling their own groups[] array (see
 * sskr_generate_bound_groups.c), and bolos_ux_bip39_to_sskr_convert()
 * hard-codes 1. sskr_generate_shards() and sskr_count_shards() are public
 * entry points in sskr.h nonetheless, with the same standing as the other
 * defensive bounds this file's callees already hold, and the check now sits
 * with the rest of the group validation instead of one layer up.
 *
 * Verification technique: interpolate() reaches past group_shares with
 * ordinary loads and stores, not a memzero()/explicit_bzero() call, so --
 * unlike the erase in sss_recover_erase_length.c, which this toolchain's
 * AddressSanitizer does not intercept -- ASan's stack-buffer-overflow
 * instrumentation does apply. Confirmed by running this file against the
 * unbounded version: it aborts with
 *
 *     stack-buffer-overflow ... interpolate ... sss_split_secret ...
 *     sskr_generate_shards_internal
 *     [64, 96) 'group_shares' <== Memory access at offset 96 overflows
 *
 * sskr.c, sss.c and interpolate.c are compiled directly into this target
 * (rather than linked from libsskr/libsss) so ASan instruments their stack
 * frames, matching test_sskr_combine_bounds and
 * test_sss_recover_erase_length.
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

#define SECRET_LEN (SSKR_MAX_STRENGTH_BYTES)
#define BUFFER_LEN (1024)

static void test_count_shards_rejects_more_groups_than_this_build_holds(
    void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 2},
                                              {.threshold = 2, .count = 2}};

    int16_t result = sskr_count_shards(1, groups, SSKR_MAX_GROUP_COUNT + 1);

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_LENGTH);
}

/* The overflowing path: without the bound, sss_split_secret() writes
 * groups_len shares into a group_shares sized for SSKR_MAX_GROUP_COUNT of
 * them. Maximum strength is used so the two shares need 64 bytes of a
 * 32-byte buffer rather than exactly filling it. */
static void test_generate_shards_rejects_more_groups_than_it_can_hold(
    void** state) {
    (void)state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 2},
                                              {.threshold = 2, .count = 2}};
    uint8_t output[BUFFER_LEN];
    uint8_t shard_len = 0;

    memset(output, 0xFF, sizeof(output));

    int16_t result = sskr_generate_shards(
        SSKR_MAX_GROUP_COUNT + 1, groups, SSKR_MAX_GROUP_COUNT + 1,
        master_secret, SECRET_LEN, &shard_len, output, sizeof(output), cx_rng);

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_LENGTH);
}

/* Boundary guard, so the bound cannot drift into rejecting the only group
 * count this build actually supports. */
static void test_generate_shards_still_accepts_the_supported_group_count(
    void** state) {
    (void)state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 2}};
    uint8_t output[BUFFER_LEN];
    uint8_t shard_len = 0;

    memset(output, 0xFF, sizeof(output));

    int16_t result = sskr_generate_shards(1, groups, SSKR_MAX_GROUP_COUNT,
                                          master_secret, SECRET_LEN, &shard_len,
                                          output, sizeof(output), cx_rng);

    assert_int_equal(result, 2);
    assert_int_equal(shard_len, SSKR_METADATA_LENGTH_BYTES + SECRET_LEN);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_count_shards_rejects_more_groups_than_this_build_holds),
        cmocka_unit_test(
            test_generate_shards_rejects_more_groups_than_it_can_hold),
        cmocka_unit_test(
            test_generate_shards_still_accepts_the_supported_group_count),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
