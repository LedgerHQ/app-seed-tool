/*
 * The contents of SSKR_WORDLIST, checked against BCR-2020-012 rather than
 * against itself.
 *
 * src/common/sskr/seed_rom_variables.c holds the 256 ByteWords of
 * BCR-2020-012 concatenated without separators. Every SSKR share the device
 * shows and every share a user types back in passes through this table, so it
 * is the datum the whole of SSKR interoperability rests on -- and it is data,
 * not code: it has no executable line, so gcov emits no DA: entry for it. It
 * is compiled into 26 unit targets and appears in no lcov report at all --
 * not at 0%, simply absent. Compiled is not checked.
 *
 * Two files carry ByteWords string literals: roundtrip.c (94 distinct words)
 * and sskr_interop_bc128_bytewords.c (55), 129 of the 256 between them. No
 * ByteWords vector anywhere in the suite covers the other 127. (Eleven of
 * those 127 do occur in unrelated literals -- BIP-39 words, keyboard prefixes
 * -- which says nothing about their value as ByteWords; the remaining 116
 * appear nowhere.)
 *
 * But those 129 are pinned only against themselves. Both literals were
 * produced with this very table -- roundtrip.c stores the shares this port
 * emits under a fake RNG, and sskr_interop_bc128_bytewords.c takes shard bytes
 * from bc-sskr but, as its own header says, encodes them "into ByteWords with
 * this repository's own SSKR_WORDLIST table". So they catch a change to the
 * table, and nothing more: had a word been wrong from the day the table was
 * written, the expected strings would have been generated wrong to match, and
 * both would still pass. Neither is an oracle for the table; both are
 * regression pins over half of it.
 *
 * Measured, on the tree this file was added to: altering 0x80 "lava" to "lavb"
 * fails those two targets, because "lava" occurs in both literals; altering
 * 0x06 "atom" to "atob" leaves the whole suite green.
 *
 * What closes this is not another round trip -- a test that encodes and
 * decodes with the same table substitutes the same word on both sides and
 * cancels the error out. It is a comparison against the published word list,
 * which is why the transcription below has to come from the document rather
 * than from the repository. Copying the table out of seed_rom_variables.c
 * would reproduce exactly the circularity described above, invisibly.
 *
 * Oracle: BCR-2020-012, "Word List".
 * https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-012-bytewords.md
 *
 * The properties asserted after the word-for-word comparison are the ones the
 * specification states about the list, quoted where they are asserted. The
 * comparison already subsumes every one of them -- a table equal to the
 * published one has them by construction -- so they add no detection to this
 * file as it stands. They are here to say what the table has to satisfy,
 * against the day someone regenerates or extends it: they turn the selection
 * criteria into something that fails out loud rather than something to be
 * rediscovered in the document. The last of them is also the reason
 * BCR-2020-012's minimal form (first and last letter only) is decodable at
 * all.
 *
 * Scope is the data. bolos_ux_sskr_byteword_to_hex(), the index helpers and
 * the keyboard candidates read this table and have their own targets
 * (words.c, sskr_keyboard_candidates.c, sskr_byteword_sentinel.c); nothing
 * here calls them.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. */
// clang-format off
#include "testutils.h"
#include "sskr/seed_rom_variables.h"
// clang-format on

#define BYTEWORD_COUNT 256

/* Transcribed from BCR-2020-012, section "Word List". One row per row of
 * the document, eight words each, so 0x00, 0x08, ... start the rows here
 * exactly as they do there -- which is what makes the two readable
 * side by side. Hence the formatting exclusion. */
