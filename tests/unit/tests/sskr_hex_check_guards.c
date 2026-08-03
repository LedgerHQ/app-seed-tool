/*
 * The two guards bolos_ux_sskr_hex_check() is supposed to hold on shares typed
 * in as ByteWords: refusing by default, and the CRC-32.
 *
 * A gcov run over the whole suite gives the rejecting branch of this function
 * (seed_sskr.c, the memzero/return 0 arm) an execution count of zero: before
 * this file, no test had ever made it say no, and deleting the CRC-32
 * comparison outright left the whole suite green. That matters more here than
 * it would elsewhere, because the CRC-32 is the only integrity check standing
 * between a mistyped ByteWord that happens to decode and the Shamir
 * recombination underneath.
 *
 * Four properties are asserted:
 *
 *   - a share count of zero is refused. Every check the function performs --
 *     CBOR tag, metadata shared by all shares, CRC-32 -- lives inside a loop
 *     bounded by that count, so zero used to fall through to the accepting
 *     return and report whatever it was handed as valid without reading a byte
 *     of it. The count is not a constant of the build: it comes from the
 *     member-threshold nibble of the entered data, and the share entry screens
 *     leave it at zero for the reserved CBOR additional-info values 25-31,
 *     precisely so this function rejects them.
 *
 *   - the CRC-32 is both compared and recomputed. One test corrupts a CRC byte
 *     (the comparison catches it), one corrupts a payload byte and leaves the
 *     stored CRC alone (only recomputing catches it). A test that did just the
 *     first would still pass against a function that compared the stored CRC
 *     with itself.
 *
 *   - the distance between two shares is bounded below, twice, for two
 *     different reasons. Both bounds are on
 *     sskr_shares_hex_length / sskr_shares_count, and both operands of that
 *     division come from the caller:
 *
 *       * at or below the four bytes of the CRC-32 there is nothing to
 *         checksum, and one step further down the unsigned subtraction that
 *         computes the checksummed length wraps and hands cx_crc32() a length
 *         of 0xFFFFFFFF. Reading four gigabytes from the share buffer is a
 *         page fault, i.e. a reset of the secure element.
 *       * below the metadata every share of one group has to agree on --
 *         eight bytes for the short CBOR header form, nine for the long one --
 *         that comparison reads out of the share it starts from, and off the
 *         end of the buffer entirely on the last one.
 *
 *     Neither is reachable from either entry path: both compute the buffer
 *     length as the share count times a per-share size that is itself at
 *     least eight (see the bound in seed_sskr.c for the arithmetic). The two
 *     tests that hold them therefore call the function directly, and the two
 *     that surround them -- the smallest stride still accepted, and the
 *     reference share -- are what keeps the bounds from being satisfied by a
 *     function that refuses everything.
 *
 * The vector is the 128-bit Blockchain Commons share set already used by
 * sskr_interop_bc128_bytewords.c, in the wire form the entry screen builds:
 * CBOR tag D9 9D 75, short-form byte string header 0x55 (21 bytes), the
 * serialized shard, then the CRC-32 in network byte order. Written out as
 * bytes here rather than as ByteWords so that corrupting one byte of the CRC
 * and one byte of the payload is visible in the source.
 *
 * Built with AddressSanitizer: the offsets this function reads from are
 * computed by dividing a caller-supplied buffer length by a caller-supplied
 * share count, and both come from entered data on the device.
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
// clang-format on

#define WIRE_LEN (29) /* tag(3) + length byte(1) + shard(21) + CRC(4) */
#define CRC_LEN (4)
#define CRC_FIRST (WIRE_LEN - CRC_LEN)

/* Member index 0 of the 2-of-3 set; valid on its own as far as this function
 * is concerned, which checks each share independently plus, from the second
 * one on, that the first 8 bytes match. */
static const uint8_t k_share[WIRE_LEN] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72,
    0x79, 0x90, 0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33,
    0xFD, 0xAE, 0x81, 0x59, 0x13, 0x5E, 0xF3, 0xB3, 0x38};

