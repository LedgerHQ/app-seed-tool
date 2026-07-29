#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "testutils.h"

const unsigned char bip39_mnemonic[] =
    "toe priority custom gauge jacket theme arrest bargain gloom wide ill fit "
    "eagle prepare capable fish limb cigar reform other priority speak rough "
    "imitate";

const unsigned char seed[] = {
    0x27, 0x18, 0x6B, 0x7F, 0x5A, 0xA1, 0xD6, 0xC2, 0xBC, 0x81, 0xCA,
    0x9A, 0xB8, 0xD4, 0x3A, 0x47, 0x8F, 0xDB, 0x80, 0xD5, 0x26, 0x04,
    0x9D, 0x7A, 0x28, 0x09, 0x89, 0xCA, 0x02, 0xDA, 0x86, 0xA2, 0xB3,
    0xB2, 0x7D, 0xD0, 0x08, 0x02, 0xA5, 0xC7, 0x96, 0xCA, 0x4A, 0x0E,
    0x51, 0x58, 0x45, 0x66, 0x7D, 0xEE, 0x32, 0xE7, 0x6A, 0xED, 0x18,
    0x49, 0x8D, 0xEA, 0x8A, 0x20, 0x61, 0xFA, 0x0D, 0x9A};

static void test_bip39(void** state) {
    uint8_t buffer[64] = {0};

    bolos_ux_bip39_mnemonic_to_seed(bip39_mnemonic, sizeof(bip39_mnemonic) - 1,
                                    buffer);

    assert_memory_equal(buffer, seed, sizeof(seed));
}

// bolos_ux_bip39_mnemonic_to_seed() only pre-hashes the mnemonic (SHA-512,
// truncated to 64 bytes) before PBKDF2 when its length exceeds 128 bytes;
// below that threshold it feeds PBKDF2 the raw mnemonic bytes directly. The
// test above uses a 152-character mnemonic and exercises only the
// pre-hashed branch -- the far more common case in practice (a
// standard-length phrase stays well under 128 characters) was untested.
// This drives that branch directly against an independently computed
// PBKDF2-HMAC-SHA512 vector (salt "mnemonic", 2048 iterations); the
// mnemonic is the same 12-word vector already used by the roundtrip tests
// below.
static void test_bip39_seed_short_mnemonic(void** state) {
    (void)state;
    static const unsigned char mnemonic[] =
        "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
        "nose";
    static const uint8_t expected_seed[64] = {
        0x24, 0x2a, 0xbe, 0x4d, 0xed, 0x79, 0x03, 0xcd, 0xb6, 0xcf, 0xcb,
        0x3e, 0x0a, 0xed, 0x5e, 0x62, 0x7d, 0x95, 0x00, 0x52, 0x09, 0x4e,
        0x51, 0xa7, 0x40, 0x47, 0x89, 0x2d, 0xc1, 0xed, 0x1d, 0xb5, 0x37,
        0xf0, 0x0c, 0xc0, 0x42, 0xd5, 0xd8, 0x3c, 0x1e, 0x40, 0x7c, 0xc7,
        0x3f, 0xfc, 0xe5, 0x5f, 0xf5, 0xfb, 0xaa, 0x4f, 0x24, 0x04, 0xd8,
        0x19, 0xef, 0xdf, 0xd2, 0x61, 0x7b, 0x49, 0xeb, 0xe3};
    uint8_t out_seed[64];

    bolos_ux_bip39_mnemonic_to_seed((unsigned char*)mnemonic,
                                    sizeof(mnemonic) - 1, out_seed);

    assert_memory_equal(out_seed, expected_seed, sizeof(expected_seed));
}

