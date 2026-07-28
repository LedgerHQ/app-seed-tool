/*
 * Interoperability regression: a 128-bit SSKR share pair published by
 * Blockchain Commons, combined here through this port's own
 * sskr_combine_shards() -- same math-level entry point already exercised by
 * test_sskr_combine() in sskr.c for the 256-bit vector, no CBOR tag or CRC32
 * involved at this layer.
 *
 * Source of the shares and secret:
 *     https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
 *
 * The secret is generated from the following BIP39 words:
 *     fly mule excess resource treat plunge nose soda reflect adult ramp
 *     planet
 *
 * Independently verified before writing this test by cloning and building
 * Blockchain Commons' own bc-sskr + bc-shamir + bc-crypto-base reference
 * implementation (unrelated to this repository) and combining share 1 with
 * share 2 below: it reconstructs 59f2293a5bce7d4de59e71b4207ac5d2. This is
 * coverage against an independent oracle, not a bug fix -- sskr_combine_shards()
 * is already correct.
 *
 * Only 2 of the 3 published shares are needed (2-of-3 threshold); the third
 * one is not used here. No 192-bit vector exists in the source document
 * (only 128 and 256 bits are published), so none is added here.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "sskr.h"
#include "testutils.h"

static void test_sskr_combine_bc_128bit_vector(void **state) {
    (void) state;

    /* metadata (5 bytes) + value (16 bytes), member_index 0 and 1 of a
     * 2-of-3 single-group set, hex from sskr-test-vector.md unmodified. */
    const uint8_t share1[] = {0x74, 0x72, 0x00, 0x01, 0x00, 0x72, 0x79, 0x90,
                              0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33,
                              0xFD, 0xAE, 0x81, 0x59, 0x13};
    const uint8_t share2[] = {0x74, 0x72, 0x00, 0x01, 0x01, 0x1B, 0x3F, 0x98,
                              0x69, 0x92, 0x4E, 0x46, 0x03, 0x14, 0x4D, 0x4A,
                              0x40, 0x84, 0xF0, 0x90, 0xCC};
    const uint8_t expected_secret[] = {0x59, 0xF2, 0x29, 0x3A, 0x5B, 0xCE,
                                       0x7D, 0x4D, 0xE5, 0x9E, 0x71, 0xB4,
                                       0x20, 0x7A, 0xC5, 0xD2};

    const uint8_t share_len = sizeof(share1);
    const uint8_t *shares[] = {share1, share2};
    uint8_t output[sizeof(expected_secret)];

    int16_t output_len = sskr_combine_shards(shares, share_len, 2, output,
                                             sizeof(output));

    assert_int_equal(output_len, sizeof(expected_secret));
    assert_memory_equal(output, expected_secret, output_len);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sskr_combine_bc_128bit_vector),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
