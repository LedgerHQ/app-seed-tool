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
        cmocka_unit_test(test_drng_with_seed_accepts_max_digest_size),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