// clang-format off
static const char* const k_bcr_2020_012[BYTEWORD_COUNT] = {
    "able", "acid", "also", "apex", "aqua", "arch", "atom", "aunt",
    "away", "axis", "back", "bald", "barn", "belt", "beta", "bias",
    "blue", "body", "brag", "brew", "bulb", "buzz", "calm", "cash",
    "cats", "chef", "city", "claw", "code", "cola", "cook", "cost",
    "crux", "curl", "cusp", "cyan", "dark", "data", "days", "deli",
    "dice", "diet", "door", "down", "draw", "drop", "drum", "dull",
    "duty", "each", "easy", "echo", "edge", "epic", "even", "exam",
    "exit", "eyes", "fact", "fair", "fern", "figs", "film", "fish",
    "fizz", "flap", "flew", "flux", "foxy", "free", "frog", "fuel",
    "fund", "gala", "game", "gear", "gems", "gift", "girl", "glow",
    "good", "gray", "grim", "guru", "gush", "gyro", "half", "hang",
    "hard", "hawk", "heat", "help", "high", "hill", "holy", "hope",
    "horn", "huts", "iced", "idea", "idle", "inch", "inky", "into",
    "iris", "iron", "item", "jade", "jazz", "join", "jolt", "jowl",
    "judo", "jugs", "jump", "junk", "jury", "keep", "keno", "kept",
    "keys", "kick", "kiln", "king", "kite", "kiwi", "knob", "lamb",
    "lava", "lazy", "leaf", "legs", "liar", "limp", "lion", "list",
    "logo", "loud", "love", "luau", "luck", "lung", "main", "many",
    "math", "maze", "memo", "menu", "meow", "mild", "mint", "miss",
    "monk", "nail", "navy", "need", "news", "next", "noon", "note",
    "numb", "obey", "oboe", "omit", "onyx", "open", "oval", "owls",
    "paid", "part", "peck", "play", "plus", "poem", "pool", "pose",
    "puff", "puma", "purr", "quad", "quiz", "race", "ramp", "real",
    "redo", "rich", "road", "rock", "roof", "ruby", "ruin", "runs",
    "rust", "safe", "saga", "scar", "sets", "silk", "skew", "slot",
    "soap", "solo", "song", "stub", "surf", "swan", "taco", "task",
    "taxi", "tent", "tied", "time", "tiny", "toil", "tomb", "toys",
    "trip", "tuna", "twin", "ugly", "undo", "unit", "urge", "user",
    "vast", "very", "veto", "vial", "vibe", "view", "visa", "void",
    "vows", "wall", "wand", "warm", "wasp", "wave", "waxy", "webs",
    "what", "when", "whiz", "wolf", "work", "yank", "yawn", "yell",
    "yoga", "yurt", "zaps", "zero", "zest", "zinc", "zone", "zoom",
};
// clang-format on

/* The i-th entry of the table, as a NUL-terminated string. */
static const char* word_at(unsigned int i, char out[SSKR_BYTEWORD_LENGTH + 1]) {
    memcpy(out, SSKR_WORDLIST + i * SSKR_BYTEWORD_LENGTH, SSKR_BYTEWORD_LENGTH);
    out[SSKR_BYTEWORD_LENGTH] = '\0';
    return out;
}

/* The table is 256 fixed-width entries and the code indexes it by multiplying
 * a byte value by SSKR_BYTEWORD_LENGTH, so both constants are part of the
 * layout, not free parameters. */
static void test_the_table_is_256_words_of_4_letters(void** state) {
    (void)state;

    assert_int_equal(SSKR_BYTEWORD_LENGTH, 4);
    assert_int_equal(SSKR_WORDLIST_LENGTH,
                     BYTEWORD_COUNT * SSKR_BYTEWORD_LENGTH);
    assert_int_equal(sizeof(SSKR_WORDLIST), SSKR_WORDLIST_LENGTH);
}

/* The comparison that makes this file an oracle: every byte value maps to the
 * word BCR-2020-012 assigns it. */
static void test_every_word_matches_bcr_2020_012(void** state) {
    (void)state;

    for (unsigned int i = 0; i < BYTEWORD_COUNT; i++) {
        char actual[SSKR_BYTEWORD_LENGTH + 1];

        if (strcmp(word_at(i, actual), k_bcr_2020_012[i]) != 0) {
            fail_msg("byte 0x%02x: table has \"%s\", BCR-2020-012 has \"%s\"",
                     i, actual, k_bcr_2020_012[i]);
        }
    }
}

/* "All words are exactly 4 ASCII letters." -- BCR-2020-012, word selection
 * criteria. Fixed width is already asserted above; this is the ASCII
 * lowercase part of it, over the raw bytes, since the table is an
 * unsigned char array and nothing else constrains what it holds. */
