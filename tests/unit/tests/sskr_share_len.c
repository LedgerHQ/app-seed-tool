/*
 * The shard-length bound in bolos_ux_sskr_combine(), on both sides of both of
 * its edges, built with AddressSanitizer.
 *
 * The function reads the length of each serialized shard straight out of the
 * CBOR header of the entered data:
 *
 *     uint8_t sskr_share_len = sskr_shares_hex[3] & 0x1F;
 *     if (sskr_share_len > 23) sskr_share_len = sskr_shares_hex[4];   // 0..255
 *
 * A serialized shard is SSKR_METADATA_LENGTH_BYTES plus a 16..32 byte value,
 * so anything outside 21..37 is not a shard. What makes the bound load-bearing
 * rather than tidy is what sits downstream: sskr_deserialize_shard() does
 *
 *     shard->value_len = source_len - SSKR_METADATA_LENGTH_BYTES;
 *     memcpy(shard->value, source + 5, shard->value_len);
 *
 * and only then validates the length -- the copy has already happened, reading
 * source_len bytes out of a share buffer that is nowhere near that long and
 * writing them into a fixed 32-byte field. Reordering that is a change to
 * sskr.c, which is kept diffable against upstream bc-sskr; the bound in this
 * wrapper is what keeps the reordering from being urgent, because here the
 * length comes from data typed on the device rather than from a library user.
 *
 * That makes this test a memory-safety test, so it asserts on the sanitizer as
 * much as on the return value. The return value alone cannot hold this bound:
 * an out-of-range length also fails validation further down, so combine()
 * answers 0 either way and the assertion passes on unbounded code. Deleting
 * the bound and running the suite is how that was established, and it is why
 * the over-long cases below hand combine() a buffer sized to the share it
 * actually contains -- which is what the entry screens do -- instead of one
 * padded out to the declared length.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
// clang-format on

/* 2-of-3 over a 128-bit secret: 21-byte shards, so the CBOR header is the
 * short form (tag + 0x40|21) and sskr_share_len sits at the low five bits of
 * byte 3. Same share set as sskr_interop_bc128_bytewords.c, generated with
 * Blockchain Commons' bc-sskr. */
#define SHORT_HEADER (4)
#define SHORT_SHARD_LEN (SSKR_MIN_SERIALIZED_LENGTH_BYTES) /* 21 */
#define SHORT_WIRE (SHORT_HEADER + SHORT_SHARD_LEN + 4)

static const uint8_t k_min_len_shares[2 * SHORT_WIRE] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72, 0x79, 0x90,
    0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33, 0xFD, 0xAE, 0x81, 0x59,
    0x13, 0x5E, 0xF3, 0xB3, 0x38, 0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00,
    0x01, 0x01, 0x1B, 0x3F, 0x98, 0x69, 0x92, 0x4E, 0x46, 0x03, 0x14, 0x4D,
    0x4A, 0x40, 0x84, 0xF0, 0x90, 0xCC, 0xAE, 0x60, 0x76, 0x65};

static const uint8_t k_min_len_secret[16] = {0x59, 0xF2, 0x29, 0x3A, 0x5B, 0xCE,
                                             0x7D, 0x4D, 0xE5, 0x9E, 0x71, 0xB4,
                                             0x20, 0x7A, 0xC5, 0xD2};

/* 2-of-3 over a 256-bit secret: 37-byte shards, the largest a shard can be, so
 * the CBOR header is the long form (tag + 0x58 + one length byte) and
 * sskr_share_len is read from byte 4. Same share set as tests/roundtrip.c. */
#define LONG_HEADER (5)
#define LONG_SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES)
#define LONG_WIRE (LONG_HEADER + LONG_SHARD_LEN)

static const uint8_t k_max_len_shares[2 * LONG_WIRE] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x01, 0x00, 0x00, 0x01, 0x00, 0xFF, 0x0F,
    0xBB, 0x2A, 0x8B, 0xCC, 0x6F, 0x00, 0xC8, 0xE6, 0x83, 0xDF, 0xB0, 0xEB,
    0x5F, 0x1C, 0x55, 0xEF, 0x12, 0xD9, 0x4B, 0x62, 0x97, 0x42, 0x42, 0xDA,
    0x21, 0x11, 0xBA, 0xC6, 0x5F, 0xAA, 0xD9, 0x9D, 0x75, 0x58, 0x25, 0x01,
    0x00, 0x00, 0x01, 0x01, 0xB4, 0x8E, 0x81, 0x9F, 0xBB, 0x8A, 0x1C, 0xC3,
    0xCF, 0xFB, 0x10, 0xBB, 0xC7, 0xB7, 0x96, 0xBC, 0xBD, 0xB3, 0x4F, 0x1E,
    0x21, 0xCE, 0x04, 0x94, 0x48, 0x1E, 0x79, 0x8C, 0x0D, 0x7E, 0xEA, 0xA2};

