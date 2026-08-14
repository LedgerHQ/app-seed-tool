/*
 * compare_recovery_phrase_finish() is the tail of
 * compare_recovery_phrase() (common_seed.c): the part that runs after
 * os_derive_bip32_no_throw() (a BOLOS syscall, not testable on host)
 * returns. Splitting it on that call's *result* rather than the call
 * itself makes the comparison and the buffer erasure testable, including
 * the derivation-failure path -- never exercised anywhere in this suite
 * until now, since nothing could force that syscall to fail in a
 * controlled way.
 *
 * It is also where the application decides whether the phrase in front of the
 * user is the one their device holds, which makes it the one decision here
 * worth injecting a fault into, and the reason the function is written the way
 * it is. What this file can hold of that, and what it cannot:
 *
 *   - the verdict is one of two constants that differ in all 32 bits, and the
 *     function returns one of those two and nothing else. Held, by asserting
 *     equality with the constants rather than truthiness, and by asserting the
 *     distance between them.
 *   - two independent comparisons have to agree. Held, by overriding
 *     os_secure_memcmp() for this target and making it report agreement on two
 *     root keys that differ in every byte -- which is what a glitch landing on
 *     that call looks like from here.
 *   - the third reading inside the "match" branch, and the checkpoint counter
 *     checked before the return. Not held, and not holdable here: both fire
 *     only on a fault, and a host process cannot be made to skip an
 *     instruction. They are written to be read, and the erasure of both
 *     buffers is asserted on every path so that neither can quietly stop
 *     happening.
 */

#include <cmocka.h>
#include <cx.h>
/* for the os_secure_memcmp() prototype: this file defines it, and a
 * definition with no declaration in sight is exactly what
 * -Wmissing-prototypes is here to report. */
#include <os_utils.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common.h"

static void assert_all_zero(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        assert_int_equal(data[i], 0);
    }
}

/*
 * os_secure_memcmp() again, overriding the one testutils.c exports for every
 * other target, so that one test can make it lie. This is the whole point of
 * the second comparison: a fault landing on that call, or on the register its
 * result comes back in, is the cheapest single-glitch way to turn "this is not
 * your seed" into "this is". Nothing else in this file changes behaviour --
 * s_memcmp_lies is false everywhere but inside the one test that sets it, and
 * that test puts it back.
 *
 * The body when it is not lying is the SDK's own (src/os.c), byte for byte:
 * two counters run down together, every byte XOR-accumulated, no early exit.
 */
static bool s_memcmp_lies = false;

char os_secure_memcmp(const void* src1, const void* src2, size_t length) {
    const unsigned char* a = (const unsigned char*)src1;
    const unsigned char* b = (const unsigned char*)src2;
    unsigned char xoracc = 0;

    if (s_memcmp_lies) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        xoracc |= a[i] ^ b[i];
    }
    return (char)xoracc;
}

static void test_finish_matching_keys_succeeds_and_erases(void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x42, sizeof(buffer));
    memset(buffer_device, 0x42, sizeof(buffer_device));

    const unsigned int result =
        compare_recovery_phrase_finish(CX_OK, buffer, buffer_device);

    assert_int_equal(result, VERDICT_MATCH);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

static void test_finish_mismatched_keys_fails_and_erases(void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x42, sizeof(buffer));
    memset(buffer_device, 0x24, sizeof(buffer_device));

    const unsigned int result =
        compare_recovery_phrase_finish(CX_OK, buffer, buffer_device);

    assert_int_equal(result, VERDICT_NO_MATCH);
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

    const unsigned int result = compare_recovery_phrase_finish(
        CX_INTERNAL_ERROR, buffer, buffer_device);

    assert_int_equal(result, VERDICT_NO_MATCH);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

/*
 * The mutation the second comparison exists for: os_secure_memcmp() reports
 * agreement on two root keys that differ in all 64 bytes. That is what a
 * successful glitch on that call looks like from here, and it is the error
 * direction that costs the user their funds -- a false "no match" sends them
 * back to the entry screen, a false "match" sends them away with a backup that
 * does not open their device.
 *
 * A second call to os_secure_memcmp() would not catch this: the stub lies to
 * every caller, exactly as a fault on the function's own body would. What
 * catches it has to read the bytes itself.
 */
static void test_finish_a_defeated_comparison_is_still_a_mismatch(
    void** state) {
    (void)state;

    uint8_t buffer[64];
    uint8_t buffer_device[64];
    memset(buffer, 0x42, sizeof(buffer));
    memset(buffer_device, 0x24, sizeof(buffer_device));

    s_memcmp_lies = true;
    const unsigned int result =
        compare_recovery_phrase_finish(CX_OK, buffer, buffer_device);
    s_memcmp_lies = false;

    assert_int_equal(result, VERDICT_NO_MATCH);
    assert_all_zero(buffer, sizeof(buffer));
    assert_all_zero(buffer_device, sizeof(buffer_device));
}

/*
 * What makes the two verdicts worth carrying around as 32-bit values rather
 * than as a bool: no single bit flip anywhere between this function and the
 * screen turns one into the other, and neither is a value a register arrives
 * at by accident. Asserted rather than left to whoever next edits common.h --
 * two constants that merely look unusual are not the same as two constants a
 * fault cannot bridge.
 */
static void test_the_two_verdicts_cannot_be_bridged_by_one_bit(void** state) {
    (void)state;

    const unsigned int difference = VERDICT_MATCH ^ VERDICT_NO_MATCH;
    unsigned int differing_bits = 0;

    for (unsigned int bit = 0; bit < 32; bit++) {
        differing_bits += (difference >> bit) & 1u;
    }

    assert_int_equal(differing_bits, 32);

    /* and neither is a value a cleared, incremented or short-counting
     * register lands on */
    assert_int_not_equal(VERDICT_MATCH, 0u);
    assert_int_not_equal(VERDICT_MATCH, 1u);
    assert_int_not_equal(VERDICT_NO_MATCH, 0u);
    assert_int_not_equal(VERDICT_NO_MATCH, 1u);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_the_two_verdicts_cannot_be_bridged_by_one_bit),
        cmocka_unit_test(test_finish_matching_keys_succeeds_and_erases),
        cmocka_unit_test(test_finish_mismatched_keys_fails_and_erases),
        cmocka_unit_test(test_finish_derivation_failure_fails_and_erases),
        cmocka_unit_test(
            test_finish_a_defeated_comparison_is_still_a_mismatch),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