static void test_every_character_is_ascii_lowercase(void** state) {
    (void)state;

    for (unsigned int i = 0; i < SSKR_WORDLIST_LENGTH; i++) {
        assert_true(SSKR_WORDLIST[i] >= 'a' && SSKR_WORDLIST[i] <= 'z');
    }
}

/* "Word list is sorted alphabetically." -- BCR-2020-012.
 *
 * Asserted strictly: equal neighbours would be a duplicate, which the
 * uniqueness criteria below already forbid, and which would make one byte
 * value undecodable. */
static void test_the_list_is_strictly_ascending(void** state) {
    (void)state;

    for (unsigned int i = 1; i < BYTEWORD_COUNT; i++) {
        char prev[SSKR_BYTEWORD_LENGTH + 1];
        char cur[SSKR_BYTEWORD_LENGTH + 1];

        (void)word_at(i - 1, prev);
        (void)word_at(i, cur);
        if (strcmp(prev, cur) >= 0) {
            fail_msg("byte 0x%02x: \"%s\" does not follow \"%s\"", i, cur,
                     prev);
        }
    }
}

/* Counts how many of the 256 words are distinct once reduced to the
 * `len` characters starting at `offset`. */
static unsigned int distinct_slices(unsigned int offset, unsigned int len) {
    unsigned int distinct = 0;

    for (unsigned int i = 0; i < BYTEWORD_COUNT; i++) {
        const unsigned char* a =
            SSKR_WORDLIST + i * SSKR_BYTEWORD_LENGTH + offset;
        bool seen_before = false;

        for (unsigned int j = 0; j < i; j++) {
            const unsigned char* b =
                SSKR_WORDLIST + j * SSKR_BYTEWORD_LENGTH + offset;
            if (memcmp(a, b, len) == 0) {
                seen_before = true;
                break;
            }
        }
        if (!seen_before) {
            distinct++;
        }
    }
    return distinct;
}

/* "Each word's first three letters must be a unique sequence (XXX-)."
 * -- BCR-2020-012. */
static void test_the_first_three_letters_are_unique(void** state) {
    (void)state;

    assert_int_equal(distinct_slices(0, 3), BYTEWORD_COUNT);
}

/* "Each word's last three letters must be a unique sequence (-XXX)."
 * -- BCR-2020-012. */
static void test_the_last_three_letters_are_unique(void** state) {
    (void)state;

    assert_int_equal(distinct_slices(1, 3), BYTEWORD_COUNT);
}

/* "Each word's first and last letters must be a unique sequence (X--X)."
 * -- BCR-2020-012. This is the criterion the minimal ByteWords form depends
 * on: two letters per byte are enough only because no two words share a
 * (first, last) pair. */
static void test_the_first_and_last_letters_are_unique(void** state) {
    (void)state;

    unsigned int distinct = 0;

    for (unsigned int i = 0; i < BYTEWORD_COUNT; i++) {
        const unsigned char* a = SSKR_WORDLIST + i * SSKR_BYTEWORD_LENGTH;
        bool seen_before = false;

        for (unsigned int j = 0; j < i; j++) {
            const unsigned char* b = SSKR_WORDLIST + j * SSKR_BYTEWORD_LENGTH;
            if (a[0] == b[0] &&
                a[SSKR_BYTEWORD_LENGTH - 1] == b[SSKR_BYTEWORD_LENGTH - 1]) {
                seen_before = true;
                break;
            }
        }
        if (!seen_before) {
            distinct++;
        }
    }
    assert_int_equal(distinct, BYTEWORD_COUNT);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_the_table_is_256_words_of_4_letters),
        cmocka_unit_test(test_every_word_matches_bcr_2020_012),
        cmocka_unit_test(test_every_character_is_ascii_lowercase),
        cmocka_unit_test(test_the_list_is_strictly_ascending),
        cmocka_unit_test(test_the_first_three_letters_are_unique),
        cmocka_unit_test(test_the_last_three_letters_are_unique),
        cmocka_unit_test(test_the_first_and_last_letters_are_unique),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
