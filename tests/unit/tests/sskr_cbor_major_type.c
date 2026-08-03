/*
 * The three type bits of the CBOR byte-string header, swept over all eight
 * values they can take, at every place this port reads that byte.
 *
 * Byte 3 of a share on the wire is a CBOR initial byte: three bits of major
 * type, five of additional information (RFC 8949 section 3). BCR-2020-011
 * wraps a shard as a byte string, so the only conformant major type is 2, and
 * the published wire form of a 21-byte shard begins
 *
 *     d9 9d 75 55 4b bf ...
 *              ^^ 010 10101 -- major type 2, additional info 21
 *
 * Every place this port reads that byte takes the additional information out
 * of it with `& 0x1F` -- twice in seed_sskr.c, four times in
 * sskr_entry_header.c. The type bits used to be discarded by all six, so
 * 0x15, 0x35, 0x75, 0x95, 0xB5, 0xD5 and 0xF5 were read exactly as 0x55 is.
 * Six of those are a different CBOR type altogether -- unsigned integer,
 * negative integer, text string, array, map, tag -- and the seventh is a
 * simple value or float. None of them is a byte string, and none of them can
 * begin a share. All eight were accepted end to end: the entry path computed
 * the same expected word count and share count for each, hex_check() passed
 * each, and combine() returned the published secret from each.
 *
 * SSKR_CBOR_IS_BYTE_STRING() is the other half of that byte, applied at all
 * six sites, and the `accepted` column below now follows `conformant`.
 *
 * This file measures that rather than assuming it, over both halves of the
 * code that reads the byte:
 *
 *   - the entry path, bolos_ux_sskr_entry_header_update(), which turns the
 *     header into an expected word count and a share count while the user is
 *     still typing;
 *   - the input check, bolos_ux_sskr_hex_check() and bolos_ux_sskr_combine(),
 *     which are what stands between typed ByteWords and the Shamir
 *     recombination.
 *
 * The k_cases table below carries two separate columns on purpose.
 * `conformant` is what RFC 8949 and BCR-2020-011 say, and it does not depend
 * on this repository. `accepted` is what this build does. They agree now; they
 * are kept apart because they are two different facts, and because the sweep
 * is worth having either way -- remove the six checks and this file reports
 * exactly which readers stopped holding the rule.
 *
 * Vector: the 2-of-3 128-bit share pair generated with Blockchain Commons'
 * bc-sskr already used by sskr_duplicate_member_index.c, sskr_share_len.c and
 * sskr_interop_bc128_bytewords.c -- member_index 0 and 1, 21-byte shards,
 * genuine CRC-32s. Only byte 3 is moved, and each frame is resealed
 * afterwards, so the CRC-32 never becomes the reason a frame is refused: what
 * the sweep reports is the header byte and nothing else.
 *
 *     https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md
 *     https://www.rfc-editor.org/rfc/rfc8949#name-major-types
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
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
// clang-format on

#define HEADER_LEN (4)
#define SHARD_LEN (21)
#define CRC_LEN (4)
#define WIRE_LEN (HEADER_LEN + SHARD_LEN + CRC_LEN) /* 29 */
#define SHARE_COUNT (2)

/* additional info 21: the shard length the published vector carries, and the
 * only part of byte 3 this port looks at. Held constant across the sweep so
 * the major type is the single variable. */
#define ADDITIONAL_INFO (21)
#define HEADER_BYTE(major_type) ((uint8_t)(((major_type) << 5) | ADDITIONAL_INFO))

/* Major type 2 is the byte string; RFC 8949 section 3.1. */
#define CBOR_MAJOR_TYPE_BYTE_STRING (2)

