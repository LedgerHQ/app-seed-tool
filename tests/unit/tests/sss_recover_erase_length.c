/*
 * Regression test for the oversized erase in sss_recover_secret()'s
 * interpolation-failure path.
 *
 * Upstream declared `uint8_t digest[share_length]` (a VLA), so
 * `memzero(secret, sizeof(digest))` erased exactly `share_length` bytes. The
 * port declares `digest[SSS_MAX_SECRET_SIZE]` (32 bytes, fixed), so the same
 * call now always erases 32 bytes of the caller's `secret` buffer regardless
 * of `share_length` -- which the caller is only required to size at
 * `share_length` bytes (SSS_MIN_SECRET_SIZE..SSS_MAX_SECRET_SIZE, so as few
 * as 16). Only on the interpolation-failure path; current callers happen to
 * pass buffers large enough for it not to matter.
 *
 * Forcing a genuine interpolation failure would need faulting the crypto
 * library's Lagrange interpolation itself. interpolate() takes a shortcut
 * instead: its first action is `cx_bn_lock()`, which fails immediately if a
 * BN context is already locked. Locking one before calling
 * sss_recover_secret() makes interpolate() fail at its very first line,
 * deterministically, without touching any of the secret-sharing math.
 *
 * `secret` and `canary` are fields of one struct rather than separate stack
 * arrays, so the layout is guaranteed by the language instead of left to the
 * compiler: `canary` is guaranteed to sit immediately after `secret`. The
 * erase is asserted against the canary rather than under a sanitizer --
 * AddressSanitizer's stack-buffer-overflow check did not fire on this write,
 * because `memzero` here resolves to `explicit_bzero`, which this
 * toolchain's ASan does not appear to intercept.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <cx.h>

#include "sss.h"
#include "sss-constants.h"
#include "testutils.h"

struct guarded_secret {
    uint8_t secret[SSS_MIN_SECRET_SIZE];
    uint8_t canary[SSS_MAX_SECRET_SIZE - SSS_MIN_SECRET_SIZE];
};

static void test_recover_failure_does_not_overrun_a_min_size_secret(void **state) {
    (void) state;

    const uint8_t share_length = SSS_MIN_SECRET_SIZE;
    struct guarded_secret guarded;
    uint8_t share_a[SSS_MIN_SECRET_SIZE];
    uint8_t share_b[SSS_MIN_SECRET_SIZE];
    const uint8_t x[2] = {0, 1};
    const uint8_t* shares[2] = {share_a, share_b};

    memset(share_a, 0x11, sizeof(share_a));
    memset(share_b, 0x22, sizeof(share_b));
    memset(guarded.canary, 0xA5, sizeof(guarded.canary));

    /* Force interpolate()'s own cx_bn_lock() to fail: it refuses to lock an
     * already-locked context. Its cleanup path unlocks whatever is locked
     * when it bails out, so this releases the lock again on the way out --
     * no matching unlock needed here. */
    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result = sss_recover_secret(2, x, shares, share_length, guarded.secret);

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);

    uint8_t expected_canary[sizeof(guarded.canary)];
    memset(expected_canary, 0xA5, sizeof(expected_canary));
    assert_memory_equal(guarded.canary, expected_canary, sizeof(guarded.canary));
}

/*
 * At share_length == SSS_MAX_SECRET_SIZE the erase is the right size by
 * coincidence -- this is the case the issue meant by "current callers pass
 * buffers large enough for it not to matter". Kept as a boundary check so
 * the fix cannot regress it.
 */
static void test_recover_failure_at_max_size_still_reports_the_error(void **state) {
    (void) state;

    const uint8_t share_length = SSS_MAX_SECRET_SIZE;
    uint8_t secret[SSS_MAX_SECRET_SIZE];
    uint8_t share_a[SSS_MAX_SECRET_SIZE];
    uint8_t share_b[SSS_MAX_SECRET_SIZE];
    const uint8_t x[2] = {0, 1};
    const uint8_t* shares[2] = {share_a, share_b};

    memset(share_a, 0x11, sizeof(share_a));
    memset(share_b, 0x22, sizeof(share_b));

    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result = sss_recover_secret(2, x, shares, share_length, secret);

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_recover_failure_does_not_overrun_a_min_size_secret),
        cmocka_unit_test(test_recover_failure_at_max_size_still_reports_the_error),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