static const uint8_t k_max_len_secret[32] = {
    0xE3, 0x95, 0x5C, 0xDA, 0x30, 0x47, 0x71, 0xC0, 0x03, 0x18, 0x95,
    0x63, 0x7F, 0x55, 0xC3, 0xAB, 0xE4, 0x51, 0x53, 0xC8, 0x7A, 0xBD,
    0x81, 0xC5, 0x1E, 0xD1, 0x4E, 0x8A, 0xAF, 0xA1, 0xAF, 0x13};

/* Far enough past the 37-byte maximum that the read runs well off the end of
 * the buffer rather than a byte or two past it. */
#define OVERLONG_SHARD_LEN (200)

static void test_combine_accepts_minimum_shard_length(void** state) {
    (void)state;

    uint8_t wire[sizeof(k_min_len_shares)];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_min_len_shares, sizeof(wire));

    /* 21 is SSKR_MIN_SERIALIZED_LENGTH_BYTES exactly: the bound must let it
     * through, not just refuse what is outside. */
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output),
                     sizeof(k_min_len_secret));
    assert_memory_equal(output, k_min_len_secret, sizeof(k_min_len_secret));
}

static void test_combine_accepts_maximum_shard_length(void** state) {
    (void)state;

    uint8_t wire[sizeof(k_max_len_shares)];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_max_len_shares, sizeof(wire));

    /* 37 is SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES exactly. */
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output),
                     sizeof(k_max_len_secret));
    assert_memory_equal(output, k_max_len_secret, sizeof(k_max_len_secret));
}

static void test_combine_rejects_shard_length_below_minimum(void** state) {
    (void)state;

    uint8_t wire[sizeof(k_min_len_shares)];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_min_len_shares, sizeof(wire));

    /* One below the minimum, in the short-form length nibble of byte 3: a
     * 20-byte shard leaves a 15-byte value, under the 16-byte minimum. */
    wire[3] = 0x40 | (SHORT_SHARD_LEN - 1);

    /* This edge documents the bound rather than holding it: too short is also
     * refused by sskr_deserialize_shard()'s own first check, so this case
     * still passes with the bound deleted. The two above the maximum are the
     * ones that hold it, and they are the dangerous side. */
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 1, output), 0);
}

static void test_combine_rejects_shard_length_above_maximum(void** state) {
    (void)state;

    /* A single share, in a buffer holding exactly the 42 bytes it occupies on
     * the wire -- the entry screens size their buffer for a real share, not
     * for whatever length the header claims. */
    uint8_t wire[LONG_WIRE];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_max_len_shares, sizeof(wire));

    /* One above the maximum, in the long-form length byte. Without the bound
     * the copy reads 33 bytes from 10 bytes in, one past the end of wire[],
     * and writes them into a 32-byte field. */
    wire[4] = LONG_SHARD_LEN + 1;

    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 1, output), 0);
}

static void test_combine_rejects_overlong_shard_length(void** state) {
    (void)state;

    /* Same single-share buffer as above, this time with a length that is not
     * merely one too many. */
    uint8_t wire[LONG_WIRE];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_max_len_shares, sizeof(wire));
    wire[4] = OVERLONG_SHARD_LEN;

    /* Must be refused before any copy. Without the bound,
     * sskr_deserialize_shard() reads 195 bytes starting 10 bytes into a
     * 42-byte buffer and writes them into a 32-byte field. */
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 1, output), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_accepts_minimum_shard_length),
        cmocka_unit_test(test_combine_accepts_maximum_shard_length),
        cmocka_unit_test(test_combine_rejects_shard_length_below_minimum),
        cmocka_unit_test(test_combine_rejects_shard_length_above_maximum),
        cmocka_unit_test(test_combine_rejects_overlong_shard_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
