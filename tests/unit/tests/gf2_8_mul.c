/*
 * The GF(2^8) multiplication every SSKR share is built from, over every input
 * it can be given.
 *
 * src/common/sskr/sss/interpolate.c carries two entirely different
 * implementations of it. Under `#if defined(TARGET_NANOS) && !defined
 * API_LEVEL` it defines its own cx_bn_gf2_n_mul(), a shift-and-XOR loop
 * written by hand on top of elementary BN primitives; everywhere else the
 * SDK's cx_bn_gf2_n_mul() is called instead, which on host is the unit
 * harness's own, backed by OpenSSL's BN_GF2m_mod_mul().
 *
 * The existing nanos targets (test_sss_nanos and friends) build the Nano S
 * variant and hold it against the Blockchain Commons vectors, which is a good
 * oracle but a narrow one: those vectors exercise a few hundred products out
 * of the 65536 the field has. Had the two implementations disagreed on a
 * single pair outside that set, shares produced on a Nano S would not have
 * recombined anywhere else, silently, and nothing here would have said so.
 *
 * The field is small enough that "every input it can be given" is literal, so
 * this file sweeps all 65536 pairs against the reference below. Two targets
 * build it: test_gf2_8_mul, where cx_bn_gf2_n_mul() resolves to the harness's
 * OpenSSL-backed one, and test_gf2_8_mul_nanos, which compiles interpolate.c
 * in with TARGET_NANOS so the hand-written variant wins instead. Each one
 * holds its own implementation to the reference; between them, the two
 * implementations are pinned to each other on every pair in the field.
 *
 * The reference is deliberately not a third BN implementation. It is the
 * textbook peasant multiplication, eight lines, readable in one sitting, which
 * is what makes it worth trusting as the thing both sides are measured
 * against.
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

/* The same three values interpolate.c passes cx_bn_gf2_n_mul(), spelled out
 * here rather than shared with it: what they are is part of what this file
 * pins down. */

/* N(x) = x^8 + x^4 + x^3 + x + 1, i.e. 0x11B, big-endian in two bytes. This is
 * the Rijndael/SLIP-39 polynomial; a port that changed it would produce shares
 * nothing else could recombine. */
static const uint8_t POLYNOMIAL[2] = {0x01, 0x1B};

/* 2nd Montgomery constant, R2 = x^(2*t*8) mod N(x) with t = 1. Unused by the
 * Nano S implementation, which takes it and ignores it. */
static const uint8_t MONTGOMERY_R2[1] = {0x02};

/* Minimal BN storage for a GF(256) value, as interpolate.c sizes it. */
#define GF2_8_MPI_BYTES 16

/*
 * Peasant multiplication in GF(2^8): add (XOR) a shifted copy of `a` for every
 * set bit of `b`, reducing by the polynomial whenever the shift carries past
 * degree 8. `0x1b` is N(x) with its x^8 term dropped, which is exactly the
 * substitution x^8 = x^4 + x^3 + x + 1 that the reduction performs.
 */
static uint8_t gf256_mul_reference(uint8_t a, uint8_t b) {
    uint8_t product = 0;

    for (uint8_t i = 0; i < 8; i++) {
        if (b & 1) {
            product ^= a;
        }
        const bool carries = (a & 0x80) != 0;
        a <<= 1;
        if (carries) {
            a ^= 0x1b;
        }
        b >>= 1;
    }

    return product;
}

/*
 * The BN operands cx_bn_gf2_n_mul() needs. Allocated once for a whole sweep:
 * doing it per product would spend the run time in the allocator rather than
 * in the multiplication, and the Nano S implementation allocates its own
 * temporaries internally either way.
 */
typedef struct {
    cx_bn_t a;
    cx_bn_t b;
    cx_bn_t r;
    cx_bn_t n;
    cx_bn_t h;
} gf_operands_t;

