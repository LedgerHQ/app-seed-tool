#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/bip85/bip85_internal.h"

// d2 (sides = 2) has bits_per_roll = 1, so every byte of the SHAKE256
// digest yields either 0 or 1 -- always < sides -- and is accepted. That
// makes the accept rate exactly 100% regardless of the seed's actual
// content, so a fixed, arbitrary seed is enough to make these tests
// deterministic: no dependency on real BIP85 entropy or on the rejection
// step ever discarding a byte.
static const uint8_t seed[BIP85_ENTROPY_LENGTH] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c,
    0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40};

// The DICE test vector BIP-85 publishes, transcribed from the specification
// (bip-0085.mediawiki, "DICE"), for:
//
//   MASTER BIP32 ROOT KEY: xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBq
//                          sx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhU
//                          tyoFnCNkfmXRyPXLjbKb
//   PATH:                  m/83696968'/89101'/6'/10'/0'
//   DERIVED ENTROPY:       5e41f8f5...4b6f6cd2  (the 64 bytes below)
//   DERIVED ROLLS:         1,0,0,2,0,1,5,5,2,4
//
// The specification prints the derived entropy next to the rolls, which is
// what makes this vector reachable here at all: the 64 bytes below are
// exactly what bolos_ux_bip85_dice() would hand bip85_dice_roll() for that
// path, so the whole vector is checkable without any BIP32 derivation and
// without os_derive_bip32_no_throw(), the BOLOS syscall that has no host
// equivalent. The derivation boundary was never the reason this vector was
// missing.
static const uint8_t dice_vector_entropy[BIP85_ENTROPY_LENGTH] = {
    0x5e, 0x41, 0xf8, 0xf5, 0xd5, 0xd9, 0xac, 0x09, 0xa2, 0x0b, 0x8a,
    0x57, 0x97, 0xa3, 0x17, 0x2b, 0x28, 0xc8, 0x06, 0xae, 0xad, 0x00,
    0xd2, 0x7e, 0x36, 0x60, 0x9e, 0x2d, 0xd1, 0x16, 0xa5, 0x91, 0x76,
    0xa7, 0x38, 0x80, 0x42, 0x36, 0x58, 0x6f, 0x66, 0x8d, 0xa8, 0xa5,
    0x1b, 0x90, 0xc7, 0x08, 0xa4, 0x22, 0x6d, 0x7f, 0x92, 0x25, 0x9c,
    0x69, 0xf6, 0x4c, 0x51, 0x12, 0x4b, 0x6f, 0x6c, 0xd2};

// The published rolls, written out as the specification prints them. Not
// recomputed here from anything this repository owns: an expected value that
// the code under test could have produced is not an oracle.
static const uint32_t dice_vector_rolls[] = {1, 0, 0, 2, 0, 1, 5, 5, 2, 4};

#define DICE_VECTOR_SIDES 6
#define DICE_VECTOR_ROLLS (sizeof(dice_vector_rolls) / sizeof(uint32_t))

// The only test in this suite that pins a produced roll value.
//
// Everything else here asserts a count (`produced == 257`, `== 10`, `== -1`,
// `== -3`) or a self-consistency -- and a self-consistency compares two runs
// of the same binary, so it keeps agreeing when that binary is wrong. Two
// rules the specification states in so many words are only visible in the
// values:
//
//   "Trim any bits in excess of bits_per_roll (retain the most significant
//    bits)"  -- keeping the low bits instead yields 3,2,5,0,3,5,4,1,1,1 on
//    this very vector, a perfectly plausible sequence of ten valid d6 rolls;
//
//   "If the trial is greater than or equal to the number of sides, skip it"
//    -- rejecting on `> sides` instead yields 6,1,0,0,2,0,1,6,5,5, which
//    opens with a 6 on a six-sided die.
//
// Neither is reachable at sides = 2, which is all the tests above use: at
// one bit per roll the shift direction is immaterial, and `< 2` and `<= 2`
// accept exactly the same values, so the rejection step never fires.
static void test_dice_roll_matches_bip85_vector(void** state) {
    (void)state;

    uint32_t out[DICE_VECTOR_ROLLS];
    int32_t produced =
        bip85_dice_roll(out, DICE_VECTOR_ROLLS, DICE_VECTOR_SIDES,
                        DICE_VECTOR_ROLLS, dice_vector_entropy);

    assert_int_equal(produced, (int32_t)DICE_VECTOR_ROLLS);

    for (size_t i = 0; i < DICE_VECTOR_ROLLS; i++) {
        // Redundant with the equality below, but it is the invariant the
        // specification states -- "an N-sided die produces values in the
        // range [0, N-1]" -- and failing on it says so directly.
        assert_true(out[i] < DICE_VECTOR_SIDES);
        assert_int_equal(out[i], dice_vector_rolls[i]);
    }
}

