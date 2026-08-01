/*
 * Regression test for the ignored sss_split_secret() return value in
 * sskr_generate_shards_internal().
 *
 * The per-group call at sskr.c's `sss_split_secret(groups[i].threshold,
 * groups[i].count, ...)` can fail -- SSS_ERROR_TOO_MANY_SHARES if
 * groups[i].count exceeds SSS_MAX_SHARE_COUNT, which nothing checks before
 * this call -- but the return value is discarded. The write loop that
 * follows then reads groups[i].count shares out of a member_shares buffer
 * that sss_split_secret() never touched (it failed before writing anything),
 * and writes that many shards into the caller's fixed-size array, one
 * element past its end at this test's count. Before the fix, this crashes
 * (verified: SIGSEGV, cmocka reports it as a failed test).
 *
 * sskr_generate_shards() -- the public entry point this test calls, since
 * sskr_generate_shards_internal() is static -- propagates the SSS_* code
 * surfacing from the internal call, so what this test asserts is the
 * SSS_ERROR_TOO_MANY_SHARES the failed split produced, that shard_len was
 * zeroed rather than left as the caller wrote it, and that output was
 * actually cleared rather than left holding shards built from whatever was
 * on the stack.
 *
 * Not reachable from the UI today (the menu options that set group/member
 * counts cannot exceed the arrays either), but a public library entry point,
 * not an internal-only path.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <lcx_rng.h>

#include "sskr.h"
#include "sss-constants.h"
#include "testutils.h"

#define SHARDS_CAPACITY (SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT)
#define SECRET_LEN      (SSKR_MIN_STRENGTH_BYTES)

/*
 * One more member share than sss_split_secret() (and the shards[] array it
 * feeds) can hold. Nothing here is malformed: it is what an (n)-of-(n+1)
 * single-group backup looks like when SSS_MAX_SHARE_COUNT is smaller on one
 * target than the count a UI on a different target could produce.
 */
static void test_generate_reports_the_split_error_instead_of_succeeding(void **state) {
    (void) state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {
        {.threshold = 2, .count = SHARDS_CAPACITY + 1}
    };
    const uint16_t share_buffer_len =
        (SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * (SHARDS_CAPACITY + 1);
    uint8_t share_buffer[share_buffer_len];
    uint8_t expected_buffer[share_buffer_len];
    uint8_t share_len = 0xFF;

    memset(share_buffer, 0xFF, share_buffer_len);
    memset(expected_buffer, 0, share_buffer_len);

    int16_t result = sskr_generate_shards(1, groups, 1, master_secret,
                                          SECRET_LEN, &share_len, share_buffer,
                                          share_buffer_len, cx_rng);

    assert_int_equal(result, SSS_ERROR_TOO_MANY_SHARES);
    assert_int_equal(share_len, 0);
    assert_memory_equal(share_buffer, expected_buffer, share_buffer_len);
}

/*
 * A group at exactly the capacity must still be accepted, so that the fix
 * cannot be an off-by-one that turns away a supported backup.
 */
static void test_generate_accepts_a_group_at_capacity(void **state) {
    (void) state;

    const uint8_t master_secret[SECRET_LEN] = {0};
    const sskr_group_descriptor_t groups[] = {
        {.threshold = 2, .count = SHARDS_CAPACITY}
    };
    const uint16_t share_buffer_len =
        (SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * SHARDS_CAPACITY;
    uint8_t share_buffer[share_buffer_len];
    uint8_t share_len;

    int16_t result = sskr_generate_shards(1, groups, 1, master_secret,
                                          SECRET_LEN, &share_len, share_buffer,
                                          share_buffer_len, cx_rng);

    assert_int_equal(result, SHARDS_CAPACITY);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_generate_reports_the_split_error_instead_of_succeeding),
        cmocka_unit_test(test_generate_accepts_a_group_at_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
