/*
 * Regression test for the ByteWords decoder's failure sentinel.
 *
 * bolos_ux_sskr_byteword_to_hex() returns SSKR_WORDLIST_LENGTH /
 * SSKR_BYTEWORD_LENGTH -- 256, one past the last valid index -- when a
 * ByteWord is not in the list. Its only caller stores that in a `char`:
 *
 *     shares.buffer[shares.length] =
 *         bolos_ux_sskr_byteword_to_hex((unsigned char *) byteword);
 *
 * 256 truncates to 0, which is also the index of the first word in the
 * list ("able"), so an unrecognised ByteWord is silently accepted as byte
 * 0x00 rather than refused.
 *
 * Not reachable today: NBGL only ever passes words that came from
 * bolos_ux_sskr_fill_with_candidates() (already in the wordlist), and BAGL
 * does not use this function at all. This fixes a documented trap rather
 * than a live one -- see issue #59's ByteWords comment.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
#include "../../../src/nbgl/sskr_shares.h"

extern unsigned char const SSKR_WORDLIST[];

/*
 * sskr_shares.c also holds sskr_shares_from_bip39_mnemonic() and
 * sskr_shares_check(), which reach into the BIP39 module and the seed
 * comparison. Neither is on the entry path under test; these exist only so
 * the object links, and each aborts if it is ever reached.
 */
char *bip39_mnemonic_get(void) { fail_msg("not on the entry path"); }
size_t bip39_mnemonic_length_get(void) { fail_msg("not on the entry path"); }
uint8_t bip39_mnemonic_final_size_get(void) { fail_msg("not on the entry path"); }
bool compare_recovery_phrase(void) { fail_msg("not on the entry path"); }

/* ByteWord whose decoded value is `b`; the table holds 256 four-letter words. */
static const char *word_for(uint8_t b)
{
    return (const char *) &SSKR_WORDLIST[(size_t) b * SSKR_BYTEWORD_LENGTH];
}

/*
 * A recognised word must still be accepted normally: this pins down the
 * positive case so the rejection test below cannot pass by having the
 * caller refuse every word.
 */
static void test_known_word_is_accepted(void **state)
{
    (void) state;
    sskr_shares_reset();

    size_t words = sskr_shares_word_add(word_for(5));

    assert_int_equal(words, 1);
    assert_int_equal(sskr_shares_length_get(), 1);
    assert_int_equal((uint8_t) sskr_shares_get()[0], 5);
}

/*
 * "0000" is four digits; none of the 256 ByteWords are anything but
 * letters, so this cannot match a real entry. It must be refused outright,
 * not accepted as byte 0x00.
 */
static void test_unknown_word_is_refused(void **state)
{
    (void) state;
    sskr_shares_reset();

    size_t words = sskr_shares_word_add("0000");

    assert_int_equal(words, 0);
    assert_int_equal(sskr_shares_length_get(), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_known_word_is_accepted),
        cmocka_unit_test(test_unknown_word_is_refused),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