// ---------------------------------------------------------------------------
// Rolls wider than one byte.
//
// bip85_dice_roll() assembles bytes_per_roll bytes of the DRNG stream into
// one integer, most significant byte first:
//
//     roll_result = roll_result << 8 | *digest_ptr++;
//
// BIP-85 requires exactly that order, and reversing it draws a different --
// still perfectly plausible -- sequence. Nothing in this file could see the
// difference before: bytes_per_roll is ceil(ceil(log2(sides)) / 8), so it is
// 1 for every sides up to and including 256, and a single byte has no order.
// The vector above uses sides = 6 and the sweep below asserts only that no
// roll reaches sides, so the loop above ran over one byte in every pinned
// case. sides > 256 is the exact threshold at which bytes_per_roll becomes 2
// and the assembly order starts to matter.
//
// Oracle for the two sequences below: not this repository. They were computed
// with a separate SHAKE256 implementation (Python's hashlib.shake_256) driven
// by the DICE algorithm as BIP-85 states it in prose --
//
//     "bits_per_roll = ceil(log_2(sides))"
//     "bytes_per_roll = ceil(bits_per_roll / 8)"
//     "Trim any bits in excess of bits_per_roll (retain the most significant
//      bits)"
//     "If the trial is greater than or equal to the number of sides, skip it"
//
// -- and that separate implementation was first checked against the one DICE
// vector the specification publishes: fed the derived entropy above with
// sides = 6 and 10 rolls, it reproduces 1,0,0,2,0,1,5,5,2,4 exactly. Only
// then was it run at the two wider sides below, on the same published
// entropy. So the expected values are an extension of a published vector by
// an implementation that agrees with it, not an output of the code under
// test.
//
// The seed is the specification's own DERIVED ENTROPY (dice_vector_entropy
// above) for both, so the whole input side of these two cases is published
// too; only sides and the roll count differ from the printed vector.
// ---------------------------------------------------------------------------

// sides = 257: bits_per_roll = 9, bytes_per_roll = 2, and the worst
// acceptance rate a 9-bit draw can have -- 257 of the 512 values a trial can
// take are kept, so roughly half the draws are discarded. That makes this
// case hold the rejection rule as well as the byte order: within these 20
// rolls a trial comes out exactly equal to 257, so accepting on `<= sides`
// instead of `< sides` would splice a 257 in as the 18th roll -- a value a
// 257-sided die cannot show -- and shift everything after it.
//
// Reversing the two bytes instead yields
// 127,36,16,127,200,25,42,9,202,238,... and keeping the low nine bits rather
// than the high nine yields 18,182,12,4,149,221,170,185,234,134,... Both are
// sequences of twenty valid rolls, which is why only pinned values catch
// either one.
static const uint32_t wide_257_rolls[] = {12,  154, 131, 146, 206, 202, 237,
                                          105, 138, 63,  24,  102, 108, 128,
                                          208, 215, 155, 121, 160, 212};

#define WIDE_257_SIDES 257
#define WIDE_257_COUNT (sizeof(wide_257_rolls) / sizeof(uint32_t))

