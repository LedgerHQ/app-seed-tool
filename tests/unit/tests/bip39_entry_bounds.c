/*
 * Bounds of the BIP39 mnemonic entry buffer.
 *
 * src/nbgl/bip39_mnemonic.c and src/nbgl/sskr_shares.c do the same job for the
 * two entry modes -- accumulate what the user types, word by word, into a
 * static buffer -- but only the SSKR side had a test target, and only the SSKR
 * side has an explicit bound check in its word_add(). This file gives the BIP39
 * side its first one.
 *
 * No defect is being fixed here. The bounds hold today, and the margin is
 * exactly:
 *
 *   - buffer[]: 24 words of at most BIP39_MAX_WORD_LENGTH (8) characters plus
 *     23 separating spaces is 215 bytes, against a capacity of
 *     BIP39_MNEMONIC_MAX_LENGTH (216). The highest index ever written is 214.
 *     One byte to spare.
 *
 *   - word_lengths[]: after the 24th word current_word_index is 23, the last
 *     valid slot of an array of BIP39_MNEMONIC_SIZE_24 (24) entries. No byte to
 *     spare.
 *
 * Both hold by construction of the caller, not by a guard in word_add():
 * src/nbgl/ui.c calls bip39_mnemonic_final_size_set() with 12, 18 or 24 before
 * opening the keyboard, and consults bip39_mnemonic_complete_check() after
 * every word, so a 25th word is never added along that path. Nothing in
 * bip39_mnemonic.c itself would stop one.
 *
 * That is what makes the invariant worth pinning rather than trusting: a longer
 * word in some future wordlist, a "25 words" option, or a UI path that forgot
 * final_size_set() -- leaving final_size at 0, which makes complete_check()
 * false forever -- would each overflow silently. The tests below are written so
 * that a single byte of drift in any of those directions fails one of them.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../../src/nbgl/bip39_mnemonic.h"
#include "bip39/common_bip39.h"
#include "constants.h"
#include "testutils.h"

/*
 * bip39_mnemonic.c also holds bip39_mnemonic_check(), which reaches into the
 * seed comparison and from there into the os_derive_bip32_no_throw() syscall.
 * It is not on the entry path under test; this exists only so the object links,
 * and aborts if it is ever reached.
 */
bool compare_recovery_phrase(bool* reconstructed) {
    (void)reconstructed;
    fail_msg("not on the entry path");
}

/*
 * bip39_mnemonic_shrink() is external but not declared in bip39_mnemonic.h --
 * word_remove() is its only caller today. Declared here rather than added to
 * the header, which would mean touching src/ for a test's sake.
 */
size_t bip39_mnemonic_shrink(const size_t size);

#define BIP39_WORDLIST_SIZE (BIP39_WORDLIST_OFFSETS_LENGTH - 1)

/*
 * Deliberately far larger than BIP39_MAX_WORD_LENGTH + 1: the scan below exists
 * precisely to catch a wordlist entry longer than the constant claims, and
 * bolos_ux_bip39_idx_strcpy() copies whatever it finds before anything gets a
 * chance to complain. A tight buffer here would smash this file's own stack
 * instead of reporting the mismatch.
 */
#define WORD_SCAN_BUFFER_SIZE 64

/* Longest mnemonic the entry path can ever hold: 24 words at the maximum word
 * length, separated by 23 single spaces. */
#define WORST_CASE_MNEMONIC_LENGTH                    \
    (BIP39_MNEMONIC_SIZE_24 * BIP39_MAX_WORD_LENGTH + \
     (BIP39_MNEMONIC_SIZE_24 - 1))

/* Written to the last byte of the capacity, which nothing should reach. Any
 * value that is not the '\0' left by bip39_mnemonic_reset() would do. */
#define CAPACITY_SENTINEL 0x7f

/* Index of the first wordlist entry that is exactly `length` characters long,
 * or BIP39_WORDLIST_SIZE if the wordlist has none. */
static unsigned int first_word_of_length(const size_t length) {
    unsigned char word[WORD_SCAN_BUFFER_SIZE];

    for (unsigned int index = 0; index < BIP39_WORDLIST_SIZE; index++) {
        if (bolos_ux_bip39_idx_strcpy(index, word) == length) {
            return index;
        }
    }
    return BIP39_WORDLIST_SIZE;
}

