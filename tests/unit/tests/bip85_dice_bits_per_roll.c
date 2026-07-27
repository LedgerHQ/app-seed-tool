#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

extern uint8_t bip85_dice_bits_per_roll(uint32_t sides);

// bits_per_roll must be ceil(log2(sides)): the minimum number of bits that
// lets a rejection-sampling loop draw uniformly from [0, sides). Non-power-
// of-two dice (d3, d6, d20) already exercised the formula the same way
// before and after the fix; the power-of-two cases (d2, d4, d8, d16, d256)
// are the ones a floor(log2(sides))+1 formula overcounts by one bit.
static void test_bip85_dice_bits_per_roll(void **state) {
    (void) state;

    assert_int_equal(bip85_dice_bits_per_roll(2), 1);      // d2
    assert_int_equal(bip85_dice_bits_per_roll(3), 2);      // d3
    assert_int_equal(bip85_dice_bits_per_roll(4), 2);      // d4
    assert_int_equal(bip85_dice_bits_per_roll(6), 3);      // d6
    assert_int_equal(bip85_dice_bits_per_roll(8), 3);      // d8
    assert_int_equal(bip85_dice_bits_per_roll(16), 4);     // d16
    assert_int_equal(bip85_dice_bits_per_roll(20), 5);     // d20
    assert_int_equal(bip85_dice_bits_per_roll(256), 8);    // d256
    assert_int_equal(bip85_dice_bits_per_roll(0x7FFFFFFF), 31); // UINT32_MAX >> 1
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip85_dice_bits_per_roll)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