// sides = 500: the other end of the same 9-bit band, where 500 of 512 trials
// are accepted and rejection almost never fires -- so this one is about the
// byte order alone, on a sides that is not one more than a power of two.
// Reversing the bytes yields 127,36,16,127,427,365,274,200,369,485.
static const uint32_t wide_500_rolls[] = {390, 12,  154, 478, 345,
                                          131, 146, 359, 383, 460};

#define WIDE_500_SIDES 500
#define WIDE_500_COUNT (sizeof(wide_500_rolls) / sizeof(uint32_t))

static void test_dice_roll_wide_matches_external_oracle(void** state) {
    (void)state;

    // The premise of both cases: above 256 sides a roll is assembled from two
    // bytes. Asserted rather than assumed, because if this ever became 1 the
    // pinned values below would still pass for the wrong reason.
    assert_int_equal(bip85_dice_bits_per_roll(WIDE_257_SIDES), 9);
    assert_int_equal(bip85_dice_bits_per_roll(WIDE_500_SIDES), 9);
    // ...and one byte still suffices at 256, which is what makes 257 the
    // threshold rather than an arbitrary choice.
    assert_int_equal(bip85_dice_bits_per_roll(256), 8);

    uint32_t out_257[WIDE_257_COUNT];
    int32_t produced_257 =
        bip85_dice_roll(out_257, WIDE_257_COUNT, WIDE_257_SIDES,
                        WIDE_257_COUNT, dice_vector_entropy);
    assert_int_equal(produced_257, (int32_t)WIDE_257_COUNT);
    for (size_t i = 0; i < WIDE_257_COUNT; i++) {
        assert_true(out_257[i] < WIDE_257_SIDES);
        assert_int_equal(out_257[i], wide_257_rolls[i]);
    }

    uint32_t out_500[WIDE_500_COUNT];
    int32_t produced_500 =
        bip85_dice_roll(out_500, WIDE_500_COUNT, WIDE_500_SIDES,
                        WIDE_500_COUNT, dice_vector_entropy);
    assert_int_equal(produced_500, (int32_t)WIDE_500_COUNT);
    for (size_t i = 0; i < WIDE_500_COUNT; i++) {
        assert_true(out_500[i] < WIDE_500_SIDES);
        assert_int_equal(out_500[i], wide_500_rolls[i]);
    }
}

// The vector above pins one case; this pins the rule behind it.
//
// Rejection only does anything when `sides` is not a power of two, so the
// sizes swept here are chosen to make it bite: three just above 2, two just
// above a power of two (129 and 257, the worst cases at 8 and 9 bits per
// roll, where barely half the draws are accepted), and two in between.
// Measured on the seed below at 50 rolls each, every one of them discards
// draws -- from 11 rejected at sides = 100 up to 37 at sides = 129. A power
// of two, or sides = 2, would make this test vacuous, which is exactly the
// trap the rest of the file falls into.
//
// The seed is the same arbitrary one the d2 tests use; here it is arbitrary
// for a different reason -- the property asserted holds for every seed, so
// no particular one is privileged.
static void test_dice_roll_never_exceeds_sides(void** state) {
    (void)state;

    static const uint32_t sides_sweep[] = {3, 5, 6, 20, 100, 129, 257};
    const uint32_t rolls = 50;

    for (size_t s = 0; s < sizeof(sides_sweep) / sizeof(uint32_t); s++) {
        uint32_t out[50];
        int32_t produced = bip85_dice_roll(out, sizeof(out) / sizeof(uint32_t),
                                           sides_sweep[s], rolls, seed);

        assert_int_equal(produced, (int32_t)rolls);
        for (uint32_t i = 0; i < rolls; i++) {
            assert_true(out[i] < sides_sweep[s]);
        }
    }
}