/* An offset inside the serialized shard, past the CBOR header and clear of the
 * CRC, so corrupting it leaves every check but the CRC-32 satisfied. */
#define PAYLOAD_OFFSET (12)

/* CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320, initial and final
 * value 0xFFFFFFFF), written out rather than calling the cx_crc32() the code
 * under test uses -- sharing that implementation would make the frames below
 * agree with hex_check() by construction. It agrees with k_share, whose CRC
 * comes from bc-sskr; test_crc_helper_reproduces_the_reference_crc() pins
 * that. */
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

/* Writes the CRC-32 of the first `covered` bytes of `frame` into the four
 * bytes that follow them, in network byte order, which is the order
 * bolos_ux_sskr_hex_check() compares raw bytes against. Byte by byte rather
 * than swapping a uint32_t, so it does not depend on the host's endianness. */
static void seal(uint8_t* frame, unsigned int covered) {
    const uint32_t crc = crc32_ieee(frame, covered);

    frame[covered + 0] = (uint8_t)(crc >> 24);
    frame[covered + 1] = (uint8_t)(crc >> 16);
    frame[covered + 2] = (uint8_t)(crc >> 8);
    frame[covered + 3] = (uint8_t)crc;
}

static void test_hex_check_accepts_reference_share(void** state) {
    (void)state;

    uint8_t wire[WIRE_LEN];
    memcpy(wire, k_share, sizeof(wire));

    /* Control: without this the three rejections below would prove nothing,
     * since a function that refuses everything passes all of them. */
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 1), 1);
}

static void test_hex_check_rejects_zero_share_count(void** state) {
    (void)state;

    /* Not a share at all: no CBOR tag, no CRC, nothing that could pass a
     * single one of this function's checks. */
    uint8_t wire[WIRE_LEN];
    memset(wire, 0xFF, sizeof(wire));

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 0), 0);

    /* And erased on the way out, like every other failure path here. */
    for (unsigned int i = 0; i < sizeof(wire); i++) {
        assert_int_equal(wire[i], 0x00);
    }
}

static void test_hex_check_rejects_corrupted_crc(void** state) {
    (void)state;

    uint8_t wire[WIRE_LEN];
    memcpy(wire, k_share, sizeof(wire));
    wire[CRC_FIRST] ^= 0x01;

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 1), 0);
}

static void test_hex_check_rejects_corrupted_payload(void** state) {
    (void)state;

    uint8_t wire[WIRE_LEN];
    memcpy(wire, k_share, sizeof(wire));
    wire[PAYLOAD_OFFSET] ^= 0x01;

    /* The stored CRC is untouched: this only fails if the CRC is recomputed
     * over the payload rather than taken on trust. */
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 1), 0);
}

/* The local CRC-32 has to agree with the one bc-sskr produced for k_share,
 * otherwise every frame sealed below would be rejected for the wrong reason
 * and the four stride tests would prove nothing. */
static void test_crc_helper_reproduces_the_reference_crc(void** state) {
    (void)state;

    uint8_t wire[WIRE_LEN];
    memcpy(wire, k_share, sizeof(wire));
    memset(&wire[CRC_FIRST], 0x00, CRC_LEN);

    seal(wire, CRC_FIRST);

    assert_memory_equal(&wire[CRC_FIRST], &k_share[CRC_FIRST], CRC_LEN);
}

/*
 * Stride 3, three bytes per share. The length passed to cx_crc32() is
 * stride - 4 on unsigned int, so this is 0xFFFFFFFF: a four-gigabyte read
 * from a 48-byte buffer. Under AddressSanitizer it is a SEGV inside
 * cx_crc32_update(), not a return value, which is why this file is built with
 * the sanitizer -- a test written against the return value would report
 * nothing here.
 *
 * The buffer is not a share and does not have to be: the subtraction happens
 * before the first byte of it is looked at.
 */
static void test_hex_check_rejects_a_stride_under_the_checksum(void** state) {
    (void)state;

    uint8_t wire[48];
    memset(wire, 0xFF, sizeof(wire));

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 16), 0);
}

