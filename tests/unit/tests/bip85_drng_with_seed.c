#include <cmocka.h>
#include <lcx_sha3.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/bip85/bip85_internal.h"

#define BIP85_DRNG_MAX_DIGEST_SIZE 256

static uint8_t seed[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                         0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                         0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                         0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

// bolos_ux_bip85_drng_with_seed() is a thin wrapper around cx_shake256_hash()
// -- no derivation, no BOLOS syscall -- so its output for a given
// seed/digest_length is checked directly against an independent call to the
// same real cx_shake256_hash(), rather than against a second BIP85 helper:
// there is no simpler oracle than the function it wraps.
static void test_drng_with_seed_matches_cx_shake256_hash(void** state) {
    (void)state;

    uint8_t digest[64];
    uint8_t expected[sizeof(digest)];

    bool ok = bolos_ux_bip85_drng_with_seed(seed, sizeof(seed), digest,
                                            sizeof(digest));
    assert_true(ok);

    assert_int_equal(
        cx_shake256_hash(seed, sizeof(seed), expected, sizeof(expected)),
        CX_OK);
    assert_memory_equal(digest, expected, sizeof(digest));
}

// The BIP85-DRNG-SHAKE256 test vector the specification publishes, for:
//
//   MASTER BIP32 ROOT KEY: xprv9s21ZrQH143K2LBWUUQRFXhucrQqBpKdRRxNVq2zBq
//                          sx8HVqFk2uYo8kmbaLLHRdqtQpUm98uKfu3vca1LqdGhU
//                          tyoFnCNkfmXRyPXLjbKb
//   PATH:                  m/83696968'/0'/0'
//   DERIVED ENTROPY:       efecfbcc...e48618f7  (the 64 bytes below)
//   DRNG(80 bytes):        b78b1ee6...02991111
//
// Same reason this is reachable as for the DICE vector: the specification
// prints the 64-byte derived entropy, which is precisely the input
// bolos_ux_bip85_drng_test() hands this function, so the vector needs no
// BIP32 derivation and no BOLOS syscall.
//
// This is the assertion the two tests above cannot make. Checking the
// function against cx_shake256_hash() shows the wrapper forwards its
// arguments correctly, and that is worth having, but it would hold just as
// well if BIP-85's DRNG were something other than SHAKE256 seeded with the
// 64 entropy bytes. Only an externally published value establishes that it
// is not.
//
// Do not reach for `bipsea` 3.2.0 to reproduce this: its `derive -a drng`
// builds m/83696968'/0'/0'/{index}', a four-component path, where the
// vector's is the three-component m/83696968'/0'/0' -- bipsea.py appends
// "/0'/{index}'" to a path that already ends in the application number 0'.
// It therefore disagrees with the specification's own vector on the entropy
// it feeds the DRNG. The discrepancy is in the path, not in SHAKE256:
// SHAKE256 over the entropy the specification publishes reproduces the
// published 80 bytes exactly. The expected bytes below come from the BIP.
//
// Not const, for the same reason `seed` above is not: the function takes a
// non-const `uint8_t *`.
static uint8_t drng_vector_entropy[] = {
    0xef, 0xec, 0xfb, 0xcc, 0xff, 0xea, 0x31, 0x32, 0x14, 0x23, 0x2d,
    0x29, 0xe7, 0x15, 0x63, 0xd9, 0x41, 0x22, 0x9a, 0xfb, 0x43, 0x38,
    0xc2, 0x1f, 0x95, 0x17, 0xc4, 0x1a, 0xaa, 0x0d, 0x16, 0xf0, 0x0b,
    0x83, 0xd2, 0xa0, 0x9e, 0xf7, 0x47, 0xe7, 0xa6, 0x4e, 0x8e, 0x2b,
    0xd5, 0xa1, 0x48, 0x69, 0xe6, 0x93, 0xda, 0x66, 0xce, 0x94, 0xac,
    0x2d, 0xa5, 0x70, 0xab, 0x7e, 0xe4, 0x86, 0x18, 0xf7};

static const uint8_t drng_vector_output[80] = {
    0xb7, 0x8b, 0x1e, 0xe6, 0xb3, 0x45, 0xea, 0xe6, 0x83, 0x6c, 0x2d, 0x53,
    0xd3, 0x3c, 0x64, 0xcd, 0xaf, 0x9a, 0x69, 0x64, 0x87, 0xbe, 0x81, 0xb0,
    0x3e, 0x82, 0x2d, 0xc8, 0x4b, 0x3f, 0x1c, 0xd8, 0x83, 0xd7, 0x55, 0x9e,
    0x53, 0xd1, 0x75, 0xf2, 0x43, 0xe4, 0xc3, 0x49, 0xe8, 0x22, 0xa9, 0x57,
    0xbb, 0xff, 0x92, 0x24, 0xbc, 0x5d, 0xde, 0x94, 0x92, 0xef, 0x54, 0xe8,
    0xa4, 0x39, 0xf6, 0xbc, 0x8c, 0x73, 0x55, 0xb8, 0x7a, 0x92, 0x5a, 0x37,
    0xee, 0x40, 0x5a, 0x75, 0x02, 0x99, 0x11, 0x11};

static void test_drng_with_seed_matches_bip85_vector(void** state) {
    (void)state;

    uint8_t digest[sizeof(drng_vector_output)];

    bool ok = bolos_ux_bip85_drng_with_seed(drng_vector_entropy,
                                            sizeof(drng_vector_entropy), digest,
                                            sizeof(digest));
    assert_true(ok);

    assert_memory_equal(digest, drng_vector_output, sizeof(digest));
}

// digest_length == BIP85_DRNG_MAX_DIGEST_SIZE is the upper bound the
// LEDGER_ASSERT allows (`digest_length <= BIP85_DRNG_MAX_DIGEST_SIZE`) --
// exercised here rather than only an arbitrary smaller length, since it is
// the one value where the assert's condition is exactly true rather than
// slack.
static void test_drng_with_seed_accepts_max_digest_size(void** state) {
    (void)state;

    uint8_t digest[BIP85_DRNG_MAX_DIGEST_SIZE];
    uint8_t expected[sizeof(digest)];

    bool ok = bolos_ux_bip85_drng_with_seed(seed, sizeof(seed), digest,
                                            sizeof(digest));
    assert_true(ok);

    assert_int_equal(
        cx_shake256_hash(seed, sizeof(seed), expected, sizeof(expected)),
        CX_OK);
    assert_memory_equal(digest, expected, sizeof(digest));
}

// digest_length > BIP85_DRNG_MAX_DIGEST_SIZE is deliberately not exercised:
// that path is a LEDGER_ASSERT, not a returned error, and LEDGER_ASSERT
// terminates the process on this host build -- same as every other
// LEDGER_ASSERT-guarded path in this file (e.g. the entropy-derivation
// asserts). There is no return value or output buffer left to assert on
// afterwards, only a crashed test binary. Documented here rather than
// worked around.

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_drng_with_seed_matches_cx_shake256_hash),
        cmocka_unit_test(test_drng_with_seed_matches_bip85_vector),
        cmocka_unit_test(test_drng_with_seed_accepts_max_digest_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