// With bytes_per_roll = 1 and a fixed BIP85_DRNG_MAX_DIGEST_SIZE = 256
// byte digest, no more than 256 rolls can ever come out of a single,
// non-extended digest -- even at the 100% acceptance rate d2 gives here,
// which is the best case any (sides, rolls) pair can have. Asking for 257
// therefore always exercises the re-extension path: it does not depend on
// the rejection step ever discarding a byte, so it does not depend on
// chance.
static void test_dice_roll_extends_past_256_bytes(void** state) {
    (void)state;

    uint32_t out[300];
    int32_t produced =
        bip85_dice_roll(out, sizeof(out) / sizeof(out[0]), 2, 257, seed);

    // Fixed: the digest is re-derived at double the length and the request
    // is satisfied in full, rather than silently truncated at 256 -- which
    // is what `bolos_ux_bip85_dice()` could not even report before, since
    // it returned void.
    assert_int_equal(produced, 257);
}

// A request that fits comfortably inside a single digest must not be
// affected by the extension path above.
static void test_dice_roll_exact_when_well_under_capacity(void** state) {
    (void)state;

    uint32_t out[10];
    int32_t produced =
        bip85_dice_roll(out, sizeof(out) / sizeof(out[0]), 2, 10, seed);

    assert_int_equal(produced, 10);
}

// SHAKE256 is a genuine XOF: a longer digest from the same seed must
// reproduce the same leading bytes as a shorter one. This is the property
// the fix's re-derive-from-scratch strategy depends on for correctness --
// verified here end to end through the real cx_shake256_hash(), not just
// assumed. rolls=256 stays within the initial digest (no extension);
// rolls=257 forces exactly one re-extension (512-byte digest). The first
// 256 results of the second call must be pixel-for-pixel identical to the
// first call's, proving the extension does not alter rolls already found.
static void test_dice_roll_extension_preserves_prior_rolls(void** state) {
    (void)state;

    uint32_t out_no_extension[256];
    int32_t produced_no_extension = bip85_dice_roll(
        out_no_extension, sizeof(out_no_extension) / sizeof(uint32_t), 2, 256,
        seed);
    assert_int_equal(produced_no_extension, 256);

    uint32_t out_extended[257];
    int32_t produced_extended = bip85_dice_roll(
        out_extended, sizeof(out_extended) / sizeof(uint32_t), 2, 257, seed);
    assert_int_equal(produced_extended, 257);

    assert_memory_equal(out_no_extension, out_extended,
                        sizeof(out_no_extension));
}

// out_capacity < rolls is checked before anything else -- the content of
// seed/sides is irrelevant, and the function must return before writing
// anything to out.
static void test_dice_roll_rejects_insufficient_capacity(void** state) {
    (void)state;

    uint32_t out[5];
    int32_t produced = bip85_dice_roll(out, 5, 2, 10, seed);

    assert_int_equal(produced, -1);
}

// Each attempt re-derives the digest from scratch rather than accumulating
// across attempts, so the most rolls any single attempt can ever produce is
// bounded by its own digest length, not by the sum of all attempts. The
// largest digest ever tried is BIP85_DRNG_MAX_DIGEST_SIZE (256) doubled
// BIP85_DICE_MAX_DRNG_DOUBLINGS (3) times, i.e. 2048 bytes -- and at d2's
// 100% acceptance rate (see the seed comment above), that is also the most
// rolls that final attempt can produce. Asking for one more than that
// (2049) therefore deterministically exhausts every attempt without ever
// depending on chance.
static void test_dice_roll_reports_drng_exhaustion(void** state) {
    (void)state;

    uint32_t out[2049];
    int32_t produced =
        bip85_dice_roll(out, sizeof(out) / sizeof(out[0]), 2, 2049, seed);

    assert_int_equal(produced, -3);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_dice_roll_matches_bip85_vector),
        cmocka_unit_test(test_dice_roll_wide_matches_external_oracle),
        cmocka_unit_test(test_dice_roll_never_exceeds_sides),
        cmocka_unit_test(test_dice_roll_extends_past_256_bytes),
        cmocka_unit_test(test_dice_roll_exact_when_well_under_capacity),
        cmocka_unit_test(test_dice_roll_extension_preserves_prior_rolls),
        cmocka_unit_test(test_dice_roll_rejects_insufficient_capacity),
        cmocka_unit_test(test_dice_roll_reports_drng_exhaustion),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
