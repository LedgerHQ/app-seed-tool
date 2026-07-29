/*
 * Regression test for the un-erased output buffer on the failure path of
 * sskr_combine_shards_internal() (src/common/sskr/sskr.c).
 *
 * The generation side already gets this right: sskr_generate_shards() does
 * `memzero(output, buffer_size); return 0;` before returning on any error.
 * The combination side's internal function cleans up its own working
 * buffers (group_shares/gx/gy/groups) on every failure, but never the
 * caller's `buffer` -- the very thing a failed combine is supposed to leave
 * clean, in case the caller reuses the same buffer across attempts.
 *
 * Forcing a genuine interpolation failure without touching the crypto math
 * itself: interpolate()'s first action is cx_bn_lock(), which fails
 * immediately if a BN context is already locked (same technique as
 * sss_recover_erase_length.c). A single group in this port
 * (SSKR_MAX_GROUP_COUNT == 1) always has group_threshold == 1, which makes
 * the outer, group-of-groups sss_recover_secret() call take the
 * threshold-1 shortcut (plain copy, no interpolate()) -- so the failure
 * has to come from the per-group *member* recovery call instead, which
 * needs member_threshold >= 2 to reach interpolate() at all. Two shards of
 * a single 2-of-2 group are enough.
 *
 * buffer_len (32, deliberately larger than the 16-byte secret) rather than
 * secret_len is what must be erased: the real caller,
 * bolos_ux_sskr_combine(), always passes SSKR_MAX_STRENGTH_BYTES (32) as
 * buffer_len while secret_len can be as low as SSKR_MIN_STRENGTH_BYTES
 * (16), so erasing only secret_len would leave up to 16 bytes of whatever
 * the buffer held before this call.
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

#define SHARD_VALUE_LEN (SSKR_MIN_STRENGTH_BYTES)
#define SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SHARD_VALUE_LEN)
#define BUFFER_LEN (SSKR_MAX_STRENGTH_BYTES)

/* One shard of a single 2-of-2 group: group_threshold 1, group_count 1,
 * group_index 0, member_threshold 2. member_index is the only field that
 * differs between the two shards built for this test. */
static void build_shard(uint8_t raw[SHARD_LEN], uint8_t member_index) {
    raw[0] = 0xAB;
    raw[1] = 0xCD;
    raw[2] = 0x00;  // (group_threshold - 1) << 4 | (group_count - 1) -> 1-of-1
    raw[3] = (uint8_t)(0x00 |
                       ((2 - 1) & 0x0F));  // group_index 0, member_threshold 2
    raw[4] = member_index;                 // reserved nibble stays zero
    memset(&raw[SSKR_METADATA_LENGTH_BYTES], 0x42 + member_index,
           SHARD_VALUE_LEN);
}

static void test_combine_erases_buffer_on_interpolation_failure(void** state) {
    (void)state;

    uint8_t raw_a[SHARD_LEN];
    uint8_t raw_b[SHARD_LEN];
    build_shard(raw_a, 0);
    build_shard(raw_b, 1);
    const uint8_t* shards[2] = {raw_a, raw_b};

    uint8_t buffer[BUFFER_LEN];
    memset(buffer, 0xFF, sizeof(buffer));

    /* Force interpolate()'s own cx_bn_lock() to fail: it refuses to lock an
     * already-locked context. Its cleanup path unlocks whatever is locked
     * when it bails out, so this releases the lock again on the way out --
     * no matching unlock needed here. */
    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result =
        sskr_combine_shards(shards, SHARD_LEN, 2, buffer, sizeof(buffer));

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);

    uint8_t expected[BUFFER_LEN];
    memset(expected, 0x00, sizeof(expected));
    assert_memory_equal(buffer, expected, sizeof(buffer));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_erases_buffer_on_interpolation_failure),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
