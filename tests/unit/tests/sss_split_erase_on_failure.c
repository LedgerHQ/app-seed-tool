/*
 * Regression test for the un-erased output buffer on the
 * interpolation-failure path of sss_split_secret()
 * (src/common/sskr/sss/sss.c).
 *
 * sss_split_secret() and sss_recover_secret() fail in the same place, on
 * the same interpolate() call, and used to draw different conclusions from
 * it. The recovery side erases what it wrote before bailing out:
 *
 *     memzero(secret, share_length);
 *     memzero(digest, sizeof(digest));
 *     memzero(verify, sizeof(verify));
 *     return SSS_ERROR_INTERPOLATION_FAILURE;
 *
 * The split side returned the error code and nothing else, leaving its own
 * digest/x/y cleanup on the success path only and the caller's `result`
 * buffer holding whatever had been written into it so far. That buffer is
 * not scratch: for threshold >= 3 the first loop has already filled
 * threshold - 2 shares from the random generator, and interpolate() writes
 * whole shares of its own before the call that fails. Shamir shares are
 * sensitive material.
 *
 * This was not a live leak. The only caller,
 * sskr_generate_shards_internal(), already erases the group_shares and
 * member_shares buffers it hands over on this exact path (covered by
 * sskr_generate_erase_on_split_failure.c). What it was is a function whose
 * safety depended entirely on its caller's discipline, in a file that is
 * also exposed as a library, and whose twin already did the work itself.
 *
 * Forcing a genuine interpolation failure without faulting the secret
 * sharing math: interpolate()'s first action is cx_bn_lock(), which fails
 * immediately if a BN context is already locked. Its own cleanup path
 * unlocks on the way out, so no matching unlock is needed here -- the same
 * technique as sss_recover_erase_length.c and
 * sskr_combine_erase_on_failure.c.
 *
 * Two thresholds, because they reach that failure with the output buffer
 * in two different states:
 *
 *   - threshold 2: `for (i = threshold - 2; ...)` starts at 0, so
 *     interpolate() fails on the very first share and the first loop ran
 *     zero times. Nothing in `result` came from this call at all -- the
 *     erase is the only thing that can clean it.
 *   - threshold 3: the first loop has already written share 0 from the
 *     random generator, so `result` holds a real share when the failure
 *     hits, and the erase has to reach it.
 *
 * `shares` and `canary` are fields of one struct rather than separate
 * stack arrays, so the layout is guaranteed by the language instead of
 * left to the compiler: `canary` is guaranteed to sit immediately after
 * the output buffer. The erase must stop at share_count * secret_length,
 * the capacity sss.h requires of that buffer and exactly what the success
 * path writes -- not at some larger fixed size. Erasing past the caller's
 * buffer is not a hypothetical here: it is the precise defect
 * sss_recover_erase_length.c exists for, on the recovery side of this same
 * file. The canary is checked directly rather than left to a sanitizer,
 * because memzero() resolves to explicit_bzero(), which this toolchain's
 * AddressSanitizer does not appear to intercept (see that same file).
 *
 * WHAT THIS FILE DOES NOT CHECK, deliberately: `digest`, `x` and `y` are
 * stack locals of sss_split_secret(). Once it returns, a unit test has no
 * reliable way to inspect them -- the frame is gone, and reading it back
 * would be undefined behaviour that any change of compiler or
 * optimisation level could quietly invalidate. Their erasure is asserted
 * by reading the code and by symmetry with sss_recover_secret(), not by
 * observation. The identical cleanup on the *success* path, which predates
 * this change, is untested for exactly the same reason. Neither is worked
 * around with stack probing or by re-including the translation unit to
 * expose the locals; the part that is genuinely observable -- the caller's
 * buffer -- is what is asserted.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sss-constants.h"
#include "sss.h"
#include "testutils.h"

#define SECRET_LEN (SSS_MIN_SECRET_SIZE)
#define CANARY_LEN (SSS_MAX_SECRET_SIZE)
#define PATTERN (0xEE)
#define PRIOR_BYTE (0xFF)
#define CANARY_BYTE (0xA5)

/* Stands in for cx_rng. A fixed non-zero pattern rather than real
 * randomness: "the buffer is all zero afterwards" then means the erase ran,
 * not that the generator happened to hand back zeros. */
static bool fill_with_pattern(uint8_t* buffer, size_t length) {
    memset(buffer, PATTERN, length);
    return true;
}

/* threshold 2: the first loop runs zero times, so interpolate() fails
 * before anything in the output buffer belongs to this call. */
static void test_split_failure_erases_an_untouched_output(void** state) {
    (void)state;

    const uint8_t threshold = 2;
    const uint8_t share_count = 2;
    const uint8_t secret[SECRET_LEN] = {0};
    struct {
        uint8_t shares[2 * SECRET_LEN];
        uint8_t canary[CANARY_LEN];
    } guarded;

    memset(guarded.shares, PRIOR_BYTE, sizeof(guarded.shares));
    memset(guarded.canary, CANARY_BYTE, sizeof(guarded.canary));

    /* Force interpolate()'s own cx_bn_lock() to fail: it refuses to lock an
     * already-locked context. Its cleanup path unlocks whatever is locked
     * when it bails out, so this releases the lock again on the way out --
     * no matching unlock needed here. */
    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result =
        sss_split_secret(threshold, share_count, secret, SECRET_LEN,
                         guarded.shares, fill_with_pattern);

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);

    uint8_t expected_shares[sizeof(guarded.shares)];
    memset(expected_shares, 0x00, sizeof(expected_shares));
    assert_memory_equal(guarded.shares, expected_shares,
                        sizeof(guarded.shares));

    uint8_t expected_canary[sizeof(guarded.canary)];
    memset(expected_canary, CANARY_BYTE, sizeof(expected_canary));
    assert_memory_equal(guarded.canary, expected_canary,
                        sizeof(guarded.canary));
}

/* threshold 3: the first loop writes share 0 into the output buffer before
 * the failing interpolate() call, so the buffer holds a real share at the
 * moment the function gives up. */
static void test_split_failure_erases_shares_already_written(void** state) {
    (void)state;

    const uint8_t threshold = 3;
    const uint8_t share_count = 3;
    const uint8_t secret[SECRET_LEN] = {0};
    struct {
        uint8_t shares[3 * SECRET_LEN];
        uint8_t canary[CANARY_LEN];
    } guarded;

    memset(guarded.shares, PRIOR_BYTE, sizeof(guarded.shares));
    memset(guarded.canary, CANARY_BYTE, sizeof(guarded.canary));

    assert_int_equal(cx_bn_lock(1, 0), CX_OK);

    int16_t result =
        sss_split_secret(threshold, share_count, secret, SECRET_LEN,
                         guarded.shares, fill_with_pattern);

    assert_int_equal(result, SSS_ERROR_INTERPOLATION_FAILURE);

    uint8_t expected_shares[sizeof(guarded.shares)];
    memset(expected_shares, 0x00, sizeof(expected_shares));
    assert_memory_equal(guarded.shares, expected_shares,
                        sizeof(guarded.shares));

    uint8_t expected_canary[sizeof(guarded.canary)];
    memset(expected_canary, CANARY_BYTE, sizeof(expected_canary));
    assert_memory_equal(guarded.canary, expected_canary,
                        sizeof(guarded.canary));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_split_failure_erases_an_untouched_output),
        cmocka_unit_test(test_split_failure_erases_shares_already_written),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
