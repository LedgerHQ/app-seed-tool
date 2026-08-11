/*
 * bolos_ux_sskr_share_wordcount(): how many ByteWords one share is, stated
 * before any share exists.
 *
 * Why this function is allowed to exist
 * ------------------------------------
 *
 * The review shown before a backup is generated tells the user how many words
 * they are about to have to copy down -- 230 for a 3-of-5 over 24 words, which
 * is the number that should make someone stop and think, and which nothing in
 * the application said before. It has to be stated *before* generation,
 * because nothing may be produced until the user has approved producing it.
 *
 * So it cannot come from bolos_ux_sskr_share_slice() or sskr_sharecount_get():
 * both describe a set that already exists. It is therefore a second route to a
 * number the generator also arrives at, which is exactly the kind of thing
 * that drifts -- the review would go on promising 46 words a share long after
 * a change made the generator write 47.
 *
 * What stops that is this file rather than the two being written next to each
 * other. test_wordcount_agrees_with_generated_shares() below generates a real
 * set with bolos_ux_bip39_to_sskr_convert() -- the function the device runs --
 * measures how long one share came out, and requires the prediction to equal
 * it. Both CBOR header forms are covered, because that is the term most likely
 * to be got wrong: a 12-word phrase gives a 21-byte shard and a short-form
 * header, 18 and 24 words give 29 and 37 and the long form.
 *
 * The three literals in test_wordcount_is_the_documented_table() are not a
 * second copy of the arithmetic -- they are what the interface promises, and
 * the point of pinning them is that changing the sum has to be a deliberate
 * edit to a number a human can read rather than a silent consequence.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "bip39/common_bip39.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
// clang-format on

#define MAX_SHARES          SSS_MAX_SHARE_COUNT
#define MAX_WORDS_PER_SHARE (SSKR_SHARE_MAX_WIRE_LENGTH * (SSKR_BYTEWORD_LENGTH + 1))
#define MAX_MNEMONIC_LEN    (24 * 9)

/* The three phrase lengths the application offers, with the shard length each
 * produces and hence which CBOR header form each exercises. */
static const struct {
    unsigned int words;
    uint8_t expected_share_len;
    uint8_t expected_wordcount;
} k_lengths[] = {
    /* 16-byte secret + 5 metadata = 21, short-form header (4 bytes) */
    {12, 21, 29},
    /* 24 + 5 = 29, long-form header (5 bytes) */
    {18, 29, 38},
    /* 32 + 5 = 37, long-form header (5 bytes) */
    {24, 37, 46},
};

static void test_wordcount_is_the_documented_table(void** state) {
    (void)state;

    for (size_t i = 0; i < sizeof(k_lengths) / sizeof(k_lengths[0]); i++) {
        assert_int_equal(bolos_ux_sskr_share_length(k_lengths[i].words),
                         k_lengths[i].expected_share_len);
        assert_int_equal(bolos_ux_sskr_share_wordcount(k_lengths[i].words),
                         k_lengths[i].expected_wordcount);
    }
}

static void test_cbor_header_length_switches_at_the_short_form_bound(
    void** state) {
    (void)state;

    /* The boundary itself, from both sides. RFC 8949 puts a byte string of up
     * to 23 bytes entirely in the initial byte and needs a following length
     * byte from 24 on. */
    assert_int_equal(
        bolos_ux_sskr_cbor_header_length(SSKR_CBOR_SHORT_FORM_MAX_LENGTH), 4);
    assert_int_equal(
        bolos_ux_sskr_cbor_header_length(SSKR_CBOR_SHORT_FORM_MAX_LENGTH + 1),
        5);
}

static void test_wordcount_refuses_a_length_no_screen_offers(void** state) {
    (void)state;

    /* Same narrow domain bolos_ux_sskr_size_get() applies: BIP-39 also defines
     * 15 and 21, and no screen in this application asks for either. Zero is
     * the answer, not a plausible-looking product -- a caller composing a
     * sentence out of it must get a number it cannot mistake for a real one. */
    assert_int_equal(bolos_ux_sskr_share_wordcount(0), 0);
    assert_int_equal(bolos_ux_sskr_share_wordcount(15), 0);
    assert_int_equal(bolos_ux_sskr_share_wordcount(21), 0);
    assert_int_equal(bolos_ux_sskr_share_wordcount(25), 0);
    assert_int_equal(bolos_ux_sskr_share_wordcount(255), 0);

    assert_int_equal(bolos_ux_sskr_share_length(15), 0);
    assert_int_equal(bolos_ux_sskr_share_length(255), 0);
}

/*
 * The agreement that makes the prediction safe to display.
 */
static void test_wordcount_agrees_with_generated_shares(void** state) {
    (void)state;

    /* A 3-of-5, which is the configuration whose 230-word total the review
     * exists to announce. The secret is fixed rather than random: what is
     * under test is a length, and a length does not vary with the secret. */
    static const uint8_t secret[SSKR_MAX_STRENGTH_BYTES] = {
        0xE3, 0x95, 0x5C, 0xDA, 0x30, 0x47, 0x71, 0xC0, 0x03, 0x18, 0x95,
        0x63, 0x7F, 0x55, 0xC3, 0xAB, 0xE4, 0x51, 0x53, 0xC8, 0x7A, 0xBD,
        0x81, 0xC5, 0x1E, 0xD1, 0x4E, 0x8A, 0xAF, 0xA1, 0xAF, 0x13};

    for (size_t i = 0; i < sizeof(k_lengths) / sizeof(k_lengths[0]); i++) {
        const unsigned int words = k_lengths[i].words;
        const uint8_t seed_len = (uint8_t)(words * 4 / 3);

        unsigned char mnemonic[MAX_MNEMONIC_LEN];
        unsigned char share_words[MAX_SHARES * MAX_WORDS_PER_SHARE];
        unsigned int group_descriptor[2] = {3, 5};
        unsigned int share_words_len = 0;
        uint8_t share_count = 0;

        const unsigned int mnemonic_len = bolos_ux_bip39_mnemonic_encode(
            secret, seed_len, mnemonic, sizeof(mnemonic));
        assert_int_not_equal(mnemonic_len, 0);

        bolos_ux_bip39_to_sskr_convert(mnemonic, mnemonic_len, words,
                                       group_descriptor, &share_count,
                                       share_words, &share_words_len);

        assert_int_equal(share_count, 5);
        assert_int_not_equal(share_words_len, 0);

        /* Space-separated ByteWords: one share of n words occupies
         * n * (SSKR_BYTEWORD_LENGTH + 1) - 1 characters, so recovering n from
         * the buffer is the inverse of what the generator wrote. */
        const unsigned int chars_per_share = share_words_len / share_count;
        const unsigned int words_per_share =
            (chars_per_share + 1) / (SSKR_BYTEWORD_LENGTH + 1);

        /* The whole point of the file: what the review will say, against what
         * the generator did. */
        assert_int_equal(bolos_ux_sskr_share_wordcount(words), words_per_share);

        /* And the character arithmetic is exact rather than rounded -- a share
         * whose length were not a whole number of ByteWords would still have
         * satisfied the division above. */
        assert_int_equal(
            chars_per_share,
            words_per_share * (SSKR_BYTEWORD_LENGTH + 1) - 1);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_wordcount_is_the_documented_table),
        cmocka_unit_test(test_cbor_header_length_switches_at_the_short_form_bound),
        cmocka_unit_test(test_wordcount_refuses_a_length_no_screen_offers),
        cmocka_unit_test(test_wordcount_agrees_with_generated_shares),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