// Exercises the exact 128/129-byte boundary of the pre-hash guard above.
// bolos_ux_bip39_mnemonic_to_seed() does not validate its input as a real
// BIP-39 phrase -- it hashes whatever bytes it is given -- so an arbitrary,
// deterministic buffer (repeating A-Z) is enough. Both expected seeds are
// computed independently (128: direct PBKDF2 on the 128 raw bytes; 129:
// PBKDF2 on SHA-512(129 bytes) truncated to 64), confirming the
// implementation lands on the correct side of the guard at each length.
static void test_bip39_seed_length_boundary(void** state) {
    (void)state;
    unsigned char buf[129];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = 'A' + (i % 26);
    }

    static const uint8_t expected_seed_128[64] = {
        0x9b, 0xc3, 0x17, 0x45, 0x6c, 0xfa, 0x41, 0x0c, 0xb7, 0x07, 0xb7,
        0x91, 0xc2, 0x2e, 0x59, 0xad, 0xc9, 0x57, 0xf1, 0x4f, 0xcd, 0x91,
        0xb9, 0x42, 0xf7, 0xec, 0x95, 0x15, 0x74, 0xb2, 0xfa, 0xc2, 0x5b,
        0x63, 0x3e, 0x02, 0x52, 0xbb, 0x76, 0x13, 0xa1, 0xaa, 0xcf, 0xa8,
        0x6c, 0x2f, 0x07, 0x27, 0xd0, 0xed, 0x04, 0xed, 0xd3, 0x83, 0xb2,
        0x86, 0x7d, 0x05, 0xb6, 0xb3, 0xce, 0x83, 0xd5, 0x12};
    static const uint8_t expected_seed_129[64] = {
        0xd2, 0x96, 0xf1, 0xf4, 0xaa, 0x20, 0xb3, 0xfb, 0x72, 0x59, 0xfc,
        0x7b, 0xbf, 0xcc, 0xd1, 0x43, 0x26, 0x39, 0x49, 0x22, 0x13, 0x51,
        0xb1, 0xff, 0x47, 0x82, 0xb9, 0x12, 0xd2, 0x97, 0x3b, 0x79, 0x95,
        0xa2, 0x40, 0x6d, 0x2f, 0x1f, 0x5f, 0x8d, 0xb8, 0xc4, 0xc6, 0xe1,
        0x52, 0xfa, 0xb7, 0x26, 0xf1, 0xc2, 0xb1, 0x48, 0x4b, 0x9e, 0x57,
        0x0b, 0x45, 0xff, 0x6f, 0xb5, 0x2e, 0x6a, 0x01, 0xfc};
    uint8_t seed_128[64];
    uint8_t seed_129[64];

    bolos_ux_bip39_mnemonic_to_seed(buf, 128, seed_128);
    assert_memory_equal(seed_128, expected_seed_128, sizeof(expected_seed_128));

    bolos_ux_bip39_mnemonic_to_seed(buf, 129, seed_129);
    assert_memory_equal(seed_129, expected_seed_129, sizeof(expected_seed_129));
}

// All twelve words exist in the wordlist; only the last word differs from
// the valid vector (whose last word is "nose") -- the checksum byte no
// longer matches, so decode() must reject it despite every word being
// individually valid.
static const unsigned char bip39_bad_checksum_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "abandon";

static void test_bip39_decode_bad_checksum(void** state) {
    (void)state;
    unsigned char bits[32 + 1];

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        bip39_bad_checksum_mnemonic, sizeof(bip39_bad_checksum_mnemonic) - 1,
        bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// Same phrase as above, but the last word is replaced with a string that
// does not appear in the BIP-39 English wordlist.
static const unsigned char bip39_unknown_word_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "notarealbip39word";

static void test_bip39_decode_unknown_word(void** state) {
    (void)state;
    unsigned char bits[32 + 1];

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        bip39_unknown_word_mnemonic, sizeof(bip39_unknown_word_mnemonic) - 1,
        bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// bolos_ux_bip39_mnemonic_decode() rejects a phrase purely on word count
// before looking at word content, so the content here does not matter --
// only the number of space-separated words.
static void test_bip39_decode_wrong_length(void** state) {
    (void)state;
    static const unsigned int word_counts[] = {11, 13, 17, 19, 20, 23, 25};
    unsigned char bits[32 + 1];
    unsigned char mnemonic[9 * 25];

    for (size_t i = 0; i < sizeof(word_counts) / sizeof(word_counts[0]); i++) {
        unsigned int words = word_counts[i];
        size_t offset = 0;

        for (unsigned int w = 0; w < words; w++) {
            memcpy(mnemonic + offset, "abandon", 7);
            offset += 7;
            if (w < words - 1) {
                mnemonic[offset++] = ' ';
            }
        }

        unsigned int result = bolos_ux_bip39_mnemonic_decode(
            mnemonic, offset, bits, sizeof(bits));

        assert_int_equal(result, 0);
    }
}

// 16 bytes of entropy encode to a 74-character mnemonic; an output buffer of
// 10 bytes is far too small for even the first two words, so encode() must
// fail cleanly (return 0) instead of writing past the buffer it was given.
static void test_bip39_encode_insufficient_buffer(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                        0x0c, 0x0d, 0x0e, 0x0f};
    unsigned char out[10];

    unsigned int result = bolos_ux_bip39_mnemonic_encode(
        entropy, sizeof(entropy), out, sizeof(out));

    assert_int_equal(result, 0);
}

// bolos_ux_bip39_mnemonic_encode() rejects seed_len before touching `out` if
// it fails any of three conditions: not a multiple of 4, below 16, or above
// 32. Only the valid lengths (16/24/32) are exercised elsewhere (the
// roundtrip tests below); each case here isolates exactly one condition,
// satisfying the other two, and a generously sized `out` confirms the
// rejection is a clean early return (0) rather than a controlled partial
// write.
static void test_bip39_encode_rejects_invalid_seed_len(void** state) {
    (void)state;
    static const uint8_t seed_bytes[36] = {0};
    unsigned char out[256];

    // Not a multiple of 4, but within [16, 32].
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 17, out, sizeof(out)), 0);

    // Multiple of 4, but below 16.
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 12, out, sizeof(out)), 0);

    // Multiple of 4, but above 32.
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 36, out, sizeof(out)), 0);
}

