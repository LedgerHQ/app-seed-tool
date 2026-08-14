/*
 * The exact CBOR byte-string header this port emits, for the three share
 * lengths it can produce, and nothing about it read back with the encoder's
 * own convention.
 *
 * seed_sskr.c picks the form of that header from one threshold:
 *
 *     if (share_len < 24) { cbor[3] |= (share_len & 0x1F); cbor_len--; }
 *     else                { cbor[3] |= 0x18; cbor[4] = share_len; }
 *
 * RFC 8949 section 3: for a byte string, additional-info values 0-23 carry
 * the length in the initial byte, 24 announces one length byte, and 28-30
 * are reserved -- data using them is *not well-formed*. Moving that
 * threshold to 32 leaves the entire suite green while a 192-bit seed (18
 * BIP-39 words, share_len 29) makes the application emit 0x5D on its own:
 * major type 2, additional info 29, malformed CBOR, instead of 0x58 0x1D.
 * 192 bits is the only seed size whose share length falls in 24-31, so it is
 * the only one where the mistake shows.
 *
 * Nothing in the repository noticed, and the reason is worth stating because
 * it dictates how this file is written:
 *
 *   - bolos_ux_sskr_hex_check() checks 3 tag bytes and the CRC-32; it never
 *     parses the header.
 *   - bolos_ux_sskr_combine() reads the length back as
 *     sskr_shares_hex[3] & 0x1F -- its own encoder's convention. The
 *     internal round trip stays consistent while the format leaves the
 *     specification.
 *   - sskr_wire_properties.c does generate 18-word sets, but its property is
 *     "the encoder is accepted by this repository's own verifier".
 *
 * So a test that read the header back with `& 0x1F` would be worth nothing
 * here. Two springs, both needed:
 *
 *   1. the emitted bytes are frozen as constants derived from RFC 8949, not
 *      from this code: 21-byte shard -> 0x55, 29 -> 0x58 0x1D, 37 ->
 *      0x58 0x25, with the whole wire length that follows from each;
 *   2. a minimal header decoder written from RFC 8949 section 3, local to
 *      this file, which insists on major type 2 and on the shortest form,
 *      and refuses additional-info 28-30 as malformed. No `& 0x1F`.
 *
 * External anchor: BCR-2020-011 publishes the complete wire form of a
 * 21-byte shard,
 *     d99d75 55 4bbf1101025abd490ee65b6084859854ee67736e75
 * so the 0x55 expected below is the specification's own byte, not this
 * port's.
 *     https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "bip39/common_bip39.h"
#include "sskr/common_sskr.h"
// clang-format on

#define CBOR_TAG_LEN (3) /* D9 9D 75, tag #6.40309 */
#define CRC_LEN (4)
#define MAX_SHARES (3)
#define MAX_WIRE_BYTES SSKR_SHARE_MAX_WIRE_LENGTH
#define MAX_WORDS_PER_SHARE (MAX_WIRE_BYTES * (SSKR_BYTEWORD_LENGTH + 1))
#define MAX_MNEMONIC_LEN (24 * 9)

typedef struct {
    unsigned int words; /* BIP-39 word count */
    uint8_t seed_len;   /* bytes of entropy */
    uint8_t shard_len;  /* SSKR_METADATA_LENGTH_BYTES + seed_len */
    uint8_t header[2];  /* the byte-string header, from RFC 8949 */
    uint8_t header_len; /* 1 or 2 */
} expectation_t;

/*
 * The three seed sizes this application offers, with the byte-string header
 * RFC 8949 section 3 prescribes for each shard length:
 *
 *   21 < 24  -> additional info carries the length: 0x40 | 21 = 0x55
 *   29 >= 24 -> additional info 24 (0x58), then the length byte 0x1D
 *   37 >= 24 -> 0x58, then 0x25
 *
 * Written out rather than computed so that this table cannot drift with the
 * code it pins.
 */
static const expectation_t k_expectations[] = {
    {12, 16, 21, {0x55, 0x00}, 1},
    {18, 24, 29, {0x58, 0x1D}, 2},
    {24, 32, 37, {0x58, 0x25}, 2},
};

/* Fixed entropy per size; the header does not depend on it, and a constant
 * keeps a failure reproducible. */
static const uint8_t k_entropy[32] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA,
    0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A,
    0x69, 0x78, 0x87, 0x96, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0};

/*
 * A byte-string header decoder written from RFC 8949 section 3, on purpose
 * knowing nothing of how seed_sskr.c builds one. Returns the declared length
 * and writes how many bytes the header occupies. Anything that is not a
 * well-formed, shortest-form definite-length byte string fails the test on
 * the spot.
 */
static size_t rfc8949_byte_string_length(const uint8_t* header,
                                         size_t available, size_t* header_len) {
    assert_true(available >= 1);

    const uint8_t major_type = (uint8_t)(header[0] >> 5);
    const uint8_t additional_info = (uint8_t)(header[0] & 0x1F);

    if (major_type != 2) {
        fail_msg("initial byte 0x%02X is major type %u, not 2 (byte string)",
                 header[0], major_type);
    }

    if (additional_info <= 23) {
        *header_len = 1;
        return additional_info;
    }

    if (additional_info == 24) {
        assert_true(available >= 2);
        /* Shortest form: a length below 24 has to be carried by the initial
         * byte, so the long form may not be used for it. */
        if (header[1] < 24) {
            fail_msg(
                "length %u encoded in long form 0x58 %02X, but it fits "
                "in the initial byte",
                header[1], header[1]);
        }
        *header_len = 2;
        return header[1];
    }

    if (additional_info >= 25 && additional_info <= 27) {
        fail_msg(
            "initial byte 0x%02X announces a %u-byte length; no share is "
            "long enough to need one",
            header[0], 1u << (additional_info - 24));
    }

    if (additional_info >= 28 && additional_info <= 30) {
        fail_msg(
            "initial byte 0x%02X uses reserved additional info %u: "
            "RFC 8949 makes this data item not well-formed",
            header[0], additional_info);
    }

    fail_msg(
        "initial byte 0x%02X is an indefinite-length byte string, which "
        "is not a share",
        header[0]);
    return 0; /* not reached: fail_msg() does not return */
}

