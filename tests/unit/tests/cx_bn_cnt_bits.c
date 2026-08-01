/*
 * What the unit harness answers when asked how many bits a *zero* bignum has.
 *
 * cx_mpi_cnt_bits() (tests/unit/lib/bolos/cx_mpi.c) is a host stub standing in
 * for an SDK primitive. It converts the bignum to big-endian bytes with
 * BN_bn2bin(), into a 2048-byte buffer left uninitialised, then walks the
 * result looking for the first significant bit. A zero bignum has no
 * significant byte at all: BN_bn2bin() writes nothing and returns 0. The walk
 * then reads a buffer nothing has written and, because the byte counter is a
 * uint32_t, the `len--` meant to stop it turns 0 into 4294967295 -- the loop's
 * only exit condition can no longer be met and the read runs off the end.
 *
 * Nothing in src/ is involved. The defect is in the code that stands in for
 * the device, and it stayed invisible because the default build of
 * src/common/sskr/sss/interpolate.c calls the SDK's cx_bn_gf2_n_mul(), which
 * this harness stubs directly and which never counts bits. Under TARGET_NANOS
 * that file multiplies by hand out of elementary BN primitives instead and
 * asks cx_bn_cnt_bits() for the bit length of each operand -- where zero is an
 * ordinary operand, and the answer for it has to be zero.
 *
 * Both targets built from this file compile with AddressSanitizer, and the
 * first of the two compiles lib/bolos/ in rather than linking it from
 * libtestutils.so so that the stub's own stack frame is instrumented whatever
 * flags the rest of the build carries. Without a sanitiser the overrun reads
 * whatever follows the buffer and reports nothing, which is exactly how this
 * survived every unsanitised run of the suite. See CMakeLists.txt for why the
 * TARGET_NANOS target cannot do the same.
 */

#include <cmocka.h>
#include <cx_errors.h>
#include <ox_bn.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "testutils.h"

/* The same three values interpolate.c passes cx_bn_gf2_n_mul(): N(x) = x^8 +
 * x^4 + x^3 + x + 1 big-endian, the second Montgomery constant the Nano S
 * implementation takes and ignores, and the BN width both use. */
static const uint8_t POLYNOMIAL[2] = {0x01, 0x1B};
static const uint8_t MONTGOMERY_R2[1] = {0x02};
#define GF2_8_MPI_BYTES 16

/*
 * Whether the defect shows up at all depends on what the stub's uninitialised
 * buffer happens to contain: a non-zero first byte stops the walk immediately
 * and yields the right answer by accident. Leaving that to whatever ran before
 * would make this test pass or fail for reasons that have nothing to do with
 * the code under test, so the region the stub's frame is about to occupy is
 * zeroed on purpose first.
 *
 * The array is volatile so that the writes survive -O2/-Os, and larger than
 * the stub's own buffer so that it covers the whole of it whatever the frame
 * layout turns out to be.
 */
#define STACK_PRIMING_BYTES 8192

static void prime_the_stack_with_zeroes(void) {
    volatile uint8_t scratch[STACK_PRIMING_BYTES];

    for (size_t i = 0; i < sizeof(scratch) / sizeof(scratch[0]); i++) {
        scratch[i] = 0;
    }
}

/*
 * A single bignum, and the field operands the multiplication needs. Set up and
 * torn down by fixtures rather than by calls in the test bodies: cmocka leaves
 * a failing test by longjmp, so a teardown written into the body is skipped
 * exactly when it matters, and the BN context would stay locked and fail the
 * next test's setup on CX_NOT_UNLOCKED -- a second failure that says nothing.
 */
typedef struct {
    cx_bn_t x;
    cx_bn_t a;
    cx_bn_t b;
    cx_bn_t r;
    cx_bn_t n;
    cx_bn_t h;
} bn_operands_t;

static int bn_open(void** state) {
    bn_operands_t* ops = test_malloc(sizeof(*ops));

    assert_non_null(ops);
    *state = ops;

    assert_int_equal(cx_bn_lock(GF2_8_MPI_BYTES, 0), CX_OK);
    assert_int_equal(cx_bn_alloc(&ops->x, GF2_8_MPI_BYTES), CX_OK);
    assert_int_equal(cx_bn_alloc(&ops->a, GF2_8_MPI_BYTES), CX_OK);
    assert_int_equal(cx_bn_alloc(&ops->b, GF2_8_MPI_BYTES), CX_OK);
    assert_int_equal(cx_bn_alloc(&ops->r, GF2_8_MPI_BYTES), CX_OK);
    assert_int_equal(cx_bn_alloc_init(&ops->n, GF2_8_MPI_BYTES, POLYNOMIAL,
                                      sizeof(POLYNOMIAL)),
                     CX_OK);
    assert_int_equal(cx_bn_alloc_init(&ops->h, GF2_8_MPI_BYTES, MONTGOMERY_R2,
                                      sizeof(MONTGOMERY_R2)),
                     CX_OK);

    return 0;
}

static int bn_close(void** state) {
    bn_operands_t* ops = *state;

    assert_int_equal(cx_bn_destroy(&ops->x), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->a), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->b), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->r), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->n), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->h), CX_OK);
    assert_int_equal(cx_bn_unlock(), CX_OK);
    test_free(ops);

    return 0;
}