static const uint8_t k_shares[SHARE_COUNT * WIRE_LEN] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72, 0x79, 0x90,
    0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33, 0xFD, 0xAE, 0x81, 0x59,
    0x13, 0x5E, 0xF3, 0xB3, 0x38, 0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00,
    0x01, 0x01, 0x1B, 0x3F, 0x98, 0x69, 0x92, 0x4E, 0x46, 0x03, 0x14, 0x4D,
    0x4A, 0x40, 0x84, 0xF0, 0x90, 0xCC, 0xAE, 0x60, 0x76, 0x65};

static const uint8_t k_secret[16] = {0x59, 0xF2, 0x29, 0x3A, 0x5B, 0xCE,
                                     0x7D, 0x4D, 0xE5, 0x9E, 0x71, 0xB4,
                                     0x20, 0x7A, 0xC5, 0xD2};

typedef struct {
    uint8_t major_type;
    /* What RFC 8949 and BCR-2020-011 say. Independent of this repository. */
    bool conformant;
    /* What this build does with it. The measurement. */
    bool accepted;
} major_type_case_t;

static const major_type_case_t k_cases[] = {
    {0, false, false}, /* 0x15  unsigned integer */
    {1, false, false}, /* 0x35  negative integer */
    {2, true, true},   /* 0x55  byte string -- the only conformant one */
    {3, false, false}, /* 0x75  text string */
    {4, false, false}, /* 0x95  array */
    {5, false, false}, /* 0xB5  map */
    {6, false, false}, /* 0xD5  tag */
    {7, false, false}, /* 0xF5  simple value / float */
};

#define CASE_COUNT (sizeof(k_cases) / sizeof(k_cases[0]))

/* CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320, initial and final
 * value 0xFFFFFFFF), written out rather than calling the cx_crc32() the code
 * under test uses -- sharing that implementation would make the resealed
 * frames agree with hex_check() by construction.
 * test_resealing_reproduces_the_published_crc() pins it against the CRC-32s
 * bc-sskr published for the pair above. */
