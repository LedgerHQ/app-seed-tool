/*
 * The SSKR keyboard suggestions: bolos_ux_sskr_fill_with_candidates() and
 * bolos_ux_sskr_get_keyboard_mask(), the two functions behind
 * `#if defined(HAVE_NBGL)` at the end of src/common/sskr/seed_sskr.c.
 *
 * Until this file existed, no test target defined HAVE_NBGL, so the
 * preprocessor removed both of them from every build the suite made -- and
 * lcov did not report a gap, because lines the preprocessor removes are
 * neither covered nor uncovered: they do not exist for the tool. seed_sskr.c
 * showed a healthy 96.2%, computed over lines 52-486 of a 542-line file. The
 * DA: entries in coverage.info were the only place the hole was visible.
 *
 * Both functions are pure -- a typed prefix in, candidate words or a keyboard
 * mask out -- so nothing here needs a screen or a syscall. The NBGL header
 * they include is stubbed in lib/nbgl_layout.h for the single constant they
 * read from it; see that file for where its value comes from and why this test
 * is compiled twice, once per live value.
 *
 * tests/bip39_keyboard_candidates.c is the twin of this file. The two
 * wordlists differ in shape -- 256 fixed-width ByteWords here, 2048
 * variable-length words there, reached through an offset table -- so each file
 * carries its own oracle, but the properties asserted are the same.
 */

#include <cmocka.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Two ordering constraints, hence the clang-format exclusion: testutils.h has
 * to come first because it defines WIDE, which sskr/seed_rom_variables.h uses
 * without defining, and cx.h has to precede common.h, which declares
 * compare_recovery_phrase_finish() as taking a cx_err_t. */
// clang-format off
#include "testutils.h"
#include <cx.h>
#include "common.h"
#include "sskr/common_sskr.h"
#include "sskr/seed_rom_variables.h"
#include <nbgl_layout.h>
// clang-format on

#define SSKR_WORD_COUNT (SSKR_WORDLIST_LENGTH / SSKR_BYTEWORD_LENGTH)

/* The keypad's own key, always left enabled so a wrong letter can be undone.
 * seed_sskr.c writes it as `1 << 28` with no name. */
#define BACK_KEY_BIT (UINT32_C(1) << 28)

/* Longest prefix the sweep below enumerates over the whole alphabet. Above
 * this it enumerates prefixes taken from the wordlist instead: 26^4 prefixes
 * would be 457k calls for the 456k of them that match nothing. */
#define MAX_SWEPT_LENGTH 3

/* Enough for any prefix either half of the sweep produces, plus a terminator.
 * ByteWords are all SSKR_BYTEWORD_LENGTH long, and one character past that is
 * swept too so that an over-long prefix is covered. */
#define PREFIX_CAPACITY (SSKR_BYTEWORD_LENGTH + 2)

static const char kbd_letters[] = KBD_LETTERS;

/* ---------------------------------------------------------------- oracles */

/* The wordlist read directly, not through the functions under test. Both
 * oracles below walk SSKR_WORDLIST themselves so that a defect in
 * bolos_ux_sskr_get_word_count_starting_with() and friends cannot cancel out
 * against the same defect in the expectation. */

static size_t oracle_match_count(const char* prefix, size_t prefix_length) {
    if (prefix_length > SSKR_BYTEWORD_LENGTH) {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 0; i < SSKR_WORD_COUNT; i++) {
        if (memcmp(&SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i], prefix,
                   prefix_length) == 0) {
            count++;
        }
    }
    return count;
}

/* Index of the first matching word, or SSKR_WORD_COUNT if there is none. */
static size_t oracle_first_match(const char* prefix, size_t prefix_length) {
    if (prefix_length > SSKR_BYTEWORD_LENGTH) {
        return SSKR_WORD_COUNT;
    }
    for (size_t i = 0; i < SSKR_WORD_COUNT; i++) {
        if (memcmp(&SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i], prefix,
                   prefix_length) == 0) {
            return i;
        }
    }
    return SSKR_WORD_COUNT;
}