/*
 * Stride 4, exactly the width of the CRC-32. The bound is <= and not <
 * because here the subtraction is well defined and gives zero: cx_crc32()
 * would be asked to checksum nothing at all and the result compared against
 * four bytes of the share, which at this stride are the CBOR tag itself.
 *
 * Held only in part by this test, and worth saying so: the frame is refused
 * before the fix too, by that same CRC comparison, since no four bytes can be
 * both the tag and the checksum of the bytes preceding them. What the bound
 * adds is that the refusal happens before cx_crc32() is handed a zero length,
 * not after.
 */
static void test_hex_check_rejects_a_stride_at_the_checksum(void** state) {
    (void)state;

    uint8_t wire[4] = {0xD9, 0x9D, 0x75, 0x55};

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 1), 0);
}

/*
 * Stride 7, one below the eight bytes of metadata two shares of one group
 * have to agree on. Both frames are well-formed as far as every other check
 * goes -- CBOR tag, and a CRC-32 over the three bytes that precede the
 * checksum field -- so the loop reaches its second turn, which is the only
 * place the metadata comparison runs. There it compares eight bytes starting
 * at offset 7 of a buffer the caller declared as 14 bytes long, and the
 * eighth of them is outside that declaration.
 *
 * The buffer is one byte longer than the length passed in, and that byte is
 * set to the value that makes the comparison agree. Without it the verdict
 * would depend on whatever follows the share buffer -- which is the defect,
 * but is not something to assert on. With it the difference is a verdict and
 * not a sanitizer report: before the bound, a pair of seven-byte frames is
 * accepted on the strength of a byte the caller never offered.
 *
 * That the frames pass everything else is what makes this a bound and not an
 * accident: without the CRC being right, the function would refuse at the
 * first turn of the loop and never reach the comparison.
 */
static void test_hex_check_rejects_a_stride_under_the_metadata(void** state) {
    (void)state;

    uint8_t wire[15];
    const unsigned int declared = 14;

    for (unsigned int i = 0; i < 2; i++) {
        uint8_t* frame = &wire[i * 7];
        frame[0] = 0xD9;
        frame[1] = 0x9D;
        frame[2] = 0x75;
        seal(frame, 3);
    }
    wire[declared] = wire[7]; /* the byte the comparison would reach past */

    assert_int_equal(bolos_ux_sskr_hex_check(wire, declared, 2), 0);
}

/*
 * Stride 8, the smallest this function can still check: four header bytes in
 * the short CBOR form, then the CRC-32 over them. No shard fits in it, but
 * that is not what this function decides -- bolos_ux_sskr_combine() holds the
 * shard length, and sskr_share_len.c enumerates it. The point here is the
 * other side of the bound above: it must refuse a stride it cannot read
 * safely and accept the first one it can, or it would be satisfied by a
 * function that refuses everything.
 */
static void test_hex_check_accepts_the_smallest_checkable_stride(void** state) {
    (void)state;

    uint8_t wire[16];

    for (unsigned int i = 0; i < 2; i++) {
        uint8_t* frame = &wire[i * 8];
        frame[0] = 0xD9;
        frame[1] = 0x9D;
        frame[2] = 0x75;
        frame[3] = 0x55;
        seal(frame, 4);
    }

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 2), 1);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hex_check_accepts_reference_share),
        cmocka_unit_test(test_hex_check_rejects_zero_share_count),
        cmocka_unit_test(test_hex_check_rejects_corrupted_crc),
        cmocka_unit_test(test_hex_check_rejects_corrupted_payload),
        cmocka_unit_test(test_crc_helper_reproduces_the_reference_crc),
        cmocka_unit_test(test_hex_check_rejects_a_stride_under_the_checksum),
        cmocka_unit_test(test_hex_check_rejects_a_stride_at_the_checksum),
        cmocka_unit_test(test_hex_check_rejects_a_stride_under_the_metadata),
        cmocka_unit_test(test_hex_check_accepts_the_smallest_checkable_stride),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