static uint32_t crc32_ieee(const uint8_t* buf, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* Rewrites one frame's trailing CRC-32 in network byte order, which is the
 * order bolos_ux_sskr_hex_check() memcmps raw bytes against. Byte by byte
 * rather than swapping a uint32_t, so it does not depend on the host's
 * endianness. */
static void reseal(uint8_t* wire) {
    const uint32_t crc = crc32_ieee(wire, WIRE_LEN - CRC_LEN);
    uint8_t* out = wire + WIRE_LEN - CRC_LEN;

    out[0] = (uint8_t)(crc >> 24);
    out[1] = (uint8_t)(crc >> 16);
    out[2] = (uint8_t)(crc >> 8);
    out[3] = (uint8_t)crc;
}

/* Loads the published pair with byte 3 of both frames set to `header`, then
 * makes both frames well-formed again. Reloaded before every call because
 * bolos_ux_sskr_hex_check() and bolos_ux_sskr_combine() memzero the buffer
 * they were given on any failure path. */
static void load(uint8_t wire[SHARE_COUNT * WIRE_LEN], uint8_t header) {
    memcpy(wire, k_shares, SHARE_COUNT * WIRE_LEN);

    for (unsigned int i = 0; i < SHARE_COUNT; i++) {
        wire[i * WIRE_LEN + 3] = header;
        reseal(&wire[i * WIRE_LEN]);
    }
}

/* The helper the whole sweep rests on: resealing a frame nobody touched has
 * to reproduce the CRC-32 bytes bc-sskr published. Without this, a frame
 * refused below could be refused for a bad checksum rather than for its
 * header byte, and the sweep would report nothing. */
static void test_resealing_reproduces_the_published_crc(void** state) {
    (void)state;

    uint8_t wire[SHARE_COUNT * WIRE_LEN];

    memcpy(wire, k_shares, sizeof(wire));
    reseal(&wire[0]);
    reseal(&wire[WIRE_LEN]);

    assert_memory_equal(wire, k_shares, sizeof(wire));
}

/* And the published header byte really is major type 2 with additional info
 * 21, so the sweep's own construction agrees with the vector it starts from
 * rather than merely resembling it. */
static void test_published_header_byte_is_a_byte_string(void** state) {
    (void)state;

    assert_int_equal(k_shares[3], 0x55);
    assert_int_equal(k_shares[3] >> 5, CBOR_MAJOR_TYPE_BYTE_STRING);
    assert_int_equal(k_shares[3] & 0x1F, ADDITIONAL_INFO);
    assert_int_equal(HEADER_BYTE(CBOR_MAJOR_TYPE_BYTE_STRING), 0x55);
}

/*
 * The input check, swept. Each case is a complete, internally consistent pair
 * of frames that differs from the published one in exactly three type bits per
 * frame.
 */
static void test_hex_check_over_every_major_type(void** state) {
    (void)state;

    for (size_t i = 0; i < CASE_COUNT; i++) {
        const major_type_case_t* c = &k_cases[i];
        const uint8_t header = HEADER_BYTE(c->major_type);
        uint8_t wire[SHARE_COUNT * WIRE_LEN];

        load(wire, header);

        const unsigned int verdict =
            bolos_ux_sskr_hex_check(wire, sizeof(wire), SHARE_COUNT);

        if (verdict != (c->accepted ? 1u : 0u)) {
            fail_msg(
                "header byte 0x%02X (major type %u, additional info %u): "
                "hex_check returned %u, expected %u",
                header, c->major_type, ADDITIONAL_INFO, verdict,
                c->accepted ? 1u : 0u);
        }
    }
}

/*
 * ...and the same sweep one step further in, because hex_check() is not the
 * only reader: bolos_ux_sskr_combine() takes the shard length from that same
 * byte with its own copy of the mask. Where the pair is accepted, the secret
 * that comes back is the published one -- so what a non-conformant header
 * buys today is not a garbled result but the real secret, recombined from a
 * frame that is not a CBOR byte string.
 */
static void test_combine_over_every_major_type(void** state) {
    (void)state;

    for (size_t i = 0; i < CASE_COUNT; i++) {
        const major_type_case_t* c = &k_cases[i];
        const uint8_t header = HEADER_BYTE(c->major_type);
        uint8_t wire[SHARE_COUNT * WIRE_LEN];
        uint8_t output[sizeof(k_secret)] = {0};

        load(wire, header);

        const unsigned int output_len =
            bolos_ux_sskr_combine(wire, sizeof(wire), SHARE_COUNT, output);

        if (!c->accepted) {
            if (output_len != 0) {
                fail_msg(
                    "header byte 0x%02X (major type %u): combine returned %u "
                    "bytes, expected a refusal",
                    header, c->major_type, output_len);
            }
            continue;
        }

        if (output_len != sizeof(k_secret)) {
            fail_msg(
                "header byte 0x%02X (major type %u): combine returned %u "
                "bytes, expected %u",
                header, c->major_type, output_len,
                (unsigned int)sizeof(k_secret));
        }
        assert_memory_equal(output, k_secret, sizeof(k_secret));
    }
}

/*
 * The other half: the entry path, which reads the same byte three words
 * earlier and decides from it how many ByteWords a share is and how many
 * shares to ask for.
 *
 * At word 3 it sets the expected total size; at word 7 the share count, from
 * the member-threshold nibble. The two positions used here are the short-form
 * ones, which is what additional info 21 selects.
 *
 * The reserved additional-info values already had a defined answer here --
 * SSKR_SHARE_MAX_WIRE_LENGTH, and the share count left untouched, so entry can
 * finish and bolos_ux_sskr_hex_check() gets the chance to refuse. A
 * non-conformant major type now takes that same shape, which is what the
 * `accepted` column selects below: entry is never cut short on the strength of
 * a byte that is not a length, and the refusal still comes from the check that
 * exists to refuse.
 */
static void test_entry_header_over_every_major_type(void** state) {
    (void)state;

    /* Byte 7 of the published frame: group-index/member-threshold. Its low
     * nibble is what the entry path turns into a share count. */
    const uint8_t threshold_nibble = k_shares[7] & 0x0F;
    const uint8_t expected_count = (uint8_t)(threshold_nibble + 1);

    for (size_t i = 0; i < CASE_COUNT; i++) {
        const major_type_case_t* c = &k_cases[i];
        const uint8_t header = HEADER_BYTE(c->major_type);
        uint8_t buffer[WIRE_LEN];

        memcpy(buffer, k_shares, WIRE_LEN);
        buffer[3] = header;

        /* A value no branch of the function can produce, so "left untouched"
         * is distinguishable from "set". */
        size_t final_size = 0xA5A5;
        uint8_t count = 0xA5;

        bolos_ux_sskr_entry_header_update(buffer, 3, 3, &final_size, &count);

        const size_t expected_size =
            c->accepted ? (size_t)WIRE_LEN : (size_t)SSKR_SHARE_MAX_WIRE_LENGTH;

        if (final_size != expected_size) {
            fail_msg(
                "header byte 0x%02X (major type %u): entry size %u, "
                "expected %u",
                header, c->major_type, (unsigned int)final_size,
                (unsigned int)expected_size);
        }

        bolos_ux_sskr_entry_header_update(buffer, 7, 7, &final_size, &count);

        const uint8_t expected_count_here = c->accepted ? expected_count : 0xA5;

        if (count != expected_count_here) {
            fail_msg(
                "header byte 0x%02X (major type %u): entry share count %u, "
                "expected %u",
                header, c->major_type, count, expected_count_here);
        }
    }
}

/*
 * The control the four sweeps above need: the published frame, untouched, is
 * accepted end to end. Without it a build that refused every header byte would
 * satisfy any column of zeroes in k_cases.
 */
static void test_published_pair_is_accepted_untouched(void** state) {
    (void)state;

    uint8_t wire[SHARE_COUNT * WIRE_LEN];
    uint8_t output[sizeof(k_secret)] = {0};

    memcpy(wire, k_shares, sizeof(wire));
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), SHARE_COUNT),
                     1);

    memcpy(wire, k_shares, sizeof(wire));
    assert_int_equal(
        bolos_ux_sskr_combine(wire, sizeof(wire), SHARE_COUNT, output),
        sizeof(k_secret));
    assert_memory_equal(output, k_secret, sizeof(k_secret));
}

