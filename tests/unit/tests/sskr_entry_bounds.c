/*
 * Regression test for the unbounded SSKR entry buffer.
 *
 * sskr_shares_word_add() appends one byte per entered ByteWord:
 *
 *     shares.buffer[shares.length] = bolos_ux_sskr_byteword_to_hex(byteword);
 *     ...
 *     shares.length++;
 *
 * and nothing compares shares.length against SSKR_SHARES_MAX_LENGTH, which is
 * only ever used to declare the buffer. Both quantities that decide how much
 * gets written come from the entered data itself:
 *
 *     case 4: final_size = 4 + 1 + buffer[length] + 4;   // up to 264
 *     case 8: count      = (buffer[length] & 0x0F) + 1;  // up to 16
 *
 * 16 shares x 264 bytes is 4224 against a 3664-byte buffer, and
 * bolos_ux_sskr_hex_check() only runs once every share has been entered, so
 * the CRC does not gate this.
 *
 * What the overflow does differs between the two UI stacks, and only the first
 * of these is exercised here:
 *
 *   - NBGL (this file): `length` is the field immediately after `buffer` in
 *     sskr_buffer_t, so the first out-of-bounds write lands on the counter
 *     driving it. The writes then fold back inside the buffer. The damage is
 *     silent corruption of already-entered shares and an inconsistent counter,
 *     contained within the struct.
 *
 *   - BAGL (src/bagl/nanos_enter_phrase.c, nanox_enter_phrase.c): there
 *     sskr_words_buffer is the LAST member of bolos_ux_context_t and its
 *     counter sits before it, so nothing folds the writes back and they run
 *     past the end of the object -- 4224 bytes against 1603 on Nano S. Those
 *     two files live inside the BAGL UX flows and cannot be driven from this
 *     harness; they take the same change, verified by compilation only.
 *
 * A serialized shard is SSKR_METADATA_LENGTH_BYTES plus a 16..32 byte value,
 * so one share is at most 5 + 37 + 4 = 46 bytes on the wire. Nothing larger
 * can be a share.
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
 * comparison. Neither is on the entry path under test; these exist only so the
 * object links, and each aborts if it is ever reached.
 */
char *bip39_mnemonic_get(void) { fail_msg("not on the entry path"); }
size_t bip39_mnemonic_length_get(void) { fail_msg("not on the entry path"); }
uint8_t bip39_mnemonic_final_size_get(void) { fail_msg("not on the entry path"); }
bool compare_recovery_phrase(void) { fail_msg("not on the entry path"); }

/* Largest a serialized share can be on the wire: CBOR long-form header
 * (tag + 0x58 + length byte), the shard itself, and the CRC32. */
#define MAX_SHARE_WIRE_LEN \
    (5 + SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES + 4)

/* ByteWord whose decoded value is `b`; the table holds 256 four-letter words. */
static const char *word_for(uint8_t b)
{
    return (const char *) &SSKR_WORDLIST[(size_t) b * SSKR_BYTEWORD_LENGTH];
}

/*
 * Header of a share declaring a 255-byte body and a member-threshold nibble of
 * 0x0F: the largest values the wire format can express. Returns how many words
 * it consumed.
 */
static size_t feed_worst_case_header(void)
{
    sskr_shares_word_add(word_for(0xD9)); /* CBOR tag                      */
    sskr_shares_word_add(word_for(0x9D));
    sskr_shares_word_add(word_for(0x75));
    sskr_shares_word_add(word_for(0x58)); /* byte string, long form        */
    sskr_shares_word_add(word_for(0xFF)); /* declared body length = 255    */
    sskr_shares_word_add(word_for(0x00)); /* identifier                    */
    sskr_shares_word_add(word_for(0x00));
    sskr_shares_word_add(word_for(0x00)); /* group threshold / count       */
    sskr_shares_word_add(word_for(0x0F)); /* member threshold -> 16 shares */
    return 9;
}

/*
 * Every accepted word must advance the buffer by exactly one byte. Once the
 * write runs off the end it lands on `length` itself, and the counter stops
 * tracking the buffer -- which is what this pins down.
 */
static void test_entry_counter_never_folds_back(void **state)
{
    (void) state;

    sskr_shares_reset();
    size_t fed = feed_worst_case_header();

    while (fed < SSKR_SHARES_MAX_LENGTH + 64) {
        const size_t before = sskr_shares_length_get();
        sskr_shares_word_add(word_for(0x42));
        const size_t after = sskr_shares_length_get();

        /* Either the word was refused (length unchanged) or it advanced by
         * exactly one. Anything else means the counter was overwritten. */
        if (after != before) {
            assert_int_equal(after, before + 1);
        }
        assert_true(after <= SSKR_SHARES_MAX_LENGTH);
        fed++;
    }
}

/* Nothing from `from` to the end of the shares buffer may survive a shrink. */
static void assert_erased_from(size_t from) {
    const char* buffer = sskr_shares_get();

    for (size_t i = from; i < SSKR_SHARES_MAX_LENGTH; i++) {
        assert_int_equal(buffer[i], 0);
    }
}

/*
 * sskr_shares_shrink() is the exact twin of bip39_mnemonic_shrink(), which
 * bip39_entry_bounds.c already covers with these three cases; the shares half
 * had none. It is what erases entered data when the user backs out of a word,
 * so each of the three ways of asking for it -- part of the buffer, all of it
 * via a size of 0, all of it via an over-large size -- has to leave the tail
 * zeroed and the length consistent with what was kept.
 *
 * Unlike the BIP39 buffer, which holds the words as text, this one holds one
 * decoded byte per ByteWord, so a word is one byte here and there is no
 * separator.
 */
static void test_shrink_erases_what_it_drops(void** state) {
    (void)state;

    /* Partial removal: the tail goes, the head stays. */
    sskr_shares_reset();
    sskr_shares_word_add(word_for(0x11));
    sskr_shares_word_add(word_for(0x42));
    assert_int_equal(sskr_shares_length_get(), 2);

    assert_int_equal(sskr_shares_shrink(1), 1);
    assert_int_equal(sskr_shares_length_get(), 1);
    assert_int_equal((uint8_t)sskr_shares_get()[0], 0x11);
    assert_erased_from(1);

    /* size == 0: erase all. */
    assert_int_equal(sskr_shares_shrink(0), 0);
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_erased_from(0);

    /* size > length: erase all, without wrapping the length around. */
    sskr_shares_reset();
    sskr_shares_word_add(word_for(0x42));
    assert_int_equal(sskr_shares_shrink(sskr_shares_length_get() + 1), 0);
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_erased_from(0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_entry_counter_never_folds_back),
        cmocka_unit_test(test_shrink_erases_what_it_drops),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
