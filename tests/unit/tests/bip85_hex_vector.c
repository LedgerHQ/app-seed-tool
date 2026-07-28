#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "testutils.h"

#define BIP85_ENTROPY_LENGTH 64

// bip85_entropy_from_key() covers only the HMAC-SHA512("bip-entropy-from-k",
// key) half of bolos_ux_bip85_entropy(); the BIP32 derivation that produces
// `key` in the first place (purpose 83696968', application 128169', a
// separate chain code, all-hardened components) goes through
// os_derive_bip32_no_throw(), a BOLOS syscall against the device's real seed
// with no host equivalent, so it cannot be exercised here -- same limitation
// already documented for test_bip85_bip39_entropy.
extern bool bip85_entropy_from_key(const uint8_t key[32], uint8_t* out,
                                   size_t out_len);

// Vector 1 is the official BIP-85 HEX-application test vector published in
// bip-0085.mediawiki (bitcoin/bips) itself, for the official master xprv on
// m/83696968'/128169'/64'/0'. Vector 2 is the same path with index 1
// (no official vector published for this index). Both were independently
// cross-checked before writing this test: (a) bipsea (an independent,
// third-party Python implementation of BIP-85, `pip install bipsea`) --
// `bipsea derive -a hex -n 64 -i <index> -x <master_xprv>` -- and (b) a
// from-scratch derivation with bip32utils (Python, outside this repo) plus
// hmac/hashlib.sha512. All three sources (spec, bipsea, manual derivation)
// agree exactly for vector 1; bipsea and the manual derivation agree exactly
// for vector 2.
typedef struct {
    uint8_t key[32];
    uint8_t entropy[BIP85_ENTROPY_LENGTH];
} bip85_hex_vector_t;

static const bip85_hex_vector_t vectors[] = {
    {
        // m/83696968'/128169'/64'/0'
        .key = {0x99, 0xa4, 0x26, 0x2e, 0xb2, 0xfc, 0x79, 0x23,
                0xd2, 0xd7, 0x87, 0x90, 0x02, 0x3e, 0x5d, 0x5e,
                0xa4, 0x4e, 0x89, 0x74, 0x21, 0xa9, 0xea, 0xe0,
                0x69, 0x57, 0x9a, 0x39, 0x3d, 0x2f, 0x94, 0x53},
        .entropy = {0x49, 0x2d, 0xb4, 0x69, 0x8c, 0xf3, 0xb7, 0x3a, 0x5a, 0x24,
                    0x99, 0x8a, 0xa3, 0xe9, 0xd7, 0xfa, 0x96, 0x27, 0x5d, 0x85,
                    0x72, 0x4a, 0x91, 0xe7, 0x1a, 0xa2, 0xd6, 0x45, 0x44, 0x2f,
                    0x87, 0x85, 0x55, 0xd0, 0x78, 0xfd, 0x1f, 0x1f, 0x67, 0xe3,
                    0x68, 0x97, 0x6f, 0x04, 0x13, 0x7b, 0x1f, 0x7a, 0x0d, 0x19,
                    0x23, 0x21, 0x36, 0xca, 0x50, 0xc4, 0x46, 0x14, 0xaf, 0x72,
                    0xb5, 0x58, 0x2a, 0x5c},
    },
    {
        // m/83696968'/128169'/64'/1'
        .key = {0x08, 0xd8, 0xb6, 0x6c, 0xda, 0xa9, 0x23, 0x41,
                0x31, 0xf2, 0xe7, 0xc9, 0xbc, 0x98, 0x44, 0x50,
                0x83, 0xd8, 0xdf, 0x20, 0xc3, 0x02, 0xc2, 0xec,
                0x73, 0x08, 0x7e, 0x13, 0xc8, 0x94, 0xfe, 0x2f},
        .entropy = {0x3c, 0x7c, 0xd8, 0xfc, 0x51, 0xf7, 0x38, 0x1c, 0x83, 0xc9,
                    0x1e, 0x83, 0x8f, 0x89, 0x34, 0x05, 0xb9, 0xfd, 0xf1, 0x4b,
                    0x36, 0xc8, 0x47, 0x53, 0x51, 0x73, 0xc9, 0xef, 0x79, 0x72,
                    0x30, 0x95, 0xd3, 0xba, 0x70, 0xd2, 0x8a, 0x89, 0x81, 0x12,
                    0x9e, 0xf3, 0x93, 0x74, 0x01, 0xe4, 0x02, 0xcd, 0x8e, 0x70,
                    0x46, 0xf1, 0x7f, 0xc9, 0xd6, 0x5d, 0x04, 0x88, 0x10, 0x76,
                    0x78, 0xf2, 0x13, 0xad},
    },
};

static void test_bip85_hex_vector_index_0(void** state) {
    (void)state;
    uint8_t entropy[BIP85_ENTROPY_LENGTH];

    assert_true(
        bip85_entropy_from_key(vectors[0].key, entropy, BIP85_ENTROPY_LENGTH));
    assert_memory_equal(entropy, vectors[0].entropy, BIP85_ENTROPY_LENGTH);
}

static void test_bip85_hex_vector_index_1(void** state) {
    (void)state;
    uint8_t entropy[BIP85_ENTROPY_LENGTH];

    assert_true(
        bip85_entropy_from_key(vectors[1].key, entropy, BIP85_ENTROPY_LENGTH));
    assert_memory_equal(entropy, vectors[1].entropy, BIP85_ENTROPY_LENGTH);
}

static void test_bip85_entropy_from_key_deterministic(void** state) {
    (void)state;
    uint8_t first[BIP85_ENTROPY_LENGTH];
    uint8_t second[BIP85_ENTROPY_LENGTH];

    assert_true(
        bip85_entropy_from_key(vectors[0].key, first, BIP85_ENTROPY_LENGTH));
    assert_true(
        bip85_entropy_from_key(vectors[0].key, second, BIP85_ENTROPY_LENGTH));
    assert_memory_equal(first, second, BIP85_ENTROPY_LENGTH);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip85_hex_vector_index_0),
        cmocka_unit_test(test_bip85_hex_vector_index_1),
        cmocka_unit_test(test_bip85_entropy_from_key_deterministic),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
