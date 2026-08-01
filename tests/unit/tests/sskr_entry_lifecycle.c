/*
 * Removal on the SSKR entry path, and how far the erasure it triggers reaches.
 *
 * src/nbgl/bip39_mnemonic.c and src/nbgl/sskr_shares.c do the same job for the
 * two entry modes -- accumulate what the user types, word by word, into a
 * static buffer, and unwind it when the user backs out. bip39_entry_bounds.c
 * drives ten of the thirteen functions in the BIP39 half,
 * bip39_mnemonic_word_remove() among them, leaving out only the three that are
 * off the entry path. The shares half has no equivalent:
 * sskr_shares_word_remove() is named by no unit test at all, and measured over
 * the whole suite every one of its six lines carries a zero counter. It is the
 * only caller of sskr_shares_shrink() in the application -- the path that
 * erases a share the user is taking back one word at a time.
 *
 * sskr_shares_shrink() itself is not untouched territory: sskr_entry_bounds.c
 * already pins its three ways in -- part of the buffer, all of it via a size of
 * 0, all of it via an over-large size -- and each of them leaves the tail
 * zeroed there. What that test cannot see is how *far* the erasure goes, and
 * that is the second half of this file. It enters two words and then looks at
 * the tail, but the tail it looks at has never held anything:
 * sskr_shares_reset() zeroed the whole struct a moment earlier. shrink()
 * promises more than that -- it clears from the new length to the end of the
 * capacity, all SSKR_SHARES_MAX_LENGTH (3664) bytes of it, not just the bytes
 * it dropped -- and the narrower reading, an erasure covering exactly the range
 * that was dropped, leaves the suite as it stands entirely green. The tests
 * below fill the buffer with a stale pattern first, which is the only way that
 * distinction becomes observable at all. bip39_entry_bounds.c plants a sentinel
 * through bip39_mnemonic_get() for the same kind of reason; here it is the
 * whole capacity rather than one byte, because the erasure's reach is the
 * property under test.
 *
 * A note on shape, because the twin invites the wrong expectation: this buffer
 * holds one decoded byte per ByteWord, not text. There is no separator between
 * words, which is why removing a word is shrink(1) here and shrink(length + 1)
 * on the BIP39 side. Read out of sskr_shares_word_add(), not carried over.
 *
 * No defect is fixed here, and nothing under src/ is touched.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * testutils.h has to come first: it defines WIDE, which the ROM variable
 * declarations reached through common_sskr.h are written in terms of. Held in
 * place the way src/nbgl/sskr_shares.c holds its own ordered include.
 */
/* clang-format off */
#include "testutils.h"
#include "sskr/common_sskr.h"
/* clang-format on */

#include "../../../src/nbgl/bip39_mnemonic.h"
#include "../../../src/nbgl/sskr_shares.h"

/*
 * sskr_shares.c also holds sskr_shares_from_bip39_mnemonic() and
 * sskr_shares_check(), which reach into the BIP39 module and the seed
 * comparison, and from there into the os_derive_bip32_no_throw() syscall.
 * Neither is on the removal path under test; these exist only so the object
 * links, and each aborts if it is ever reached.
 */
char* bip39_mnemonic_get(void) { fail_msg("not on the removal path"); }
size_t bip39_mnemonic_length_get(void) { fail_msg("not on the removal path"); }
size_t bip39_mnemonic_final_size_get(void) {
    fail_msg("not on the removal path");
}
bool compare_recovery_phrase(bool* reconstructed) {
    (void)reconstructed;
    fail_msg("not on the removal path");
}

/* Enough words to unwind through several removals; no more than that, since
 * nothing here depends on a share being complete. */
#define ENTERED_WORDS 12

/*
 * Stands in for share bytes the buffer held before the removal under test. Any
 * non-zero value would do; what matters is that it covers the whole capacity,
 * so that an erasure stopping short of the end leaves a trace.
 */
#define STALE_BYTE 0xA5

/* ByteWord whose decoded value is `value`; the table holds 256 four-letter
 * words, so every byte has one. */
static const char* word_for(const uint8_t value) {
    return (const char*)&SSKR_WORDLIST[(size_t)value * SSKR_BYTEWORD_LENGTH];
}

/* Distinct per position, so that a removal that dropped the wrong byte, or
 * kept one byte too many, changes what the head compares against. */
static uint8_t entered_value(const size_t index) {
    return (uint8_t)(0x10 + index);
}

/* Every byte from `from` to the end of the capacity is zero. */
static void assert_erased_from(const size_t from) {
    static const char zeroes[SSKR_SHARES_MAX_LENGTH] = {0};

    assert_true(from <= SSKR_SHARES_MAX_LENGTH);
    assert_memory_equal(&sskr_shares_get()[from], zeroes,
                        SSKR_SHARES_MAX_LENGTH - from);
}

