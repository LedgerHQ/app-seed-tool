/*
 * Two input guards that were documented but not made, and one that was made in
 * the wrong place.
 *
 * 1. bolos_ux_sskr_hex_check() and the CBOR initial byte.
 *
 *    That byte is three bits of major type and five of additional information
 *    (RFC 8949 section 3). The major type was already tested. The additional
 *    information was not: 25 to 31 are reserved -- two, four and eight-byte
 *    lengths, one reserved value and indefinite length -- and none of them
 *    carries a length this function can act on, yet all seven were accepted.
 *    So was a declared length that disagreed with the frame it arrived in.
 *
 *    Nothing broke, because two mechanisms one layer away caught it:
 *    bolos_ux_sskr_entry_header_update() leaves the share count at zero for a
 *    reserved value and the zero-count bound refuses the frame, while
 *    bolos_ux_sskr_combine() bounds the declared length itself. Both are real
 *    and both stay. What they are not is this function -- and this function is
 *    the one the entry paths call to ask whether what was typed is a share.
 *
 *    These cases therefore call it directly, with a count of their own, which
 *    is exactly the shape of caller the side effect above does not protect.
 *
 * 2. bolos_ux_sskr_size_get() and bip39_type.
 *
 *    *share_len came back as bip39_type * 4 / 3 + 5 whatever bip39_type was,
 *    and the caller sizes a buffer from it. Every comparable entry point in
 *    this file bounds its parameters; this one did not.
 */

#include <cmocka.h>
#include <cx.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// testutils.h first: it defines WIDE, which the SSKR wordlist declaration in
// seed_rom_variables.h needs and which only the device build gets from os.h.
/* clang-format off */
#include "testutils.h"
#include "constants.h"
#include "sskr/common_sskr.h"
#include "sskr/seed_sskr_internal.h"
#include "sskr/sskr-constants.h"
/* clang-format on */

// A 21-byte shard: 5 metadata bytes plus a 16-byte value, the short form every
// 128-bit seed produces.
#define SHARD_LEN  21
#define HEADER_LEN 4
#define CRC_LEN    4
#define FRAME_LEN  (HEADER_LEN + SHARD_LEN + CRC_LEN)

// CBOR tag #6.40309, the v2 `sskr` tag.
static void build_frame(unsigned char *buf, uint8_t initial_byte, uint8_t shard_len) {
    buf[0] = 0xD9;
    buf[1] = 0x9D;
    buf[2] = 0x75;
    buf[3] = initial_byte;
    memset(buf + 4, 0x11, shard_len);
    buf[4] = 0x4b;  // identifier high
    buf[5] = 0xbf;  // identifier low
    buf[6] = 0x00;  // group-threshold - 1 = 0, group-count - 1 = 0
    buf[7] = 0x00;  // group-index = 0, member-threshold - 1 = 0
    buf[8] = 0x00;  // reserved = 0, member-index = 0

    // Bytewords standard form: a big-endian CRC32 of everything before it. The
    // frames below are all otherwise well-formed, so the checksum never gets to
    // be the reason one is refused.
    uint32_t crc = cx_crc32(buf, 4u + shard_len);
    buf[4 + shard_len + 0] = (unsigned char) (crc >> 24);
    buf[4 + shard_len + 1] = (unsigned char) (crc >> 16);
    buf[4 + shard_len + 2] = (unsigned char) (crc >> 8);
    buf[4 + shard_len + 3] = (unsigned char) (crc);
}

// The control. Without it every refusal below would also be satisfied by a
// function that refused everything.
static void test_well_formed_frame_is_accepted(void **state) {
    (void) state;
    unsigned char frame[FRAME_LEN];
    build_frame(frame, 0x40 | SHARD_LEN, SHARD_LEN);
    assert_int_equal(bolos_ux_sskr_hex_check(frame, FRAME_LEN, 1), 1);
}

static void test_reserved_additional_info_is_refused(void **state) {
    (void) state;
    for (unsigned info = 25; info <= 31; info++) {
        unsigned char frame[FRAME_LEN];
        build_frame(frame, (uint8_t) (0x40 | info), SHARD_LEN);

        assert_int_equal(bolos_ux_sskr_hex_check(frame, FRAME_LEN, 1), 0);

        // Refused frames are erased, like every other rejection in this
        // function: what was typed is not left sitting in the caller's buffer.
        for (size_t i = 0; i < FRAME_LEN; i++) {
            assert_int_equal(frame[i], 0);
        }
    }
}

