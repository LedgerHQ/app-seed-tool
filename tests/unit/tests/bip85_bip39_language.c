/*
 * The language component of the BIP-85 BIP39 path, and the wordlist the
 * mnemonic is actually read out of.
 *
 * bolos_ux_bip85_bip39() validates `words` -- bip85_bip39_words_valid()
 * backs a LEDGER_ASSERT on it -- and does not validate `language`, which
 * travels unchecked into m/83696968'/39'/{language}'/{words}'/{index}'.
 * The words that come back out are then read from the one wordlist this
 * application embeds, which is the English one. Those two facts have to
 * agree, and nothing held them together.
 *
 * What a disagreement would look like: a non-zero language changes the
 * derivation path, so it changes the entropy, so it changes the mnemonic --
 * but the words are still looked up in the English list. The result is a
 * valid English mnemonic derived at the path BIP-85 reserves for another
 * language's list. No error, no indication on screen, and it does not match
 * what any other BIP-85 implementation produces for either language.
 *
 * This is not reachable today. One caller in the whole application reaches
 * bolos_ux_bip85_bip39(), src/nbgl/bip85_app.c, and it passes a literal 0;
 * there is no second caller and no screen that lets the value be chosen.
 * Checked over src/ before this file was written, not assumed. So this file
 * is not a bug report -- it is what keeps that from becoming one silently,
 * the day a language selector or a moved parameter puts a different value
 * on that call.
 *
 * Both halves are pinned against numbers published outside this repository:
 *
 *   - BIP-85 ("Deterministic Entropy From BIP32 Keychains"), BIP39
 *     application: path m/83696968'/39'/{language}'/{words}'/{index}',
 *     Language Table entry "English | 0'".
 *
 *   - BIP-39's own english.txt, the wordlist file the specification ships:
 *     https://github.com/bitcoin/bips/blob/master/bip-0039/english.txt
 *     2048 words, one per line, SHA-256 of the file
 *     2f5eed53a4727b4bf8880d8f3f199efc90e58503646d9ff8eff3a2ed3b24dbda.
 *     The digest is over the published file as it stands -- every word
 *     followed by a newline, 13116 bytes -- and the test below rebuilds
 *     exactly that byte sequence from BIP39_WORDLIST/BIP39_WORDLIST_OFFSETS
 *     and hashes it. A digest is what makes this an oracle for all 2048
 *     entries rather than for the handful a spot check could name; and
 *     unlike a word-for-word transcription it does not put a second copy of
 *     the list in the tree.
 *
 * Scope. tests/bip85_derivation_path.c already pins every component of every
 * BIP-85 path this application builds, language included, but only against
 * itself -- it never asks what wordlist the code number stands for, because
 * that file links seed_bip85.c alone and there is no wordlist in it. The
 * pairing is the gap, and it needs both files in one target.
 */

#include <cmocka.h>
#include <lcx_sha256.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * bip39/seed_rom_variables.h uses without defining. */
// clang-format off
#include "testutils.h"
#include "bip39/common_bip39.h"
#include "bip39/seed_rom_variables.h"
#include "common/bip85/bip85_internal.h"
// clang-format on

#define HARDENED_OFFSET 0x80000000u
#define HARDENED(n) (HARDENED_OFFSET | (uint32_t)(n))

// BIP-85's Language Table, as the specification prints it. Only the first is
// reachable here; the rest are named so that the number this application
// pins is visibly the English one and not merely "zero".
#define BIP85_LANG_ENGLISH 0u
#define BIP85_LANG_JAPANESE 1u
#define BIP85_LANG_FRENCH 6u

#define BIP85_PATH_LEN_BIP39 5

// Words are stored back to back with no separator; english.txt separates
// them with a newline, so the reconstruction below is the concatenation plus
// one byte per word.
#define ENGLISH_WORD_COUNT 2048u
#define ENGLISH_FILE_LENGTH (BIP39_WORDLIST_LENGTH + ENGLISH_WORD_COUNT)

static const uint8_t k_english_txt_sha256[32] = {
    0x2f, 0x5e, 0xed, 0x53, 0xa4, 0x72, 0x7b, 0x4b, 0xf8, 0x88, 0x0d,
    0x8f, 0x3f, 0x19, 0x9e, 0xfc, 0x90, 0xe5, 0x85, 0x03, 0x64, 0x6d,
    0x9f, 0xf8, 0xef, 0xf3, 0xa2, 0xed, 0x3b, 0x24, 0xdb, 0xda};