/* The first `length` bytes are exactly the words that were entered. */
static void assert_head_is_the_entered_words(const size_t length) {
    const char* const buffer = sskr_shares_get();

    for (size_t i = 0; i < length; i++) {
        assert_int_equal((uint8_t)buffer[i], entered_value(i));
    }
}

/*
 * Leaves the entry state a reset() gives, with the whole buffer dirty. The
 * struct's own fields are untouched: sskr_shares_get() hands out shares.buffer
 * alone, and SSKR_SHARES_MAX_LENGTH is exactly its declared size.
 */
static void reset_with_a_dirty_buffer(void) {
    sskr_shares_reset();
    memset(sskr_shares_get(), STALE_BYTE, SSKR_SHARES_MAX_LENGTH);
}

/* Adds `count` words through the real entry API, checking as it goes that each
 * one lands as a single byte. */
static void enter_words(const size_t count) {
    for (size_t i = 0; i < count; i++) {
        assert_int_equal(sskr_shares_word_add(word_for(entered_value(i))),
                         i + 1);
        assert_int_equal(sskr_shares_length_get(), i + 1);
    }
}

/*
 * The state sskr_shares_reset() leaves is current_word_index == (size_t)-1, and
 * the guard reading it is the whole of the refusal. Without it the index
 * decrements to (size_t)-2 and sskr_shares_current_word_number_get(), which
 * returns index + 1, starts answering SIZE_MAX -- while shrink(1) is called on
 * an empty buffer.
 */
static void test_word_remove_refuses_an_empty_buffer(void** state) {
    (void)state;

    sskr_shares_reset();

    assert_false(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
    assert_erased_from(0);

    /* Refused again, and still without moving the counter under it. */
    assert_false(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
}

/*
 * The application path, unwound one word at a time. One byte per ByteWord and
 * no separator, so every removal takes the length down by exactly one; the head
 * is compared against the words that were entered at every step, and the tail
 * against zero all the way to the end of the capacity -- which is what the
 * stale pattern underneath makes meaningful.
 */
static void test_removal_unwinds_the_entry_word_by_word(void** state) {
    (void)state;

    reset_with_a_dirty_buffer();
    enter_words(ENTERED_WORDS);
    assert_int_equal(sskr_shares_current_word_number_get(), ENTERED_WORDS);

    for (size_t words = ENTERED_WORDS; words > 0; words--) {
        assert_true(sskr_shares_word_remove());

        assert_int_equal(sskr_shares_current_word_number_get(), words - 1);
        assert_int_equal(sskr_shares_length_get(), words - 1);
        assert_head_is_the_entered_words(words - 1);
        assert_erased_from(words - 1);
    }

    /* Back to the state reset() leaves, by the only test that state has: the
     * next removal is refused. */
    assert_false(sskr_shares_word_remove());
}

/*
 * The boundary the loop above passes through in one step, on its own: a single
 * entered word taken back leaves nothing behind, and asking once more fails
 * without disturbing that.
 */
static void test_removing_the_last_word_returns_to_the_empty_state(
    void** state) {
    (void)state;

    reset_with_a_dirty_buffer();
    enter_words(1);

    assert_true(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
    assert_erased_from(0);

    assert_false(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
    assert_erased_from(0);
}

/*
 * The reach of the erasure, on all three ways into shrink(). Only shrink(1) is
 * reachable from the UI, through word_remove() above; the other two are how the
 * function is documented in sskr_shares.h and are exercised directly. The
 * length each returns is already pinned by sskr_entry_bounds.c -- what is new
 * here is that the pattern sitting past the new length does not survive any of
 * them, out to the last byte of the capacity.
 */
static void test_shrink_erases_to_the_end_of_the_capacity(void** state) {
    (void)state;

    /* Partial: the tail goes, all of it, and the head is left alone. */
    reset_with_a_dirty_buffer();
    enter_words(ENTERED_WORDS);
    assert_int_equal(sskr_shares_shrink(3), ENTERED_WORDS - 3);
    assert_int_equal(sskr_shares_length_get(), ENTERED_WORDS - 3);
    assert_head_is_the_entered_words(ENTERED_WORDS - 3);
    assert_erased_from(ENTERED_WORDS - 3);

    /* size == 0: drop everything. */
    reset_with_a_dirty_buffer();
    enter_words(ENTERED_WORDS);
    assert_int_equal(sskr_shares_shrink(0), 0);
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_erased_from(0);

    /* size > length: drop everything, without wrapping the length around. */
    reset_with_a_dirty_buffer();
    enter_words(ENTERED_WORDS);
    assert_int_equal(sskr_shares_shrink(sskr_shares_length_get() + 1), 0);
    assert_int_equal(sskr_shares_length_get(), 0);
    assert_erased_from(0);

    sskr_shares_reset();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_word_remove_refuses_an_empty_buffer),
        cmocka_unit_test(test_removal_unwinds_the_entry_word_by_word),
        cmocka_unit_test(
            test_removing_the_last_word_returns_to_the_empty_state),
        cmocka_unit_test(test_shrink_erases_to_the_end_of_the_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