/*
 * Copies a real wordlist entry of the requested length into `word` and returns
 * that length, failing the test if the wordlist has no word of that length --
 * the point of feeding the entry path actual words rather than invented strings
 * of the right size is that the test stays representative of what the UI hands
 * it.
 */
static size_t real_word_of_length(const size_t length,
                                  unsigned char word[WORD_SCAN_BUFFER_SIZE]) {
    const unsigned int index = first_word_of_length(length);
    assert_int_not_equal(index, BIP39_WORDLIST_SIZE);

    const size_t copied = bolos_ux_bip39_idx_strcpy(index, word);
    assert_int_equal(copied, length);
    return copied;
}

/* Every byte from `from` up to the end of the capacity is zero. */
static void assert_erased_from(const size_t from) {
    static const char zeroes[BIP39_MNEMONIC_MAX_LENGTH] = {0};

    assert_true(from <= BIP39_MNEMONIC_MAX_LENGTH);
    assert_memory_equal(&bip39_mnemonic_get()[from], zeroes,
                        BIP39_MNEMONIC_MAX_LENGTH - from);
}

/*
 * The premise every bound below rests on. BIP39_MAX_WORD_LENGTH is what sizes
 * the mnemonic buffer, but nothing checks it against the wordlist that is
 * actually shipped, so a longer word added to that list would quietly turn the
 * one spare byte into an overflow. Measured here against the real data rather
 * than assumed.
 */
static void test_wordlist_matches_the_assumed_word_length(void** state) {
    (void)state;

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    size_t longest = 0;

    for (unsigned int index = 0; index < BIP39_WORDLIST_SIZE; index++) {
        const size_t length = bolos_ux_bip39_idx_strcpy(index, word);
        assert_int_not_equal(length, 0);
        if (length > longest) {
            longest = length;
        }
    }

    assert_int_equal(longest, BIP39_MAX_WORD_LENGTH);
    /* ...and the capacity is still derived from it the way the header says. */
    assert_int_equal(BIP39_MNEMONIC_MAX_LENGTH,
                     BIP39_MNEMONIC_SIZE_24 * (BIP39_MAX_WORD_LENGTH + 1));
    /* One byte to spare, and no more than one. */
    assert_int_equal(WORST_CASE_MNEMONIC_LENGTH, BIP39_MNEMONIC_MAX_LENGTH - 1);
}

/*
 * The worst case the entry path can produce, fed one word at a time exactly as
 * the keyboard does it. A sentinel sits on the last byte of the capacity: an
 * overflow of a single byte overwrites it, which is the whole point of the
 * test.
 */
static void test_worst_case_entry_leaves_the_last_byte_untouched(void** state) {
    (void)state;

    bip39_mnemonic_reset();

    char* const buffer = bip39_mnemonic_get();
    /* reset() has just zeroed the entire struct, so this is the only non-zero
     * byte in the buffer when the first word arrives. */
    buffer[BIP39_MNEMONIC_MAX_LENGTH - 1] = CAPACITY_SENTINEL;

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    const size_t word_length = real_word_of_length(BIP39_MAX_WORD_LENGTH, word);

    char expected[BIP39_MNEMONIC_MAX_LENGTH] = {0};
    size_t expected_length = 0;

    for (size_t words = 0; words < BIP39_MNEMONIC_SIZE_24; words++) {
        if (words > 0) {
            expected[expected_length++] = ' ';
        }
        memcpy(&expected[expected_length], word, word_length);
        expected_length += word_length;

        assert_int_equal(
            bip39_mnemonic_word_add((const char*)word, word_length), words + 1);
        assert_int_equal(bip39_mnemonic_length_get(), expected_length);
    }

    assert_int_equal(expected_length, WORST_CASE_MNEMONIC_LENGTH);
    assert_memory_equal(buffer, expected, expected_length);

    /* The 24th word fills word_lengths[23], its last slot. */
    assert_int_equal(bip39_mnemonic_current_word_number_get(),
                     BIP39_MNEMONIC_SIZE_24);

    /*
     * Nothing reached the last byte of the capacity. Note what this also says:
     * a full-length mnemonic is not NUL-terminated -- word_add() only writes
     * the terminator ahead of the *next* word -- so every consumer has to go
     * through bip39_mnemonic_length_get(). Today they all do.
     */
    assert_int_equal((unsigned char)buffer[BIP39_MNEMONIC_MAX_LENGTH - 1],
                     CAPACITY_SENTINEL);
    /* And a write that had folded back onto the length counter, which sits
     * immediately after the buffer in the struct, would show up here. */
    assert_int_equal(bip39_mnemonic_length_get(), WORST_CASE_MNEMONIC_LENGTH);
}