/*
 * Zero, asked directly. Two things are being held here at once: the value --
 * a bignum with no bits set has no bits, whichever way the count is read --
 * and, under the sanitiser, that answering it reads nothing it should not.
 *
 * Without the guard in the stub this test is red either way: the walk stops at
 * the first non-zero byte past the priming above and returns a bit length in
 * the billions, or, when the whole buffer is zero, it leaves the buffer and
 * AddressSanitizer reports the read. Which of the two happens depends on the
 * frame layout; both are failures, and neither is the value asked for.
 */
static void test_zero_has_no_bits(void** state) {
    bn_operands_t* ops = *state;
    uint32_t nbits = UINT32_MAX;

    assert_int_equal(cx_bn_set_u32(ops->x, 0), CX_OK);

    prime_the_stack_with_zeroes();

    assert_int_equal(cx_bn_cnt_bits(ops->x, &nbits), CX_OK);
    assert_int_equal(nbits, 0);
}

/*
 * The other half of the contract, and the reason the guard above returns 0
 * rather than short-circuiting the whole function: what this primitive answers
 * for a non-zero bignum must not change.
 *
 * Worth pinning for a second reason. The SDK header (ox_bn.h) documents
 * cx_bn_cnt_bits() as the "number of bits set to 1"; this implementation
 * returns the bit *length*, and interpolate.c depends on the bit length -- it
 * takes cnt_bits(N) - 1 as the degree of the modulus polynomial, which is 8
 * for 0x11B only under the second reading. The two readings agree on 0 and on
 * nothing else here, so the table below is what says which one is in force.
 */
static void test_a_non_zero_bignum_counts_its_bit_length(void** state) {
    bn_operands_t* ops = *state;
    static const struct {
        uint32_t value;
        uint32_t nbits;
    } cases[] = {
        {1, 1},
        {2, 2},
        {3, 2},
        {4, 3},
        {5, 3},
        {7, 3},
        {0x7F, 7},
        {0x80, 8},
        {0xFF, 8},
        {0x100, 9},
        {0x11B, 9},
        {0x8000, 16},
        {0xFFFF, 16},
        {0x10000, 17},
        {0x800000, 24},
        {0x1000000, 25},
        {0x80000000u, 32},
        {0xFFFFFFFFu, 32},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        uint32_t nbits = UINT32_MAX;

        assert_int_equal(cx_bn_set_u32(ops->x, cases[i].value), CX_OK);
        assert_int_equal(cx_bn_cnt_bits(ops->x, &nbits), CX_OK);

        if (nbits != cases[i].nbits) {
            fail_msg("cnt_bits(0x%X): got %u, expected %u", cases[i].value,
                     nbits, cases[i].nbits);
        }
    }
}

/* One product through whichever cx_bn_gf2_n_mul() this target links. */
static uint8_t gf_mul(bn_operands_t* ops, uint8_t a, uint8_t b) {
    uint32_t product = UINT32_MAX;

    assert_int_equal(cx_bn_set_u32(ops->a, a), CX_OK);
    assert_int_equal(cx_bn_set_u32(ops->b, b), CX_OK);
    assert_int_equal(cx_bn_gf2_n_mul(ops->r, ops->a, ops->b, ops->n, ops->h),
                     CX_OK);
    assert_int_equal(cx_bn_get_u32(ops->r, &product), CX_OK);

    return (uint8_t)product;
}

/*
 * The path the incident actually came down: multiplying by zero in GF(2^8).
 *
 * Legitimate arithmetic -- interpolate() multiplies by zero whenever a
 * Lagrange coefficient vanishes -- and, under TARGET_NANOS, the only thing
 * standing between it and the stub is cx_bn_cnt_bits() on a zero operand. A
 * bit length in the billions there makes the implementation's "both operands
 * are in the field" guard reject the pair with CX_INVALID_PARAMETER, which is
 * how a Shamir round trip turns into SSS_ERROR_INTERPOLATION_FAILURE without
 * a sanitiser ever being involved.
 *
 * Both zero positions and every second operand: 0 * b, a * 0 and 0 * 0 are 511
 * products, cheap enough to take exhaustively rather than sample.
 */
static void test_multiplying_by_zero_gives_zero(void** state) {
    bn_operands_t* ops = *state;

    for (unsigned int other = 0; other <= UINT8_MAX; other++) {
        const uint8_t left = gf_mul(ops, 0, (uint8_t)other);
        const uint8_t right = gf_mul(ops, (uint8_t)other, 0);

        if (left != 0) {
            fail_msg("0 * %u is %u, not 0", other, left);
        }
        if (right != 0) {
            fail_msg("%u * 0 is %u, not 0", other, right);
        }
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_zero_has_no_bits, bn_open,
                                        bn_close),
        cmocka_unit_test_setup_teardown(
            test_a_non_zero_bignum_counts_its_bit_length, bn_open, bn_close),
        cmocka_unit_test_setup_teardown(test_multiplying_by_zero_gives_zero,
                                        bn_open, bn_close),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