/*
 * Drives the production encoder for one seed size and hands back the first
 * share as bytes, exactly as the entry screens reconstruct them from typed
 * ByteWords. Returns the length of one share in bytes.
 */
static unsigned int encode_first_share(const expectation_t* expected,
                                       uint8_t* share) {
    unsigned char mnemonic[MAX_MNEMONIC_LEN];
    unsigned char words[MAX_SHARES * MAX_WORDS_PER_SHARE];
    unsigned int group_descriptor[2] = {2, MAX_SHARES};
    unsigned int words_len = 0;
    uint8_t share_count = 0;

    const unsigned int mnemonic_len = bolos_ux_bip39_mnemonic_encode(
        k_entropy, expected->seed_len, mnemonic, sizeof(mnemonic));
    assert_int_not_equal(mnemonic_len, 0);

    bolos_ux_bip39_to_sskr_convert(mnemonic, mnemonic_len, expected->words,
                                   group_descriptor, &share_count, words,
                                   &words_len);

    assert_int_equal(share_count, MAX_SHARES);
    assert_int_not_equal(words_len, 0);

    /* Space-separated ByteWords: one share of n bytes is
     * n * (length + 1) - 1 characters long. */
    const unsigned int chars_per_share = words_len / share_count;
    const unsigned int bytes_per_share =
        (chars_per_share + 1) / (SSKR_BYTEWORD_LENGTH + 1);

    assert_true(bytes_per_share <= MAX_WIRE_BYTES);

    for (unsigned int i = 0; i < bytes_per_share; i++) {
        const unsigned char* byteword = &words[i * (SSKR_BYTEWORD_LENGTH + 1)];
        assert_true(bolos_ux_sskr_byteword_to_hex(byteword, &share[i]));
    }

    return bytes_per_share;
}

/*
 * Spring 1: the bytes on the wire are the ones RFC 8949 prescribes, and the
 * frame is exactly as long as those bytes imply.
 */
static void test_emitted_length_header_is_frozen(void** state) {
    (void)state;

    for (size_t i = 0; i < sizeof(k_expectations) / sizeof(k_expectations[0]);
         i++) {
        const expectation_t* expected = &k_expectations[i];
        uint8_t share[MAX_WIRE_BYTES];

        const unsigned int share_bytes = encode_first_share(expected, share);

        const uint8_t tag[CBOR_TAG_LEN] = {0xD9, 0x9D, 0x75};
        assert_memory_equal(share, tag, sizeof(tag));

        assert_memory_equal(&share[CBOR_TAG_LEN], expected->header,
                            expected->header_len);

        assert_int_equal(share_bytes, CBOR_TAG_LEN + expected->header_len +
                                          expected->shard_len + CRC_LEN);
    }
}

/*
 * Spring 2: read back with RFC 8949 rather than with `& 0x1F`. The declared
 * length has to be the shard length, in a well-formed shortest-form header.
 */
static void test_emitted_header_parses_as_rfc8949(void** state) {
    (void)state;

    for (size_t i = 0; i < sizeof(k_expectations) / sizeof(k_expectations[0]);
         i++) {
        const expectation_t* expected = &k_expectations[i];
        uint8_t share[MAX_WIRE_BYTES];

        const unsigned int share_bytes = encode_first_share(expected, share);

        size_t header_len = 0;
        const size_t declared = rfc8949_byte_string_length(
            &share[CBOR_TAG_LEN], share_bytes - CBOR_TAG_LEN, &header_len);

        assert_int_equal(declared, expected->shard_len);
        assert_int_equal(header_len, expected->header_len);
        assert_int_equal(share_bytes,
                         CBOR_TAG_LEN + header_len + declared + CRC_LEN);
    }
}

/*
 * The same decoder against the complete wire form BCR-2020-011 publishes for
 * a 21-byte shard. Its header byte is the specification's, and it is the one
 * the table above expects for a 128-bit seed.
 */
static void test_published_wire_form_agrees(void** state) {
    (void)state;

    static const uint8_t k_published[] = {
        0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x02,
        0x5A, 0xBD, 0x49, 0x0E, 0xE6, 0x5B, 0x60, 0x84, 0x85,
        0x98, 0x54, 0xEE, 0x67, 0x73, 0x6E, 0x75};

    size_t header_len = 0;
    const size_t declared = rfc8949_byte_string_length(
        &k_published[CBOR_TAG_LEN], sizeof(k_published) - CBOR_TAG_LEN,
        &header_len);

    assert_int_equal(header_len, 1);
    assert_int_equal(declared, 21);
    assert_int_equal(sizeof(k_published), CBOR_TAG_LEN + header_len + declared);

    /* and it is the byte this port is expected to emit for a 128-bit seed */
    assert_int_equal(k_published[CBOR_TAG_LEN], k_expectations[0].header[0]);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_emitted_length_header_is_frozen),
        cmocka_unit_test(test_emitted_header_parses_as_rfc8949),
        cmocka_unit_test(test_published_wire_form_agrees),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