// Additional information 24 means "one length byte follows", which is a
// different frame shape, not a reserved value -- it has to keep working.
static void test_long_form_is_still_accepted(void **state) {
    (void) state;
    // 4 tag/header bytes + 1 length byte + 24-byte shard + 4 CRC.
    const uint8_t long_shard = 24;
    unsigned char frame[5 + 24 + 4];

    frame[0] = 0xD9;
    frame[1] = 0x9D;
    frame[2] = 0x75;
    frame[3] = 0x40 | 24;   // additional information 24
    frame[4] = long_shard;  // the declared length
    memset(frame + 5, 0x11, long_shard);
    frame[5] = 0x4b;
    frame[6] = 0xbf;
    frame[7] = 0x00;
    frame[8] = 0x00;
    frame[9] = 0x00;
    uint32_t crc = cx_crc32(frame, 5u + long_shard);
    frame[5 + long_shard + 0] = (unsigned char) (crc >> 24);
    frame[5 + long_shard + 1] = (unsigned char) (crc >> 16);
    frame[5 + long_shard + 2] = (unsigned char) (crc >> 8);
    frame[5 + long_shard + 3] = (unsigned char) (crc);

    assert_int_equal(bolos_ux_sskr_hex_check(frame, sizeof(frame), 1), 1);
}

// A frame whose header declares one length while the frame it arrived in is
// another. Both directions, because the sum is what has to match, not a bound.
static void test_declared_length_must_match_the_frame(void **state) {
    (void) state;
    const uint8_t mismatched[] = {20, 22, 16, 23};

    for (size_t i = 0; i < sizeof(mismatched) / sizeof(mismatched[0]); i++) {
        unsigned char frame[FRAME_LEN];
        // The frame still holds SHARD_LEN bytes and a CRC over them; only the
        // declared length in the header disagrees.
        build_frame(frame, (uint8_t) (0x40 | mismatched[i]), SHARD_LEN);
        assert_int_equal(bolos_ux_sskr_hex_check(frame, FRAME_LEN, 1), 0);
    }
}

// bip39_type reaches bolos_ux_sskr_size_get() as the multiplicand of the shard
// length the caller sizes a buffer from.
static void test_size_get_refuses_unknown_bip39_type(void **state) {
    (void) state;
    // 2-of-3: threshold 1 with a count above 1 is refused as a singleton member,
    // which would mask what these cases are actually about.
    unsigned int descriptor[2] = {2, 3};

    const unsigned int rejected[] = {0, 1, 11, 13, 15, 21, 23, 25, 32, 190, 191, 255};
    for (size_t i = 0; i < sizeof(rejected) / sizeof(rejected[0]); i++) {
        uint8_t share_len = 0xFF;
        int16_t count = bolos_ux_sskr_size_get((uint8_t) rejected[i], 1, descriptor, 1,
                                               &share_len);
        assert_int_equal(count, SSKR_ERROR_INVALID_BIP39_TYPE);
        // The output the caller sizes a buffer from, defined on the failure
        // path as well as the success one.
        assert_int_equal(share_len, 0);
    }
}

static void test_size_get_accepts_the_three_offered_lengths(void **state) {
    (void) state;
    // 2-of-3: threshold 1 with a count above 1 is refused as a singleton member,
    // which would mask what these cases are actually about.
    unsigned int descriptor[2] = {2, 3};
    const struct {
        uint8_t words;
        uint8_t shard_len;
    } accepted[] = {
        {BIP39_MNEMONIC_SIZE_12, 16 + SSKR_METADATA_LENGTH_BYTES},
        {BIP39_MNEMONIC_SIZE_18, 24 + SSKR_METADATA_LENGTH_BYTES},
        {BIP39_MNEMONIC_SIZE_24, 32 + SSKR_METADATA_LENGTH_BYTES},
    };

    for (size_t i = 0; i < sizeof(accepted) / sizeof(accepted[0]); i++) {
        uint8_t share_len = 0;
        int16_t count =
            bolos_ux_sskr_size_get(accepted[i].words, 1, descriptor, 1, &share_len);
        assert_int_equal(count, 3);
        assert_int_equal(share_len, accepted[i].shard_len);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_well_formed_frame_is_accepted),
        cmocka_unit_test(test_reserved_additional_info_is_refused),
        cmocka_unit_test(test_long_form_is_still_accepted),
        cmocka_unit_test(test_declared_length_must_match_the_frame),
        cmocka_unit_test(test_size_get_refuses_unknown_bip39_type),
        cmocka_unit_test(test_size_get_accepts_the_three_offered_lengths),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