/*
 * word_lengths[] is only ever read back by word_remove(). Feeding 24 words of
 * deliberately differing lengths and unwinding them one at a time checks every
 * slot up to the last: a length read from the wrong slot shrinks the buffer by
 * the wrong amount, and the running length below stops matching.
 */
static void test_word_lengths_track_every_slot_up_to_the_last(void** state) {
    (void)state;

    /* The full spread the English wordlist offers, cycled so that the 24th
     * word -- the one landing in the last slot -- is a longest one. */
    static const size_t pattern[] = {3, 4, 5, 6, 7, 8};

    bip39_mnemonic_reset();

    /* length_after[n] is the buffer length once n words have been entered. */
    size_t length_after[BIP39_MNEMONIC_SIZE_24 + 1];
    length_after[0] = 0;

    for (size_t words = 0; words < BIP39_MNEMONIC_SIZE_24; words++) {
        unsigned char word[WORD_SCAN_BUFFER_SIZE];
        const size_t word_length = real_word_of_length(
            pattern[words % (sizeof(pattern) / sizeof(pattern[0]))], word);

        assert_int_equal(
            bip39_mnemonic_word_add((const char*)word, word_length), words + 1);

        length_after[words + 1] =
            length_after[words] + word_length + (words > 0 ? 1 : 0);
        assert_int_equal(bip39_mnemonic_length_get(), length_after[words + 1]);
    }

    assert_int_equal(bip39_mnemonic_current_word_number_get(),
                     BIP39_MNEMONIC_SIZE_24);

    for (size_t words = BIP39_MNEMONIC_SIZE_24; words > 0; words--) {
        assert_true(bip39_mnemonic_word_remove());
        assert_int_equal(bip39_mnemonic_current_word_number_get(), words - 1);

        /*
         * Removing the first word also takes the separator that was never
         * written in front of it, so the length bottoms out at 0 rather than
         * going negative -- shrink() clamps it. Every other removal lands
         * exactly on the length the buffer had before that word arrived.
         */
        const size_t expected = words > 1 ? length_after[words - 1] : 0;
        assert_int_equal(bip39_mnemonic_length_get(), expected);
        assert_erased_from(expected);
    }

    assert_int_equal(bip39_mnemonic_length_get(), 0);
}

/* Removing from an empty buffer is refused, and changes nothing. */
static void test_word_remove_refuses_an_empty_buffer(void** state) {
    (void)state;

    bip39_mnemonic_reset();

    assert_false(bip39_mnemonic_word_remove());
    assert_int_equal(bip39_mnemonic_length_get(), 0);
    assert_int_equal(bip39_mnemonic_current_word_number_get(), 0);
    assert_erased_from(0);
}

/*
 * Both branches of shrink(), and in each case the erasure past the new length.
 * That memzero is how an abandoned mnemonic stops being readable in RAM; it is
 * checked dynamically on the Speculos side, never here until now.
 */
static void test_shrink_erases_everything_past_the_new_length(void** state) {
    (void)state;

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    const size_t word_length = real_word_of_length(BIP39_MAX_WORD_LENGTH, word);

    /* Partial removal: the tail is erased, the head is left alone. */
    bip39_mnemonic_reset();
    bip39_mnemonic_word_add((const char*)word, word_length);
    bip39_mnemonic_word_add((const char*)word, word_length);
    const size_t two_words = bip39_mnemonic_length_get();
    assert_int_equal(two_words, 2 * word_length + 1);

    assert_int_equal(bip39_mnemonic_shrink(word_length + 1), word_length);
    assert_int_equal(bip39_mnemonic_length_get(), word_length);
    assert_memory_equal(bip39_mnemonic_get(), word, word_length);
    assert_erased_from(word_length);

    /* size == 0: erase all. */
    assert_int_equal(bip39_mnemonic_shrink(0), 0);
    assert_erased_from(0);

    /* size > length: erase all, without wrapping the length around. */
    bip39_mnemonic_reset();
    bip39_mnemonic_word_add((const char*)word, word_length);
    assert_int_equal(bip39_mnemonic_shrink(bip39_mnemonic_length_get() + 1), 0);
    assert_int_equal(bip39_mnemonic_length_get(), 0);
    assert_erased_from(0);
}

