#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "testutils.h"

#define BIP85_ENTROPY_LENGTH 64

// bip85_entropy_from_key() covers only the HMAC-SHA512("bip-entropy-from-k",
// key) half of bolos_ux_bip85_entropy(); the BIP32 derivation that produces
// `key` in the first place goes through os_derive_bip32_no_throw(), a BOLOS
// syscall against the device's real seed with no host equivalent, so it
// cannot be exercised here. The three keys below stand in for that
// derivation's output.
extern bool bip85_entropy_from_key(const uint8_t key[32], uint8_t* out,
                                   size_t out_len);

// Independent oracle for all three vectors: the official BIP-85 master
// xprv (bip-0085.mediawiki, bitcoin/bips), derived on
// m/83696968'/39'/0'/{12,18,24}'/0' with bip32utils (Python, outside this
// repo) to get the 32-byte key below, then cross-checked against this
// repo's own HMAC-SHA512("bip-entropy-from-k", key) computed independently
// with Python's hmac/hashlib against the entropy the spec publishes for
// each path -- all three matched exactly. The mnemonics are copied
// verbatim from the same spec. Neither the master xprv nor the derived
// keys are anything other than these public test vectors.

typedef struct {
    uint8_t words;
    uint8_t key[32];
    uint8_t entropy_len;
    uint8_t entropy[32];
    const char* mnemonic;
} bip85_bip39_vector_t;

static const bip85_bip39_vector_t vectors[] = {
    {
        .words = 12,
        .key = {0x83, 0x69, 0x51, 0xa7, 0x92, 0x90, 0x20, 0xf8,
                0xb4, 0x45, 0x60, 0x34, 0x26, 0x7f, 0xde, 0xbc,
                0x90, 0x91, 0x27, 0x4a, 0x4e, 0x46, 0x67, 0x7c,
                0x7d, 0x41, 0x80, 0xef, 0x0c, 0x0e, 0xce, 0xa7},
        .entropy_len = 16,
        .entropy = {0x62, 0x50, 0xb6, 0x8d, 0xaf, 0x74, 0x6d, 0x12, 0xa2, 0x4d,
                    0x58, 0xb4, 0x78, 0x7a, 0x71, 0x4b},
        .mnemonic = "girl mad pet galaxy egg matter matrix prison refuse "
                    "sense ordinary nose",
    },
    {
        .words = 18,
        .key = {0x65, 0x2a, 0x04, 0x32, 0x90, 0xed, 0x06, 0x66,
                0xe9, 0x22, 0xb2, 0xa9, 0xfd, 0x61, 0x89, 0xd8,
                0xf7, 0x9c, 0x89, 0x0b, 0x5b, 0x06, 0x81, 0x5b,
                0xbe, 0x05, 0x80, 0x00, 0x19, 0xcd, 0xfb, 0xd6},
        .entropy_len = 24,
        .entropy = {0x93, 0x80, 0x33, 0xed, 0x8b, 0x12, 0x69, 0x84,
                    0x49, 0xd4, 0xbb, 0xca, 0x3c, 0x85, 0x3c, 0x66,
                    0xb2, 0x93, 0xea, 0x1b, 0x1c, 0xe9, 0xd9, 0xdc},
        .mnemonic = "near account window bike charge season chef number "
                    "sketch tomorrow excuse sniff circle vital hockey "
                    "outdoor supply token",
    },
    {
        .words = 24,
        .key = {0xce, 0x7c, 0x1b, 0xae, 0xee, 0xd4, 0x18, 0x14,
                0xca, 0xc0, 0x4b, 0x73, 0xd3, 0xf7, 0x1f, 0x79,
                0x73, 0xb6, 0x45, 0xac, 0xaa, 0x4c, 0x71, 0x07,
                0x1d, 0xb0, 0x99, 0xc1, 0x3b, 0xcb, 0x44, 0x38},
        .entropy_len = 32,
        .entropy = {0xae, 0x13, 0x1e, 0x23, 0x12, 0xcd, 0xc6, 0x13,
                    0x31, 0x54, 0x2e, 0xfe, 0x0d, 0x10, 0x77, 0xba,
                    0xc5, 0xea, 0x80, 0x3a, 0xdf, 0x24, 0xb3, 0x13,
                    0xa4, 0xf0, 0xe4, 0x8e, 0x9c, 0x51, 0xf3, 0x7f},
        .mnemonic = "puppy ocean match cereal symbol another shed magic "
                    "wrap hammer bulb intact gadget divorce twin tonight "
                    "reason outdoor destroy simple truth cigar social "
                    "volcano",
    },
};

static void run_vector(const bip85_bip39_vector_t* vector) {
    uint8_t entropy[BIP85_ENTROPY_LENGTH];

    assert_true(
        bip85_entropy_from_key(vector->key, entropy, BIP85_ENTROPY_LENGTH));
    assert_memory_equal(entropy, vector->entropy, vector->entropy_len);

    unsigned char mnemonic_out[256];
    size_t expected_len = strlen(vector->mnemonic);

    unsigned int encoded_len = bolos_ux_bip39_mnemonic_encode(
        entropy, vector->entropy_len, mnemonic_out, sizeof(mnemonic_out));

    assert_int_equal(encoded_len, expected_len);
    assert_memory_equal(mnemonic_out, vector->mnemonic, expected_len);
}

static void test_bip85_bip39_12_words(void** state) {
    (void)state;
    run_vector(&vectors[0]);
}

static void test_bip85_bip39_18_words(void** state) {
    (void)state;
    run_vector(&vectors[1]);
}

static void test_bip85_bip39_24_words(void** state) {
    (void)state;
    run_vector(&vectors[2]);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip85_bip39_12_words),
        cmocka_unit_test(test_bip85_bip39_18_words),
        cmocka_unit_test(test_bip85_bip39_24_words),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
