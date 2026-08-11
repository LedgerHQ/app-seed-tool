/*
 * bip85_path_format(): the derivation path as the specification writes it.
 *
 * A BIP-85 result is reproducible somewhere else only if the path is known,
 * which is what makes it worth deriving rather than storing -- and no screen
 * in this application showed one before. Displaying it puts the path into the
 * class of things that must be right rather than merely present: a path wrong
 * in one component still derives a perfectly well-formed secret, just not the
 * one on screen, and nothing downstream would notice.
 *
 * So the cases below are pinned against the decimal numbers in the
 * specification (BIP-85, "Applications"), not against what the builders
 * happen to produce -- tests/bip85_path_components.c already holds the
 * builders themselves. What this file adds is that the rendering does not lose
 * or invent anything on the way to the screen: the hardening bit is taken off
 * every component and reported by the apostrophe, and a path that does not fit
 * is refused outright rather than shown cut short.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/bip85/bip85_internal.h"
#include "common/bip85/common_bip85.h"

/* m / 83696968' / 39' / 0' / 24' / 42' -- the example the interface
 * specification carries, and the shape the BIP-39 application derives. */
static void test_formats_the_bip39_path(void** state) {
    (void)state;

    unsigned int path[5];
    char out[BIP85_PATH_STRING_MAX_LENGTH];

    const unsigned int len = bip85_path_bip39(path, 0, 24, 42);

    assert_true(bip85_path_format(path, len, out, sizeof(out)));
    assert_string_equal(out, "m/83696968'/39'/0'/24'/42'");
}

static void test_formats_the_password_paths(void** state) {
    (void)state;

    unsigned int path[4];
    char out[BIP85_PATH_STRING_MAX_LENGTH];

    /* Base64 is application 707764, Base85 is 707785 -- two numbers that
     * differ in one digit, which is exactly the kind of slip a rendered path
     * is there to make visible. */
    assert_true(bip85_path_format(path, bip85_path_pwd_base64(path, 21, 0), out,
                                  sizeof(out)));
    assert_string_equal(out, "m/83696968'/707764'/21'/0'");

    assert_true(bip85_path_format(path, bip85_path_pwd_base85(path, 12, 7), out,
                                  sizeof(out)));
    assert_string_equal(out, "m/83696968'/707785'/12'/7'");
}

static void test_formats_the_largest_index_the_keypad_accepts(void** state) {
    (void)state;

    unsigned int path[5];
    char out[BIP85_PATH_STRING_MAX_LENGTH];

    /* BIP85_INDEX_MAX_NUMBER_LENGTH caps entry at seven digits, so this is the
     * longest real path the application can be asked to draw. It has to fit in
     * the buffer the screens size from BIP85_PATH_STRING_MAX_LENGTH. */
    const unsigned int len = bip85_path_bip39(path, 0, 24, 9999999);

    assert_true(bip85_path_format(path, len, out, sizeof(out)));
    assert_string_equal(out, "m/83696968'/39'/0'/24'/9999999'");
}

static void test_buffer_holds_the_widest_path_possible(void** state) {
    (void)state;

    /* Not the widest the *screens* can ask for -- the widest the type allows:
     * five components each one below the hardening bit. If
     * BIP85_PATH_STRING_MAX_LENGTH is ever cut to fit what today's keypad
     * accepts, this is what says the constant no longer covers its own
     * documented domain. */
    unsigned int path[5];
    char out[BIP85_PATH_STRING_MAX_LENGTH];

    for (unsigned int i = 0; i < 5; i++) {
        path[i] = 0x80000000u | 0x7FFFFFFFu;
    }

    assert_true(bip85_path_format(path, 5, out, sizeof(out)));
    assert_string_equal(
        out, "m/2147483647'/2147483647'/2147483647'/2147483647'/2147483647'");
    assert_true(strlen(out) < BIP85_PATH_STRING_MAX_LENGTH);
}

/*
 * The refusals. A path shown cut short is a different path, not a partial one,
 * so every one of these has to leave the caller with nothing to display.
 */
static void test_refuses_rather_than_truncating(void** state) {
    (void)state;

    unsigned int path[5];
    char out[16];

    const unsigned int len = bip85_path_bip39(path, 0, 24, 42);

    /* One byte short of the 27 this path needs, including its terminator. */
    char exact[27];
    assert_true(bip85_path_format(path, len, exact, sizeof(exact)));
    assert_string_equal(exact, "m/83696968'/39'/0'/24'/42'");

    char one_short[26];
    assert_false(bip85_path_format(path, len, one_short, sizeof(one_short)));
    assert_string_equal(one_short, "");

    assert_false(bip85_path_format(path, len, out, sizeof(out)));
    assert_string_equal(out, "");

    /* A capacity that cannot even hold "m". */
    char tiny[1];
    assert_false(bip85_path_format(path, len, tiny, sizeof(tiny)));
    assert_string_equal(tiny, "");
}

static void test_refuses_a_path_that_was_never_built(void** state) {
    (void)state;

    unsigned int path[5] = {0};
    char out[BIP85_PATH_STRING_MAX_LENGTH];

    /* bip85_app_path_format() reports an unknown application as a zero-length
     * path. Rendering that as a bare "m" would put a derivation nobody
     * performed on screen. */
    assert_false(bip85_path_format(path, 0, out, sizeof(out)));
    assert_string_equal(out, "");

    assert_false(bip85_path_format(NULL, 5, out, sizeof(out)));
    assert_string_equal(out, "");

    assert_false(bip85_path_format(path, 5, NULL, sizeof(out)));
    assert_false(bip85_path_format(path, 5, out, 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_formats_the_bip39_path),
        cmocka_unit_test(test_formats_the_password_paths),
        cmocka_unit_test(test_formats_the_largest_index_the_keypad_accepts),
        cmocka_unit_test(test_buffer_holds_the_widest_path_possible),
        cmocka_unit_test(test_refuses_rather_than_truncating),
        cmocka_unit_test(test_refuses_a_path_that_was_never_built),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
