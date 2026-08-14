/*
 * What bolos_ux_bip39_mnemonic_to_seed() does when the key derivation
 * underneath it fails.
 *
 * The call it makes reads as a void one and is not: cx_pbkdf2_sha512 is a
 * macro (lcx_pbkdf2.h) over cx_pbkdf2_no_throw(CX_SHA512, ...), which returns
 * a cx_err_t. The SDK header says in as many words, above that prototype, that
 * it does not mark the return WARN_UNUSED_RESULT because the "return value is
 * never checked" -- so nothing in a build points at a caller that drops it.
 * The value was dropped here, in this application's seed derivation.
 *
 * The failure is not reachable from the entry screens. cx_pbkdf2_hmac()
 * returns CX_INVALID_PARAMETER only for a NULL password, salt or output, and
 * all three are stack arrays at the one call site; everything else it can
 * return comes from the 2048 rounds of HMAC-SHA512 underneath, i.e. from the
 * hardware. That is precisely the condition a fault-injection attempt sets out
 * to create, and it is why this is worth a test rather than an argument: the
 * only way to reach it on host is to stand in for that hardware.
 *
 * cx_pbkdf2_no_throw() is therefore defined here, overriding the SDK build of
 * it that libtestutils carries for every other target -- the same technique
 * tests/unit/tests/compare_recovery_phrase_finish.c uses for
 * os_secure_memcmp(). Two behaviours, chosen per test:
 *
 *   - an error, with the output left exactly as the caller passed it in. That
 *     is not quite what the real function does on a mid-loop failure (it fills
 *     the output block by block, so some of it would be derived), but it is
 *     the stronger case for what is asserted: if the caller returns the
 *     buffer untouched, whatever was there before travels on as a seed.
 *   - success, writing a recognisable pattern. Without this the test would
 *     pass against a function that zeroed the seed and returned false on every
 *     call.
 *
 * What this file does not cover: it does not exercise the real PBKDF2 at all.
 * tests/unit/tests/bip39.c holds that against published vectors, and now also
 * asserts the true return on each of them.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * bip39/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "bip39/common_bip39.h"
// clang-format on

/* An arbitrary mnemonic-shaped input. Nothing here decodes or validates it --
 * the function under test hashes whatever bytes it is given. */
static const unsigned char k_mnemonic[] =
    "fly mule excess resource treat plunge nose soda reflect adult ramp planet";

/* The byte the seed buffer is prefilled with, so that "was it erased" and
 * "was it never written" are different observations. */
#define PREFILL (0x5A)

/* The byte the succeeding stub writes, so the control test can tell a real
 * write from an untouched buffer. */
#define STUB_DERIVED (0xC7)

static bool s_pbkdf2_fails = false;

cx_err_t cx_pbkdf2_no_throw(cx_md_t md_type, const uint8_t *password,
                            size_t password_len, uint8_t *salt, size_t salt_len,
                            uint32_t iterations, uint8_t *out,
                            size_t out_len) {
    (void)md_type;
    (void)password;
    (void)password_len;
    (void)salt;
    (void)salt_len;
    (void)iterations;

    if (s_pbkdf2_fails) {
        /* left untouched on purpose: see the file header */
        return CX_INTERNAL_ERROR;
    }

    memset(out, STUB_DERIVED, out_len);
    return CX_OK;
}

static void test_a_failed_derivation_is_reported_and_wipes_the_seed(
    void **state) {
    (void)state;

    uint8_t seed[64];
    memset(seed, PREFILL, sizeof(seed));

    s_pbkdf2_fails = true;
    const bool ok = bolos_ux_bip39_mnemonic_to_seed(
        k_mnemonic, sizeof(k_mnemonic) - 1, seed);
    s_pbkdf2_fails = false;

    assert_false(ok);

    /* Not one byte of what was there is left. Before this, the caller had no
     * way to know the difference and would HMAC it as a seed. */
    for (size_t i = 0; i < sizeof(seed); i++) {
        assert_int_equal(seed[i], 0x00);
    }
}

/* The control. A function that answered false and zeroed the seed whatever
 * happened would satisfy the test above and be useless. */
static void test_a_successful_derivation_is_reported_and_kept(void **state) {
    (void)state;

    uint8_t seed[64];
    memset(seed, PREFILL, sizeof(seed));

    const bool ok = bolos_ux_bip39_mnemonic_to_seed(
        k_mnemonic, sizeof(k_mnemonic) - 1, seed);

    assert_true(ok);

    for (size_t i = 0; i < sizeof(seed); i++) {
        assert_int_equal(seed[i], STUB_DERIVED);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_failed_derivation_is_reported_and_wipes_the_seed),
        cmocka_unit_test(test_a_successful_derivation_is_reported_and_kept),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
