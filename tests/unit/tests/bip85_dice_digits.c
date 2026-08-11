/*
 * bip85_dice_rolls_to_digits(): the whole of what makes a PIN out of DICE.
 *
 * A PIN is `DICE(sides = 10, rolls = length)` and its digits are the rolls
 * themselves. Nothing is hashed, reduced or folded on the way to the screen --
 * anything that were would make this device's PIN unreproducible by any other
 * BIP-85 implementation, which is the only reason to derive one rather than
 * invent one. That leaves this function with exactly two ways to be wrong, and
 * both of them are silent:
 *
 *   - a leading zero lost, which is what any conversion through an integer
 *     does. "0934" and "934" are a four-digit PIN and a three-digit one, and
 *     the screen that showed the second would look perfectly normal;
 *
 *   - a digit dropped because the destination was too small, or because a roll
 *     was out of range. A PIN cut short is a different PIN, not an incomplete
 *     one: there is no ellipsis on a number, and the user copies down what is
 *     drawn.
 *
 * So the refusals below are checked on the buffer as much as on the return
 * value: every one of them has to leave the caller with an empty string and
 * with no digit written.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/bip85/common_bip85.h"

/* The PIN lengths the interface offers, at the widest. */
#define PIN_DIGITS_MAX 8

static void test_writes_the_rolls_in_order(void** state) {
    (void)state;

    const uint32_t rolls[] = {5, 8, 3, 1, 0, 4};
    char out[PIN_DIGITS_MAX + 1];

    assert_true(bip85_dice_rolls_to_digits(rolls, 6, out, sizeof(out)));
    assert_string_equal(out, "583104");
}

static void test_keeps_leading_zeros(void** state) {
    (void)state;

    char out[PIN_DIGITS_MAX + 1];

    /* The case that says a PIN is not a number: strtol("0012") is 12, so a
     * PIN read through an integer anywhere loses both of its leading zeros
     * and comes back two digits long with nothing to say so.
     *
     * These four rolls are chosen for that property, not derived from
     * anything. The derived form of the same case is pinned by the functional
     * suite, where four rolls at index 1 of the specification's seed are
     * 0, 9, 3, 4 -- see PIN_4_INDEX_1 in tests/functional/test_bip85_pin.py. */
    const uint32_t leading[] = {0, 0, 1, 2};
    assert_true(bip85_dice_rolls_to_digits(leading, 4, out, sizeof(out)));
    assert_string_equal(out, "0012");

    /* And a PIN that is nothing but zeros, which is the same property taken to
     * its end: eight digits, not one. */
    const uint32_t zeros[] = {0, 0, 0, 0, 0, 0, 0, 0};
    assert_true(bip85_dice_rolls_to_digits(zeros, 8, out, sizeof(out)));
    assert_string_equal(out, "00000000");
    assert_int_equal(strlen(out), 8);
}

static void test_terminates_at_the_digit_count(void** state) {
    (void)state;

    /* A buffer with something in it already: the terminator has to be written
     * at the end of the PIN rather than left wherever the previous journey
     * put one. Otherwise a four-digit PIN drawn into a buffer that held six
     * would show two digits of the one before it. */
    char out[PIN_DIGITS_MAX + 1];
    memset(out, 'x', sizeof(out) - 1);
    out[sizeof(out) - 1] = '\0';

    const uint32_t rolls[] = {7, 0, 2, 8};
    assert_true(bip85_dice_rolls_to_digits(rolls, 4, out, sizeof(out)));
    assert_string_equal(out, "7028");
    assert_int_equal(out[4], '\0');
}

static void test_fits_a_buffer_of_exactly_the_right_size(void** state) {
    (void)state;

    const uint32_t rolls[] = {1, 6, 4, 1, 1, 4, 6, 6};

    /* Eight digits and a terminator, which is what the application sizes its
     * PIN buffers from. */
    char exact[9];
    assert_true(bip85_dice_rolls_to_digits(rolls, 8, exact, sizeof(exact)));
    assert_string_equal(exact, "16411466");
}

static void test_refuses_a_buffer_too_short(void** state) {
    (void)state;

    const uint32_t rolls[] = {1, 6, 4, 1, 1, 4, 6, 6};

    /* Room for the digits but not for the terminator. */
    char no_room_to_terminate[8];
    assert_false(bip85_dice_rolls_to_digits(rolls, 8, no_room_to_terminate,
                                            sizeof(no_room_to_terminate)));
    assert_string_equal(no_room_to_terminate, "");

    char half[5];
    assert_false(bip85_dice_rolls_to_digits(rolls, 8, half, sizeof(half)));
    assert_string_equal(half, "");

    char one_byte[1];
    assert_false(
        bip85_dice_rolls_to_digits(rolls, 8, one_byte, sizeof(one_byte)));
    assert_string_equal(one_byte, "");
}

static void test_refuses_a_roll_that_is_not_a_digit(void** state) {
    (void)state;

    char out[PIN_DIGITS_MAX + 1];

    /* Ten sides give values in [0, 9]. Anything else came from a die this is
     * not the rendering for -- a twenty-sided roll of 11 is one roll, not the
     * two digits "11" -- and there is no character to write for it. */
    const uint32_t ten[] = {1, 2, 10, 4};
    assert_false(bip85_dice_rolls_to_digits(ten, 4, out, sizeof(out)));
    assert_string_equal(out, "");

    /* The bad roll last, so that three valid digits were available to write
     * before it: the refusal has to come before any of them reaches the
     * buffer. */
    const uint32_t last[] = {1, 2, 3, 99};
    assert_false(bip85_dice_rolls_to_digits(last, 4, out, sizeof(out)));
    assert_string_equal(out, "");

    const uint32_t huge[] = {0, 0, 0, UINT32_MAX};
    assert_false(bip85_dice_rolls_to_digits(huge, 4, out, sizeof(out)));
    assert_string_equal(out, "");
}

static void test_refuses_what_is_not_a_pin_at_all(void** state) {
    (void)state;

    const uint32_t rolls[] = {1, 2, 3, 4};
    char out[PIN_DIGITS_MAX + 1];

    /* No rolls is not an empty PIN, it is a PIN that was never derived. A
     * caller that lost its roll count must not be handed "" as though it were
     * a secret to display. */
    assert_false(bip85_dice_rolls_to_digits(rolls, 0, out, sizeof(out)));
    assert_string_equal(out, "");

    assert_false(bip85_dice_rolls_to_digits(NULL, 4, out, sizeof(out)));
    assert_string_equal(out, "");

    assert_false(bip85_dice_rolls_to_digits(rolls, 4, NULL, sizeof(out)));
    assert_false(bip85_dice_rolls_to_digits(rolls, 4, out, 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_writes_the_rolls_in_order),
        cmocka_unit_test(test_keeps_leading_zeros),
        cmocka_unit_test(test_terminates_at_the_digit_count),
        cmocka_unit_test(test_fits_a_buffer_of_exactly_the_right_size),
        cmocka_unit_test(test_refuses_a_buffer_too_short),
        cmocka_unit_test(test_refuses_a_roll_that_is_not_a_digit),
        cmocka_unit_test(test_refuses_what_is_not_a_pin_at_all),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