/*
 * Fixtures rather than calls at the top and bottom of each test: cmocka leaves
 * a failing test by longjmp, so a teardown written into the test body is
 * skipped exactly when it matters. The BN context would stay locked and the
 * next test would fail on CX_NOT_UNLOCKED in its own setup -- a second failure
 * that says nothing about the field and buries the one that does.
 */
static int gf_open(void** state) {
    gf_operands_t* ops = test_malloc(sizeof(*ops));

    assert_non_null(ops);
    *state = ops;

    assert_int_equal(cx_bn_lock(GF2_8_MPI_BYTES, 0), CX_OK);
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

static int gf_close(void** state) {
    gf_operands_t* ops = *state;

    assert_int_equal(cx_bn_destroy(&ops->a), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->b), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->r), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->n), CX_OK);
    assert_int_equal(cx_bn_destroy(&ops->h), CX_OK);
    assert_int_equal(cx_bn_unlock(), CX_OK);
    test_free(ops);

    return 0;
}

/* One product through whichever cx_bn_gf2_n_mul() this target links. */
static uint8_t gf_mul(gf_operands_t* ops, uint8_t a, uint8_t b) {
    uint32_t product = 0;

    assert_int_equal(cx_bn_set_u32(ops->a, a), CX_OK);
    assert_int_equal(cx_bn_set_u32(ops->b, b), CX_OK);
    assert_int_equal(cx_bn_gf2_n_mul(ops->r, ops->a, ops->b, ops->n, ops->h),
                     CX_OK);
    assert_int_equal(cx_bn_get_u32(ops->r, &product), CX_OK);

    /* A product of two field elements is a field element: anything above 255
     * means the reduction did not happen, which would not show up in the
     * comparison below if the caller truncated it first. */
    assert_true(product <= UINT8_MAX);

    return (uint8_t)product;
}

/*
 * All 65536 pairs. The failure message names the pair rather than leaving a
 * bare "x != y": which inputs disagree is the whole of what makes a report
 * like this actionable.
 */
static void test_every_product_matches_the_reference(void** state) {
    gf_operands_t* ops = *state;

    for (unsigned int a = 0; a <= UINT8_MAX; a++) {
        for (unsigned int b = 0; b <= UINT8_MAX; b++) {
            const uint8_t expected =
                gf256_mul_reference((uint8_t)a, (uint8_t)b);
            const uint8_t actual = gf_mul(ops, (uint8_t)a, (uint8_t)b);

            if (actual != expected) {
                fail_msg("%u * %u: got %u, reference says %u", a, b, actual,
                         expected);
            }
        }
    }
}

/*
 * The property interpolate() actually leans on: in GF(2^8) the inverse of x is
 * x^254, which is how it divides by (xi[i] - xi[j]) -- eleven multiplications
 * building x^254 for every pair of shares. This checks the algebra rather than
 * the reference: x * x^254 == 1 has to hold whatever the reference says, so a
 * fault the reference happened to share would still be caught here.
 *
 * x^254 is built by plain repeated multiplication instead of interpolate()'s
 * square-and-multiply chain. Copying that chain here would only test the copy;
 * what matters is that the exponent is reached at all.
 */
static void test_x_to_the_254_inverts_every_non_zero_element(void** state) {
    gf_operands_t* ops = *state;

    for (unsigned int x = 1; x <= UINT8_MAX; x++) {
        uint8_t inverse = (uint8_t)x;

        for (unsigned int i = 0; i < 253; i++) {
            inverse = gf_mul(ops, inverse, (uint8_t)x);
        }

        const uint8_t product = gf_mul(ops, (uint8_t)x, inverse);

        if (product != 1) {
            fail_msg("%u * %u^254 (= %u) is %u, not 1", x, x, inverse, product);
        }
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(
            test_every_product_matches_the_reference, gf_open, gf_close),
        cmocka_unit_test_setup_teardown(
            test_x_to_the_254_inverts_every_non_zero_element, gf_open,
            gf_close),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