/* The set of letters that extend `prefix` into a word, as a bitmask over
 * KBD_LETTERS positions. Built with |=, deliberately: the function under test
 * uses += (see test_the_mask_matches_the_wordlist_for_every_prefix). */
static uint32_t oracle_next_letter_bits(const char* prefix,
                                        size_t prefix_length) {
    uint32_t bits = 0;
    if (prefix_length >= SSKR_BYTEWORD_LENGTH) {
        /* A full-length ByteWord has no next letter, and neither has anything
         * longer, which matches nothing at all. */
        return bits;
    }
    for (size_t i = 0; i < SSKR_WORD_COUNT; i++) {
        const unsigned char* const word =
            &SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i];
        if (memcmp(word, prefix, prefix_length) != 0) {
            continue;
        }
        const char next = (char)word[prefix_length];
        /* strchr() would find a '\0' at the end of KBD_LETTERS and report
         * position 26, one past the 26 letters; the wordlist holds no such
         * character and this says so rather than assuming it. */
        assert_true(next != '\0');
        const char* const position = strchr(kbd_letters, next);
        assert_non_null(position);
        bits |= UINT32_C(1) << (size_t)(position - kbd_letters);
    }
    return bits;
}

static uint32_t oracle_keyboard_mask(const char* prefix, size_t prefix_length) {
    return ~(BACK_KEY_BIT | oracle_next_letter_bits(prefix, prefix_length));
}

static size_t oracle_next_letter_count(const char* prefix,
                                       size_t prefix_length) {
    uint32_t bits = oracle_next_letter_bits(prefix, prefix_length);
    size_t count = 0;
    while (bits != 0) {
        count += bits & 1u;
        bits >>= 1;
    }
    return count;
}

/* ------------------------------------------------------------------ sweep */

/* Every prefix of length 0 to MAX_SWEPT_LENGTH over the 26 keyboard letters,
 * then every prefix of every ByteWord, then every ByteWord with one extra
 * letter appended -- prefixes longer than a word, which the entry code can
 * produce since it does not stop the user at four characters. */
static void sweep_prefixes(void (*check)(const char* prefix,
                                         size_t prefix_length)) {
    char prefix[PREFIX_CAPACITY];

    for (size_t length = 0; length <= MAX_SWEPT_LENGTH; length++) {
        size_t combinations = 1;
        for (size_t i = 0; i < length; i++) {
            combinations *= 26;
        }
        for (size_t combination = 0; combination < combinations;
             combination++) {
            size_t remainder = combination;
            for (size_t position = length; position-- > 0;) {
                prefix[position] = kbd_letters[remainder % 26];
                remainder /= 26;
            }
            prefix[length] = '\0';
            check(prefix, length);
        }
    }

    for (size_t i = 0; i < SSKR_WORD_COUNT; i++) {
        memcpy(prefix, &SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i],
               SSKR_BYTEWORD_LENGTH);
        for (size_t length = MAX_SWEPT_LENGTH + 1;
             length <= SSKR_BYTEWORD_LENGTH; length++) {
            prefix[length] = '\0';
            check(prefix, length);
        }
        prefix[SSKR_BYTEWORD_LENGTH] = 'a';
        prefix[SSKR_BYTEWORD_LENGTH + 1] = '\0';
        check(prefix, SSKR_BYTEWORD_LENGTH + 1);
    }
}

/* ------------------------------------------- the candidate suggestions */

/* Buffers sized exactly to what the correct behaviour writes, on the heap so
 * that AddressSanitizer's redzones make a single byte past the end a failure
 * in the ASan configuration of .github/workflows/unit-tests-matrix.yml, and
 * not only a wrong return value in the others. src/nbgl/ui.c hands over
 * `wordCandidates[(BIP39_MAX_WORD_LENGTH + 1) * NB_MAX_SUGGESTION_BUTTONS]`
 * and `buttonTexts[NB_MAX_SUGGESTION_BUTTONS]`; nothing here relies on that
 * being roomier than SSKR needs. */
struct candidate_buffers {
    char* words;
    size_t words_size;
    const char** indexor;
    size_t indexor_count;
};

