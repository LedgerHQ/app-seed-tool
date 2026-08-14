#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "sskr/common_sskr.h"
#include "testutils.h"

static void test_words_bip39(void** state) {
    unsigned char next_letters[27] = {0};
    size_t return_num = 0;
    unsigned char prefix[] = "ab";
    unsigned char buffer[8] = {0};

    return_num = bolos_ux_bip39_get_word_idx_starting_with(
        (const unsigned char*)prefix, 2);
    assert_int_equal(return_num, 0);

    return_num = bolos_ux_bip39_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 7);
    assert_string_equal((const char*)buffer, "abandon");

    return_num = bolos_ux_bip39_get_word_count_starting_with(
        (const unsigned char*)prefix, 2);
    assert_int_equal(return_num, 10);

    return_num = bolos_ux_bip39_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 2, next_letters);
    assert_int_equal(return_num, 6);
    assert_string_equal((const char*)next_letters, "ailosu");

    prefix[0] = 'z';
    memset(next_letters, 0, sizeof(next_letters));

    return_num = bolos_ux_bip39_get_word_idx_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 2044);

    return_num = bolos_ux_bip39_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 5);
    assert_string_equal((const char*)buffer, "zebra");

    return_num = bolos_ux_bip39_get_word_count_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 4);

    return_num = bolos_ux_bip39_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 1, next_letters);
    assert_int_equal(return_num, 2);
    assert_string_equal((const char*)next_letters, "eo");
}

static void test_words_sskr(void** state) {
    unsigned char next_letters[27] = {0};
    size_t return_num = 0;
    unsigned char prefix[] = "ab";
    unsigned char buffer[5] = {0};

    return_num = bolos_ux_sskr_get_word_idx_starting_with(
        (const unsigned char*)prefix, 2);
    assert_int_equal(return_num, 0);

    return_num = bolos_ux_sskr_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 4);
    assert_string_equal((const char*)buffer, "able");

    return_num = bolos_ux_sskr_get_word_count_starting_with(
        (const unsigned char*)prefix, 2);
    assert_int_equal(return_num, 1);

    return_num = bolos_ux_sskr_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 2, next_letters);
    assert_int_equal(return_num, 1);
    assert_string_equal((const char*)next_letters, "l");

    prefix[0] = 'z';
    memset(next_letters, 0, sizeof(next_letters));

    return_num = bolos_ux_sskr_get_word_idx_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 250);

    return_num = bolos_ux_sskr_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 4);
    assert_string_equal((const char*)buffer, "zaps");

    return_num = bolos_ux_sskr_get_word_count_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 6);

    return_num = bolos_ux_sskr_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 1, next_letters);
    assert_int_equal(return_num, 4);
    assert_string_equal((const char*)next_letters, "aeio");
}

// '9' is not a letter, and no word in either wordlist starts with a digit,
// so this prefix matches nothing. get_word_idx_starting_with() must return
// its documented sentinel (one past the last valid index -- note this is
// BIP39_WORDLIST_OFFSETS_LENGTH itself for BIP-39, not that minus one, since
// the function's own not-found return statement uses the raw macro).
// idx_strcpy() must then reject that sentinel index and leave buffer
// untouched (checked here via a pre-filled, non-zero pattern, not just
// checking the return value). get_word_count_starting_with() must report
// zero matches.
static void test_words_bip39_no_match(void** state) {
    unsigned char prefix[] = "9";
    unsigned char buffer[8];
    size_t return_num = 0;

    memset(buffer, 0xff, sizeof(buffer));

    return_num = bolos_ux_bip39_get_word_idx_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, BIP39_WORDLIST_OFFSETS_LENGTH);

    return_num = bolos_ux_bip39_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 0);
    assert_int_equal(buffer[0], 0xff);

    return_num = bolos_ux_bip39_get_word_count_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 0);
}

static void test_words_sskr_no_match(void** state) {
    unsigned char prefix[] = "9";
    unsigned char buffer[5];
    size_t return_num = 0;

    memset(buffer, 0xff, sizeof(buffer));

    return_num = bolos_ux_sskr_get_word_idx_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, SSKR_WORDLIST_LENGTH / SSKR_BYTEWORD_LENGTH);

    return_num = bolos_ux_sskr_idx_strcpy(return_num, buffer);
    assert_int_equal(return_num, 0);
    assert_int_equal(buffer[0], 0xff);

    return_num = bolos_ux_sskr_get_word_count_starting_with(
        (const unsigned char*)prefix, 1);
    assert_int_equal(return_num, 0);
}

// prefixlength = 0 is the state before any keypress -- what computes the
// initial keyboard. Neither wordlist has a word starting with 'x' (checked
// independently against the real wordlist data, not assumed), so 25 of the
// 26 letters are expected, not 26.
static void test_words_bip39_empty_prefix(void** state) {
    unsigned char next_letters[27] = {0};
    unsigned char prefix[] = "";
    size_t return_num = 0;

    return_num = bolos_ux_bip39_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 0, next_letters);
    assert_int_equal(return_num, 25);
    assert_string_equal((const char*)next_letters, "abcdefghijklmnopqrstuvwyz");
}

static void test_words_sskr_empty_prefix(void** state) {
    unsigned char next_letters[27] = {0};
    unsigned char prefix[] = "";
    size_t return_num = 0;

    return_num = bolos_ux_sskr_get_word_next_letters_starting_with(
        (const unsigned char*)prefix, 0, next_letters);
    assert_int_equal(return_num, 25);
    assert_string_equal((const char*)next_letters, "abcdefghijklmnopqrstuvwyz");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_words_bip39),
        cmocka_unit_test(test_words_sskr),
        cmocka_unit_test(test_words_bip39_no_match),
        cmocka_unit_test(test_words_sskr_no_match),
        cmocka_unit_test(test_words_bip39_empty_prefix),
        cmocka_unit_test(test_words_sskr_empty_prefix)};
    return cmocka_run_group_tests(tests, NULL, NULL);
}