/* reset() leaves no trace of a phrase that was entered before it. */
static void test_reset_clears_a_previously_entered_phrase(void** state) {
    (void)state;

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    const size_t word_length = real_word_of_length(BIP39_MAX_WORD_LENGTH, word);

    bip39_mnemonic_reset();
    bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_12);
    for (size_t words = 0; words < BIP39_MNEMONIC_SIZE_12; words++) {
        bip39_mnemonic_word_add((const char*)word, word_length);
    }
    assert_true(bip39_mnemonic_length_get() > 0);
    assert_memory_equal(bip39_mnemonic_get(), word, word_length);

    bip39_mnemonic_reset();

    assert_int_equal(bip39_mnemonic_length_get(), 0);
    assert_int_equal(bip39_mnemonic_current_word_number_get(), 0);
    /* current_word_index back to (size_t)-1, which is what makes the next
     * word_add() skip the leading separator. */
    assert_false(bip39_mnemonic_word_remove());
    /* The expected size is part of the struct, so it goes too. */
    assert_int_equal(bip39_mnemonic_final_size_get(), 0);
    assert_erased_from(0);
}

/*
 * complete_check() is the only thing standing between the keyboard and a 25th
 * word. The final_size == 0 case is the one that matters for the bounds above:
 * a UI path reaching the keyboard without final_size_set() gets a check that is
 * false no matter how many words have been entered.
 */
static void test_complete_check_never_completes_without_a_final_size(
    void** state) {
    (void)state;

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    const size_t word_length = real_word_of_length(BIP39_MAX_WORD_LENGTH, word);

    bip39_mnemonic_reset();
    assert_int_equal(bip39_mnemonic_final_size_get(), 0);
    assert_false(bip39_mnemonic_complete_check());

    for (size_t words = 0; words < BIP39_MNEMONIC_SIZE_24; words++) {
        bip39_mnemonic_word_add((const char*)word, word_length);
        assert_false(bip39_mnemonic_complete_check());
    }
}

/* The three sizes the UI actually sets, each reached at its own word count. */
static void test_complete_check_at_each_supported_size(void** state) {
    (void)state;

    static const size_t sizes[] = {
        BIP39_MNEMONIC_SIZE_12, BIP39_MNEMONIC_SIZE_18, BIP39_MNEMONIC_SIZE_24};

    unsigned char word[WORD_SCAN_BUFFER_SIZE];
    const size_t word_length = real_word_of_length(BIP39_MAX_WORD_LENGTH, word);

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        const size_t final_size = sizes[i];

        bip39_mnemonic_reset();
        bip39_mnemonic_final_size_set(final_size);
        assert_int_equal(bip39_mnemonic_final_size_get(), final_size);

        for (size_t words = 1; words <= final_size; words++) {
            bip39_mnemonic_word_add((const char*)word, word_length);
            if (words < final_size) {
                assert_false(bip39_mnemonic_complete_check());
            } else {
                assert_true(bip39_mnemonic_complete_check());
            }
        }

        /* Stepping back below the expected count reopens the entry. */
        assert_true(bip39_mnemonic_word_remove());
        assert_false(bip39_mnemonic_complete_check());
    }

    bip39_mnemonic_reset();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wordlist_matches_the_assumed_word_length),
        cmocka_unit_test(test_worst_case_entry_leaves_the_last_byte_untouched),
        cmocka_unit_test(test_word_lengths_track_every_slot_up_to_the_last),
        cmocka_unit_test(test_word_remove_refuses_an_empty_buffer),
        cmocka_unit_test(test_shrink_erases_everything_past_the_new_length),
        cmocka_unit_test(test_reset_clears_a_previously_entered_phrase),
        cmocka_unit_test(
            test_complete_check_never_completes_without_a_final_size),
        cmocka_unit_test(test_complete_check_at_each_supported_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