/*
 * Stated as an assertion rather than left in the prose above: of the eight
 * values byte 3's type bits can take, exactly one is a CBOR byte string. How
 * many of the eight this build accepts is the number the sweeps measure.
 */
static void test_exactly_one_major_type_is_conformant(void** state) {
    (void)state;

    unsigned int conformant = 0;
    unsigned int accepted = 0;

    for (size_t i = 0; i < CASE_COUNT; i++) {
        conformant += k_cases[i].conformant ? 1u : 0u;
        accepted += k_cases[i].accepted ? 1u : 0u;
    }

    assert_int_equal(CASE_COUNT, 8);
    assert_int_equal(conformant, 1);
    assert_true(k_cases[CBOR_MAJOR_TYPE_BYTE_STRING].conformant);
    /* Whatever else it accepts, the conformant one is never refused. */
    assert_true(k_cases[CBOR_MAJOR_TYPE_BYTE_STRING].accepted);
    assert_true(accepted >= conformant);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_resealing_reproduces_the_published_crc),
        cmocka_unit_test(test_published_header_byte_is_a_byte_string),
        cmocka_unit_test(test_published_pair_is_accepted_untouched),
        cmocka_unit_test(test_hex_check_over_every_major_type),
        cmocka_unit_test(test_combine_over_every_major_type),
        cmocka_unit_test(test_entry_header_over_every_major_type),
        cmocka_unit_test(test_exactly_one_major_type_is_conformant),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