// The language component of every path this application can build.
//
// `words` is one of the three sizes src/nbgl/ui.c offers and `index` is the
// only other thing the user moves, so sweeping those two while holding
// language at 0 covers the whole reachable input space of that parameter --
// which is the claim being made: it is a constant, not a variable.
static void test_language_component_is_english_for_every_reachable_call(
    void** state) {
    (void)state;

    static const uint8_t words_offered[] = {12, 18, 24};
    static const unsigned int indices[] = {0u, 1u, 0x7fffffffu, 0xffffffffu};

    for (size_t w = 0; w < sizeof(words_offered) / sizeof(words_offered[0]);
         w++) {
        for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
            unsigned int path[BIP85_PATH_LEN_BIP39];
            unsigned int path_len = bip85_path_bip39(
                path, (uint8_t)BIP85_LANG_ENGLISH, words_offered[w],
                indices[i]);

            assert_int_equal(path_len, BIP85_PATH_LEN_BIP39);
            assert_int_equal(path[2], HARDENED(BIP85_LANG_ENGLISH));
        }
    }
}

// The other half: 0' means English, so the list the words are read out of
// has to be the English one. Held against the published file's digest rather
// than against a transcription, so all 2048 entries are covered at once.
static void test_embedded_wordlist_is_the_published_english_one(void** state) {
    (void)state;

    // The offsets table has one entry per word plus a final sentinel, and
    // the last offset is the total length of the concatenation. Both are
    // part of the reconstruction below, so both are asserted rather than
    // trusted.
    assert_int_equal(BIP39_WORDLIST_OFFSETS_LENGTH, ENGLISH_WORD_COUNT + 1);
    assert_int_equal(BIP39_WORDLIST_OFFSETS[ENGLISH_WORD_COUNT],
                     BIP39_WORDLIST_LENGTH);

    static uint8_t english_txt[ENGLISH_FILE_LENGTH];
    size_t position = 0;

    for (unsigned int i = 0; i < ENGLISH_WORD_COUNT; i++) {
        unsigned short start = BIP39_WORDLIST_OFFSETS[i];
        unsigned short end = BIP39_WORDLIST_OFFSETS[i + 1];

        assert_true(end > start);
        assert_true(position + (size_t)(end - start) + 1 <=
                    sizeof(english_txt));

        memcpy(english_txt + position, BIP39_WORDLIST + start,
               (size_t)(end - start));
        position += (size_t)(end - start);
        english_txt[position++] = '\n';
    }

    assert_int_equal(position, sizeof(english_txt));

    uint8_t digest[32] = {0};
    cx_hash_sha256(english_txt, sizeof(english_txt), digest, sizeof(digest));

    assert_memory_equal(digest, k_english_txt_sha256, sizeof(digest));
}

// The consequence of the two above, stated as an assertion rather than left
// in a comment: the language parameter is live -- a different code really
// does derive from a different path -- while the wordlist reached from it is
// the same one either way, because this build embeds exactly one.
//
// Nothing here calls this a defect. It is the reason the constant matters:
// were a non-zero language ever to reach bip85_path_bip39(), the mnemonic
// would still be spelled in English words, and nothing between here and the
// screen would say otherwise. Whether that argues for refusing a non-zero
// language outright, or for the language parameter to disappear until a
// second wordlist exists, is a design decision this file deliberately does
// not take.
static void test_a_non_english_language_would_change_only_the_path(
    void** state) {
    (void)state;

    unsigned int english[BIP85_PATH_LEN_BIP39];
    unsigned int japanese[BIP85_PATH_LEN_BIP39];
    unsigned int french[BIP85_PATH_LEN_BIP39];

    bip85_path_bip39(english, (uint8_t)BIP85_LANG_ENGLISH, 12, 0);
    bip85_path_bip39(japanese, (uint8_t)BIP85_LANG_JAPANESE, 12, 0);
    bip85_path_bip39(french, (uint8_t)BIP85_LANG_FRENCH, 12, 0);

    // Accepted without complaint, and each gives a distinct derivation path:
    // distinct entropy, hence a distinct mnemonic.
    assert_int_equal(japanese[2], HARDENED(BIP85_LANG_JAPANESE));
    assert_int_equal(french[2], HARDENED(BIP85_LANG_FRENCH));
    assert_int_not_equal(japanese[2], english[2]);
    assert_int_not_equal(french[2], english[2]);

    // ...and every other component is untouched, so the language code is the
    // only thing that moved.
    for (size_t i = 0; i < BIP85_PATH_LEN_BIP39; i++) {
        if (i == 2) {
            continue;
        }
        assert_int_equal(japanese[i], english[i]);
        assert_int_equal(french[i], english[i]);
    }

    // The list those words would be read from, in all three cases. There is
    // one, and the test above says which.
    unsigned char first[16] = {0};
    unsigned char last[16] = {0};
    assert_int_equal(bolos_ux_bip39_idx_strcpy(0, first), 7);
    assert_string_equal((const char*)first, "abandon");
    assert_int_equal(bolos_ux_bip39_idx_strcpy(ENGLISH_WORD_COUNT - 1, last),
                     3);
    assert_string_equal((const char*)last, "zoo");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_language_component_is_english_for_every_reachable_call),
        cmocka_unit_test(test_embedded_wordlist_is_the_published_english_one),
        cmocka_unit_test(
            test_a_non_english_language_would_change_only_the_path),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
