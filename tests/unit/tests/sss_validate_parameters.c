/*
 * Coverage for the secret-length rejections of sss_validate_parameters()
 * (src/common/sskr/sss/sss.c), and for the one call site that has never seen
 * that helper fail.
 *
 * A gcov run over the whole suite shows the three secret-length branches with
 * an execution count of zero: every existing test feeds sss_split_secret() a
 * 32-byte secret, so only the share-count and threshold branches above them
 * ever fire. Each test below violates exactly one of the three conditions and
 * leaves the others satisfied, so the returned code identifies the branch that
 * ran.
 *
 * sss_recover_secret() calls the same helper first thing, but no existing test
 * makes that call fail either -- its `if (error) return error;` also has a zero
 * count, while the equivalent line in sss_split_secret() does not. The last two
 * tests cover it.
 *
 * This is coverage on already-correct code, not a bug fix.
 */

#include <cmocka.h>
#include <lcx_rng.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sss.h"
#include "testutils.h"

/* Large enough for every length used below, valid or not. */
#define SECRET_CAPACITY (64)
#define SHARE_COUNT (3)
#define THRESHOLD (2)

static void split_expecting(uint8_t secret_length, int16_t expected) {
    uint8_t secret[SECRET_CAPACITY];
    uint8_t result[SECRET_CAPACITY * SHARE_COUNT];

    memset(secret, 0x5A, sizeof(secret));
    memset(result, 0x00, sizeof(result));

    int16_t error = sss_split_secret(THRESHOLD, SHARE_COUNT, secret,
                                     secret_length, result, test_rng);

    assert_int_equal(error, expected);
}

static void test_split_rejects_secret_longer_than_max(void** state) {
    (void)state;

    /* Even, above SSS_MAX_SECRET_SIZE: only the "too long" branch applies. */
    split_expecting(SSS_MAX_SECRET_SIZE + 2, SSS_ERROR_SECRET_TOO_LONG);
}

static void test_split_rejects_secret_shorter_than_min(void** state) {
    (void)state;

    /* Even, below SSS_MIN_SECRET_SIZE and not above the maximum. */
    split_expecting(SSS_MIN_SECRET_SIZE - 2, SSS_ERROR_SECRET_TOO_SHORT);
}

static void test_split_rejects_odd_secret_length(void** state) {
    (void)state;

    /* Within [SSS_MIN_SECRET_SIZE, SSS_MAX_SECRET_SIZE], but odd, so the two
     * range checks pass and only the parity one can fire. */
    split_expecting(SSS_MIN_SECRET_SIZE + 1, SSS_ERROR_SECRET_NOT_EVEN_LEN);
}

static void recover_expecting(uint8_t threshold, uint8_t share_length,
                              int16_t expected) {
    uint8_t share[SECRET_CAPACITY];
    uint8_t secret[SECRET_CAPACITY];
    const uint8_t* shares[] = {share, share};
    const uint8_t member_indexes[] = {0x00, 0x01};

    memset(share, 0x5A, sizeof(share));
    memset(secret, 0x00, sizeof(secret));

    int16_t error = sss_recover_secret(threshold, member_indexes, shares,
                                       share_length, secret);

    assert_int_equal(error, expected);
}

static void test_recover_rejects_zero_threshold(void** state) {
    (void)state;

    /* sss_recover_secret() validates with share_count == threshold, so a zero
     * threshold trips `threshold < 1` before anything is read. */
    recover_expecting(0, SSS_MAX_SECRET_SIZE, SSS_ERROR_INVALID_THRESHOLD);
}

static void test_recover_rejects_odd_share_length(void** state) {
    (void)state;

    recover_expecting(2, SSS_MIN_SECRET_SIZE + 1,
                      SSS_ERROR_SECRET_NOT_EVEN_LEN);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_split_rejects_secret_longer_than_max),
        cmocka_unit_test(test_split_rejects_secret_shorter_than_min),
        cmocka_unit_test(test_split_rejects_odd_secret_length),
        cmocka_unit_test(test_recover_rejects_zero_threshold),
        cmocka_unit_test(test_recover_rejects_odd_share_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