static struct candidate_buffers alloc_candidate_buffers(
    size_t expected_candidates) {
    struct candidate_buffers buffers;
    buffers.indexor_count = expected_candidates;
    buffers.words_size = expected_candidates * (SSKR_BYTEWORD_LENGTH + 1);
    /* calloc(0, ...) may return NULL; one spare byte keeps the no-match case
     * pointing at real memory so that "the buffer was not written" is a
     * statement about a buffer that exists. */
    buffers.words = calloc(buffers.words_size + 1, 1);
    buffers.indexor = calloc(expected_candidates + 1, sizeof(*buffers.indexor));
    assert_non_null(buffers.words);
    assert_non_null(buffers.indexor);
    return buffers;
}

static void free_candidate_buffers(struct candidate_buffers* buffers) {
    free(buffers->words);
    free((void*)buffers->indexor);
    buffers->words = NULL;
    buffers->indexor = NULL;
}

static void test_no_match_returns_zero_and_writes_nothing(void** state) {
    (void)state;

    /* No ByteWord starts with "zz". */
    const char prefix[] = "zz";
    assert_int_equal(oracle_match_count(prefix, strlen(prefix)), 0);

    struct candidate_buffers buffers =
        alloc_candidate_buffers(NB_MAX_SUGGESTION_BUTTONS);
    memset(buffers.words, 0xA5, buffers.words_size);
    for (size_t i = 0; i < buffers.indexor_count; i++) {
        buffers.indexor[i] = (const char*)(uintptr_t)0xA5A5A5A5u;
    }

    const size_t returned = bolos_ux_sskr_fill_with_candidates(
        (const unsigned char*)prefix, strlen(prefix), buffers.words,
        buffers.indexor);

    assert_int_equal(returned, 0);
    for (size_t i = 0; i < buffers.words_size; i++) {
        if ((unsigned char)buffers.words[i] != 0xA5) {
            fail_msg(
                "byte %zu of the candidate buffer was written for a "
                "prefix that matches no ByteWord",
                i);
        }
    }
    for (size_t i = 0; i < buffers.indexor_count; i++) {
        if (buffers.indexor[i] != (const char*)(uintptr_t)0xA5A5A5A5u) {
            fail_msg(
                "entry %zu of the indexor was written for a prefix that "
                "matches no ByteWord",
                i);
        }
    }

    free_candidate_buffers(&buffers);
}

static void test_a_single_match_is_copied_and_terminated(void** state) {
    (void)state;

    /* "able" is the only ByteWord starting with "ab". */
    const char prefix[] = "ab";
    assert_int_equal(oracle_match_count(prefix, strlen(prefix)), 1);

    struct candidate_buffers buffers = alloc_candidate_buffers(1);

    const size_t returned = bolos_ux_sskr_fill_with_candidates(
        (const unsigned char*)prefix, strlen(prefix), buffers.words,
        buffers.indexor);

    assert_int_equal(returned, 1);
    assert_string_equal(buffers.words, "able");
    /* The word is NUL-terminated inside the buffer, not merely followed by
     * one: the byte at index SSKR_BYTEWORD_LENGTH is the terminator. */
    assert_int_equal(buffers.words[SSKR_BYTEWORD_LENGTH], '\0');
    /* And the indexor points at it, rather than anywhere else that happens to
     * hold the same characters. */
    assert_ptr_equal(buffers.indexor[0], buffers.words);

    free_candidate_buffers(&buffers);
}

