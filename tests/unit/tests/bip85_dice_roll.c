#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BIP85_ENTROPY_LENGTH 64

extern int32_t bip85_dice_roll(uint32_t* out, size_t out_capacity,
                               uint32_t sides, uint32_t rolls,
                               const uint8_t seed[BIP85_ENTROPY_LENGTH]);

// d2 (sides = 2) has bits_per_roll = 1, so every byte of the SHAKE256
// digest yields either 0 or 1 -- always < sides -- and is accepted. That
// makes the accept rate exactly 100% regardless of the seed's actual
// content, so a fixed, arbitrary seed is enough to make this test
// deterministic: no dependency on real BIP85 entropy or on the rejection
// step ever discarding a byte.
static const uint8_t seed[BIP85_ENTROPY_LENGTH] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
    0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40};

// With bytes_per_roll = 1 and a fixed BIP85_DRNG_MAX_DIGEST_SIZE = 256
// byte digest, no more than 256 rolls can ever come out of a single,
// non-extended digest -- even at the 100% acceptance rate d2 gives here,
// which is the best case any (sides, rolls) pair can have. Asking for 257
// therefore always demonstrates the silent-truncation bug: it does not
// depend on the rejection step ever discarding a byte, so it does not
// depend on chance.
static void test_dice_roll_truncates_past_256_bytes(void** state) {
    (void)state;

    uint32_t out[300];
    int32_t produced =
        bip85_dice_roll(out, sizeof(out) / sizeof(out[0]), 2, 257, seed);

    // Bug: fewer rolls than requested come back, and the caller has no
    // signal beyond comparing the return value to what it asked for --
    // which today's `bolos_ux_bip85_dice()` cannot do at all, since it
    // returns void.
    assert_int_equal(produced, 256);
}

// A request that fits comfortably inside a single digest must not be
// affected by the bug above -- this is the baseline the fix must not
// regress.
static void test_dice_roll_exact_when_well_under_capacity(void** state) {
    (void)state;

    uint32_t out[10];
    int32_t produced =
        bip85_dice_roll(out, sizeof(out) / sizeof(out[0]), 2, 10, seed);

    assert_int_equal(produced, 10);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dice_roll_truncates_past_256_bytes),
        cmocka_unit_test(test_dice_roll_exact_when_well_under_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