// The three entropy/mnemonic pairs below are the same independently-verified
// vectors already used by tests/bip85_bip39_entropy.c (12/18/24 words).
static void run_roundtrip(const uint8_t* entropy, uint8_t entropy_len,
                          const char* mnemonic) {
    unsigned char encoded[256];
    size_t mnemonic_len = strlen(mnemonic);

    unsigned int encoded_len = bolos_ux_bip39_mnemonic_encode(
        entropy, entropy_len, encoded, sizeof(encoded));

    assert_int_equal(encoded_len, mnemonic_len);
    assert_memory_equal(encoded, mnemonic, mnemonic_len);

    unsigned char bits[32 + 1];
    unsigned int decode_result = bolos_ux_bip39_mnemonic_decode(
        (const unsigned char*)mnemonic, mnemonic_len, bits, sizeof(bits));

    assert_int_equal(decode_result, 1);
    assert_memory_equal(bits, entropy, entropy_len);
}

static void test_bip39_roundtrip_12_words(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x62, 0x50, 0xb6, 0x8d, 0xaf, 0x74,
                                        0x6d, 0x12, 0xa2, 0x4d, 0x58, 0xb4,
                                        0x78, 0x7a, 0x71, 0x4b};

    run_roundtrip(entropy, sizeof(entropy),
                  "girl mad pet galaxy egg matter matrix prison refuse "
                  "sense ordinary nose");
}

static void test_bip39_roundtrip_18_words(void** state) {
    (void)state;
    static const uint8_t entropy[24] = {
        0x93, 0x80, 0x33, 0xed, 0x8b, 0x12, 0x69, 0x84, 0x49, 0xd4, 0xbb, 0xca,
        0x3c, 0x85, 0x3c, 0x66, 0xb2, 0x93, 0xea, 0x1b, 0x1c, 0xe9, 0xd9, 0xdc};

    run_roundtrip(entropy, sizeof(entropy),
                  "near account window bike charge season chef number "
                  "sketch tomorrow excuse sniff circle vital hockey "
                  "outdoor supply token");
}

static void test_bip39_roundtrip_24_words(void** state) {
    (void)state;
    static const uint8_t entropy[32] = {
        0xae, 0x13, 0x1e, 0x23, 0x12, 0xcd, 0xc6, 0x13, 0x31, 0x54, 0x2e,
        0xfe, 0x0d, 0x10, 0x77, 0xba, 0xc5, 0xea, 0x80, 0x3a, 0xdf, 0x24,
        0xb3, 0x13, 0xa4, 0xf0, 0xe4, 0x8e, 0x9c, 0x51, 0xf3, 0x7f};

    run_roundtrip(entropy, sizeof(entropy),
                  "puppy ocean match cereal symbol another shed magic "
                  "wrap hammer bulb intact gadget divorce twin tonight "
                  "reason outdoor destroy simple truth cigar social "
                  "volcano");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip39),
        cmocka_unit_test(test_bip39_seed_short_mnemonic),
        cmocka_unit_test(test_bip39_seed_length_boundary),
        cmocka_unit_test(test_bip39_decode_unknown_word),
        cmocka_unit_test(test_bip39_decode_bad_checksum),
        cmocka_unit_test(test_bip39_decode_wrong_length),
        cmocka_unit_test(test_bip39_encode_insufficient_buffer),
        cmocka_unit_test(test_bip39_encode_rejects_invalid_seed_len),
        cmocka_unit_test(test_bip39_roundtrip_12_words),
        cmocka_unit_test(test_bip39_roundtrip_18_words),
        cmocka_unit_test(test_bip39_roundtrip_24_words),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