static void test_more_matches_than_buttons_are_capped(void** state) {
    (void)state;

    /* 15 ByteWords start with "f", more than either live value of
     * NB_MAX_SUGGESTION_BUTTONS (12 on Stax, 8 on Flex and Apex).
     *
     * This is a one-character prefix, which src/nbgl/ui.c never passes: it
     * calls this function only once the entered text reaches two characters,
     * and no two-character prefix matches more than 6 ByteWords -- so on this
     * wordlist the cap is never reached through the interface. It is still
     * the one line of this function that memory safety depends on, since it
     * is what bounds the writes into the caller's two buffers, and the
     * function is a public entry point of common_sskr.h. It is held here on
     * the shortest prefix that reaches it. */
    const char prefix[] = "f";
    const size_t matches = oracle_match_count(prefix, strlen(prefix));
    assert_true(matches > NB_MAX_SUGGESTION_BUTTONS);

    struct candidate_buffers buffers =
        alloc_candidate_buffers(NB_MAX_SUGGESTION_BUTTONS);

    const size_t returned = bolos_ux_sskr_fill_with_candidates(
        (const unsigned char*)prefix, strlen(prefix), buffers.words,
        buffers.indexor);

    assert_int_equal(returned, NB_MAX_SUGGESTION_BUTTONS);
    /* The spare slot past the cap is untouched, in the build configurations
     * where ASan is not watching the redzone for us. */
    assert_null(buffers.indexor[NB_MAX_SUGGESTION_BUTTONS]);
    assert_int_equal(buffers.words[buffers.words_size], '\0');

    free_candidate_buffers(&buffers);
}

static void test_the_buffer_holds_consecutive_nul_separated_words(
    void** state) {
    (void)state;

    /* Six ByteWords start with "wa", the widest two-character prefix there
     * is, and the entry code reaches it. Checking the count alone would let
     * an offset that drifts by one word go unnoticed, so every candidate is
     * checked against the wordlist entry it should be, at the offset it
     * should be at. */
    const char prefix[] = "wa";
    const size_t prefix_length = strlen(prefix);
    const size_t matches = oracle_match_count(prefix, prefix_length);
    assert_true(matches > 1);
    assert_true(matches <= NB_MAX_SUGGESTION_BUTTONS);

    struct candidate_buffers buffers = alloc_candidate_buffers(matches);

    const size_t returned = bolos_ux_sskr_fill_with_candidates(
        (const unsigned char*)prefix, prefix_length, buffers.words,
        buffers.indexor);

    assert_int_equal(returned, matches);

    const size_t first = oracle_first_match(prefix, prefix_length);
    for (size_t i = 0; i < returned; i++) {
        const size_t offset = i * (SSKR_BYTEWORD_LENGTH + 1);
        const unsigned char* const expected =
            &SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * (first + i)];

        /* Consecutive, one after the other, each ending in its own '\0'. */
        if (memcmp(&buffers.words[offset], expected, SSKR_BYTEWORD_LENGTH) !=
            0) {
            fail_msg("candidate %zu is '%.4s', expected '%.4s'", i,
                     &buffers.words[offset], (const char*)expected);
        }
        assert_int_equal(buffers.words[offset + SSKR_BYTEWORD_LENGTH], '\0');
        /* And the indexor entry points inside the buffer the caller supplied,
         * at that word. */
        assert_ptr_equal(buffers.indexor[i], &buffers.words[offset]);
    }

    free_candidate_buffers(&buffers);
}

static void test_the_empty_prefix_returns_the_head_of_the_wordlist(
    void** state) {
    (void)state;

    /* Not reachable through the interface: src/nbgl/ui.c calls this function
     * only when the entered text is two characters or more ("Suggestions only
     * when the word contains 2+ letters"), and the mask function only when it
     * is at least one. Established rather than assumed, and pinned here
     * anyway, because the function is public and an empty prefix is the case
     * where every word matches -- the widest input the cap has to hold. */
    const char prefix[] = "";
    assert_int_equal(oracle_match_count(prefix, 0), SSKR_WORD_COUNT);

    struct candidate_buffers buffers =
        alloc_candidate_buffers(NB_MAX_SUGGESTION_BUTTONS);

    const size_t returned = bolos_ux_sskr_fill_with_candidates(
        (const unsigned char*)prefix, 0, buffers.words, buffers.indexor);

    assert_int_equal(returned, NB_MAX_SUGGESTION_BUTTONS);
    for (size_t i = 0; i < returned; i++) {
        const size_t offset = i * (SSKR_BYTEWORD_LENGTH + 1);
        if (memcmp(&buffers.words[offset],
                   &SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i],
                   SSKR_BYTEWORD_LENGTH) != 0) {
            fail_msg(
                "candidate %zu of the empty prefix is '%.4s', expected "
                "the wordlist's own word %zu, '%.4s'",
                i, &buffers.words[offset], i,
                (const char*)&SSKR_WORDLIST[SSKR_BYTEWORD_LENGTH * i]);
        }
    }

    free_candidate_buffers(&buffers);
}

