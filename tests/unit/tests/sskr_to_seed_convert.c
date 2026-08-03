/*
 * Coverage for bolos_ux_sskr_to_seed_convert() (src/common/sskr/seed_sskr.c).
 *
 * A gcov run over the whole suite gives every line of this function, including
 * its signature, an execution count of zero: no test has ever called it. It is
 * the function that chains the three steps a user's SSKR recovery goes through
 *
 *     bolos_ux_sskr_combine()          -- shards back to the entropy
 *     bolos_ux_bip39_mnemonic_encode() -- entropy to the BIP-39 mnemonic
 *     bolos_ux_bip39_mnemonic_to_seed()-- mnemonic to the 64-byte seed
 *
 * and each of the three is covered on its own elsewhere, so what is untested is
 * the wiring between them: which buffer and which length each step hands the
 * next one.
 *
 * The shards, the entropy and the mnemonic are the 256-bit Blockchain Commons
 * vector already used by tests/roundtrip.c. The expected seed is not taken from
 * this implementation: it was computed independently in Python
 * (hashlib.pbkdf2_hmac) over the SHA-512 digest of the mnemonic -- the mnemonic
 * is 152 characters, so it goes through the >128-character pre-hash -- with the
 * salt "mnemonic" and 2048 rounds.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "sskr/common_sskr.h"
#include "testutils.h"

static const unsigned char expected_mnemonic[] =
    "toe priority custom gauge jacket theme arrest bargain gloom wide ill fit "
    "eagle prepare capable fish limb cigar reform other priority speak rough "
    "imitate";

/* Two of the three 2-of-3 shards, in the wire form the share entry screen
 * produces: CBOR tag, long-form byte string header, then the shard. */
static const uint8_t sskr_shares_wire[] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x01, 0x00, 0x00, 0x01, 0x00, 0xFF, 0x0F,
    0xBB, 0x2A, 0x8B, 0xCC, 0x6F, 0x00, 0xC8, 0xE6, 0x83, 0xDF, 0xB0, 0xEB,
    0x5F, 0x1C, 0x55, 0xEF, 0x12, 0xD9, 0x4B, 0x62, 0x97, 0x42, 0x42, 0xDA,
    0x21, 0x11, 0xBA, 0xC6, 0x5F, 0xAA, 0xD9, 0x9D, 0x75, 0x58, 0x25, 0x01,
    0x00, 0x00, 0x01, 0x01, 0xB4, 0x8E, 0x81, 0x9F, 0xBB, 0x8A, 0x1C, 0xC3,
    0xCF, 0xFB, 0x10, 0xBB, 0xC7, 0xB7, 0x96, 0xBC, 0xBD, 0xB3, 0x4F, 0x1E,
    0x21, 0xCE, 0x04, 0x94, 0x48, 0x1E, 0x79, 0x8C, 0x0D, 0x7E, 0xEA, 0xA2};

/* PBKDF2-HMAC-SHA512(SHA512(mnemonic), "mnemonic", 2048) truncated to 64
 * bytes, computed outside this implementation. */
static const uint8_t expected_seed[64] = {
    0x27, 0x18, 0x6B, 0x7F, 0x5A, 0xA1, 0xD6, 0xC2, 0xBC, 0x81, 0xCA,
    0x9A, 0xB8, 0xD4, 0x3A, 0x47, 0x8F, 0xDB, 0x80, 0xD5, 0x26, 0x04,
    0x9D, 0x7A, 0x28, 0x09, 0x89, 0xCA, 0x02, 0xDA, 0x86, 0xA2, 0xB3,
    0xB2, 0x7D, 0xD0, 0x08, 0x02, 0xA5, 0xC7, 0x96, 0xCA, 0x4A, 0x0E,
    0x51, 0x58, 0x45, 0x66, 0x7D, 0xEE, 0x32, 0xE7, 0x6A, 0xED, 0x18,
    0x49, 0x8D, 0xEA, 0x8A, 0x20, 0x61, 0xFA, 0x0D, 0x9A};

static void test_shards_convert_to_mnemonic_and_seed(void** state) {
    (void)state;

    /* bolos_ux_sskr_combine() erases the shard buffer on failure, so it gets a
     * writable copy rather than the constant above. */
    unsigned char shares[sizeof(sskr_shares_wire)];
    unsigned char words_buffer[sizeof(expected_mnemonic)];
    unsigned int words_buffer_length = sizeof(words_buffer);
    unsigned char seed[64];

    memcpy(shares, sskr_shares_wire, sizeof(shares));
    memset(words_buffer, 0x00, sizeof(words_buffer));
    memset(seed, 0x5A, sizeof(seed));

    assert_true(bolos_ux_sskr_to_seed_convert(shares, sizeof(shares), 2,
                                              words_buffer,
                                              &words_buffer_length, seed));

    /* The mnemonic length the encode step reported, propagated to the caller
     * and used as the length fed to the seed derivation. */
    assert_int_equal(words_buffer_length, sizeof(expected_mnemonic) - 1);
    assert_memory_equal(words_buffer, expected_mnemonic, words_buffer_length);
    assert_memory_equal(seed, expected_seed, sizeof(expected_seed));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_shards_convert_to_mnemonic_and_seed),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
