/*
 * compare_recovery_phrase_finish() is the tail of
 * compare_recovery_phrase() (common_seed.c): the part that runs after
 * os_derive_bip32_no_throw() (a BOLOS syscall, not testable on host)
 * returns. Splitting it on that call's *result* rather than the call
 * itself makes the comparison and the buffer erasure testable, including
 * the derivation-failure path -- never exercised anywhere in this suite
 * until now, since nothing could force that syscall to fail in a
 * controlled way.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern bool compare_recovery_phrase_finish(cx_err_t derivation_status,
                                           uint8_t buffer[64],
                                           uint8_t buffer_device[64]);

static void assert_all_zero(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        assert_int_equal(data[i], 0);
    }
}

static void test_finish_matching_keys_succeeds_and_erases(void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x42, sizeof(buffer));
    memset(buffer_device, 0x42, sizeof(buffer_device));

    bool result = compare_recovery_phrase_finish(CX_OK, buffer, buffer_device);

    assert_true(result);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

static void test_finish_mismatched_keys_fails_and_erases(void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x42, sizeof(buffer));
    memset(buffer_device, 0x24, sizeof(buffer_device));

    bool result = compare_recovery_phrase_finish(CX_OK, buffer, buffer_device);

    assert_false(result);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

// derivation_status != CX_OK short-circuits before the comparison -- buffer
// and buffer_device are prefilled with distinct, arbitrary non-zero values
// (as real leftover memory from a failed derivation would be, not the
// zeroed/matching state the two tests above use) precisely so that a
// comparison, if one happened, would not by coincidence agree with the
// expected `false` result. CX_INTERNAL_ERROR is the one error
// os_derive_bip32_with_seed_no_throw() (os_seed.h) is documented to return
// on failure, not an invented status.
static void test_finish_derivation_failure_fails_and_erases(void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x11, sizeof(buffer));
    memset(buffer_device, 0x99, sizeof(buffer_device));

    bool result = compare_recovery_phrase_finish(CX_INTERNAL_ERROR, buffer,
                                                 buffer_device);

    assert_false(result);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_finish_matching_keys_succeeds_and_erases),
        cmocka_unit_test(test_finish_mismatched_keys_fails_and_erases),
        cmocka_unit_test(test_finish_derivation_failure_fails_and_erases),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