/* ------------------------------------------------- the keyboard mask */

static void check_mask(const char* prefix, size_t prefix_length) {
    const uint32_t returned = bolos_ux_sskr_get_keyboard_mask(
        (const unsigned char*)prefix, (unsigned int)prefix_length);
    const uint32_t expected = oracle_keyboard_mask(prefix, prefix_length);
    if (returned != expected) {
        fail_msg("prefix '%s': mask is 0x%08" PRIX32 ", expected 0x%08" PRIX32
                 " (differing bits 0x%08" PRIX32 ")",
                 prefix, returned, expected, returned ^ expected);
    }
}

static void test_the_mask_matches_the_wordlist_for_every_prefix(void** state) {
    (void)state;

    /* Exact in both directions, on every prefix the sweep produces: a letter
     * is enabled if and only if it extends some ByteWord. Inclusion alone
     * would pass a mask that enables the whole alphabet.
     *
     * This is also what holds the `existing_mask += 1 << i` in seed_sskr.c,
     * which is an addition where a bitwise or would be the natural thing to
     * write. The two agree only while no letter is offered twice: a second
     * `+= 1 << i` for the same i carries into bit i+1, enabling the wrong
     * key. bolos_ux_sskr_get_word_next_letters_starting_with() cannot produce
     * that duplicate, because it compares each candidate letter against the
     * one it wrote last and the wordlist is alphabetically ordered, which
     * makes equal next-letters contiguous. That reasoning is what this
     * exhaustive comparison against an oracle built with |= turns into
     * something the suite checks rather than something a reader has to
     * accept: any duplicate, on any prefix, moves a bit and shows up here. */
    sweep_prefixes(check_mask);
}

static void test_the_back_key_is_always_enabled(void** state) {
    (void)state;

    /* seed_sskr.c seeds the mask with `1 << 28` before adding letters, and
     * the caller reads the result as the set of *disabled* keys, so the bit
     * is clear in every mask this function returns -- including for a prefix
     * that matches nothing, where it is the only key left to press. */
    static const char* const prefixes[] = {"",   "a",    "ab",
                                           "zz", "able", "ablea"};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        const uint32_t mask =
            bolos_ux_sskr_get_keyboard_mask((const unsigned char*)prefixes[i],
                                            (unsigned int)strlen(prefixes[i]));
        if ((mask & BACK_KEY_BIT) != 0) {
            fail_msg("prefix '%s': the back key is disabled (mask 0x%08" PRIX32
                     ")",
                     prefixes[i], mask);
        }
    }
}

static void test_a_dead_end_prefix_disables_every_letter(void** state) {
    (void)state;

    /* No ByteWord starts with "zz", so no letter can extend it. */
    assert_int_equal(oracle_next_letter_count("zz", 2), 0);
    assert_int_equal(
        bolos_ux_sskr_get_keyboard_mask((const unsigned char*)"zz", 2),
        ~BACK_KEY_BIT);

    /* A complete ByteWord is a dead end too: there is no fifth letter. */
    assert_int_equal(oracle_next_letter_count("able", 4), 0);
    assert_int_equal(
        bolos_ux_sskr_get_keyboard_mask((const unsigned char*)"able", 4),
        ~BACK_KEY_BIT);
}

