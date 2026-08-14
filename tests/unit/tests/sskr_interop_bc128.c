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
 * That document publishes its shares as ByteWords only, so the bytes below
 * were obtained from it this way: each 29-word share was decoded with the
 * 256-word table of BCR-2020-012, its trailing CRC-32 recomputed over the
 * preceding 25 bytes and compared (all three match), and the 21-byte
 * serialized shard taken out from between the CBOR header and that CRC-32.
 * Nothing was recomputed with this port. The three shares decode to
 *
 *     747200010041c07bbc6ae0b757ac459350ac76e051
 *     74720001017a8ed97600e5ac541199c54ebee621f7
 *     7472000102375c2433beea8151cde63f6c884d7906
 *
 * and any two of them reconstruct 59f2293a5bce7d4de59e71b4207ac5d2 with a
 * valid SLIP-39 share digest, checked against an independent GF(2^8)
 * implementation written from the specification. This is coverage against an
 * external oracle, not a bug fix -- sskr_combine_shards() is already correct.
 *
 * Only the shard layer is used here, which is why the document's bytes fit
 * unchanged: the wire frames it publishes carry the now-superseded CBOR tag
 * 309, and that layer is not involved below.
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
     * 2-of-3 single-group set, as decoded from the ByteWords published in
     * sskr-test-vector.md. */
    const uint8_t share1[] = {0x74, 0x72, 0x00, 0x01, 0x00, 0x41, 0xC0, 0x7B,
                              0xBC, 0x6A, 0xE0, 0xB7, 0x57, 0xAC, 0x45, 0x93,
                              0x50, 0xAC, 0x76, 0xE0, 0x51};
    const uint8_t share2[] = {0x74, 0x72, 0x00, 0x01, 0x01, 0x7A, 0x8E, 0xD9,
                              0x76, 0x00, 0xE5, 0xAC, 0x54, 0x11, 0x99, 0xC5,
                              0x4E, 0xBE, 0xE6, 0x21, 0xF7};
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