static void test_the_returned_mask_is_the_disabled_keys(void** state) {
    (void)state;

    /* One frozen value, so that the final `-1 ^ existing_mask` in
     * seed_sskr.c has something holding it: what the function returns is the
     * complement of what it computed, the keys to grey out rather than the
     * keys to offer. Everything above is written against the oracle and would
     * follow the convention if it flipped; this would not.
     *
     * "fl" is extended by exactly three ByteWords -- flap, flew, flux -- so
     * a, e and u are the letters to leave enabled. In KBD_LETTERS
     * ("qwertyuiopasdfghjklzxcvbnm") those sit at positions 10, 2 and 6, and
     * with bit 28 for the back key the enabled set is 0x10000444. The
     * function returns its complement. */
    assert_int_equal(oracle_next_letter_count("fl", 2), 3);
    assert_int_equal(
        bolos_ux_sskr_get_keyboard_mask((const unsigned char*)"fl", 2),
        0xEFFFFBBB);
}

/* --------------------------------------------- the next-letter buffer */

static size_t widest_next_letter_count;

static void check_next_letters(const char* prefix, size_t prefix_length) {
    /* Exactly the array bolos_ux_sskr_get_keyboard_mask() declares, on the
     * heap so ASan sees a write past it. That function then writes
     * `next_letters[nb_letters] = '\0'`, so a count of ALPHABET_LENGTH would
     * already be one too many. */
    unsigned char* const next_letters = calloc(ALPHABET_LENGTH, 1);
    assert_non_null(next_letters);

    const size_t count = bolos_ux_sskr_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, (unsigned int)prefix_length,
        next_letters);

    if (count >= ALPHABET_LENGTH) {
        fail_msg(
            "prefix '%s': %zu next letters, which leaves no room for "
            "the terminator get_keyboard_mask() writes at index %zu of a "
            "%d-byte array",
            prefix, count, count, ALPHABET_LENGTH);
    }
    /* The write that the caller makes right after, at the index this function
     * reported. Out of bounds by one byte and ASan says so. */
    next_letters[count] = '\0';

    assert_int_equal(count, oracle_next_letter_count(prefix, prefix_length));
    for (size_t i = 0; i < count; i++) {
        /* No letter read back as '\0'. The mask loop runs i over the full
         * ALPHABET_LENGTH, one past the 26 letters of KBD_LETTERS, so it
         * compares against that string's terminator once; a '\0' among the
         * next letters would match it and enable bit 26, a key that is not on
         * the keyboard. */
        if (next_letters[i] == '\0') {
            fail_msg("prefix '%s': next letter %zu is a NUL", prefix, i);
        }
        assert_non_null(strchr(kbd_letters, next_letters[i]));
    }

    if (count > widest_next_letter_count) {
        widest_next_letter_count = count;
    }

    free(next_letters);
}

static void test_next_letters_never_fills_its_array(void** state) {
    (void)state;

    /* `unsigned char next_letters[ALPHABET_LENGTH]` with ALPHABET_LENGTH 27
     * (src/common/common.h), filled by this function and then terminated by
     * its caller at the returned index. The margin is not obvious from the
     * declaration -- 27 for a 26-letter keyboard -- so it is measured here
     * over every prefix in the sweep rather than argued.
     *
     * The widest case is the empty prefix, which offers the first letter of
     * all 256 ByteWords: 25 distinct letters, every letter except x. That
     * leaves index 25 for the terminator and one byte spare. */
    widest_next_letter_count = 0;
    sweep_prefixes(check_next_letters);
    assert_int_equal(widest_next_letter_count, 25);
    assert_int_equal(oracle_next_letter_count("", 0), 25);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_no_match_returns_zero_and_writes_nothing),
        cmocka_unit_test(test_a_single_match_is_copied_and_terminated),
        cmocka_unit_test(test_more_matches_than_buttons_are_capped),
        cmocka_unit_test(test_the_buffer_holds_consecutive_nul_separated_words),
        cmocka_unit_test(
            test_the_empty_prefix_returns_the_head_of_the_wordlist),
        cmocka_unit_test(test_the_mask_matches_the_wordlist_for_every_prefix),
        cmocka_unit_test(test_the_back_key_is_always_enabled),
        cmocka_unit_test(test_a_dead_end_prefix_disables_every_letter),
        cmocka_unit_test(test_the_returned_mask_is_the_disabled_keys),
        cmocka_unit_test(test_next_letters_never_fills_its_array),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
