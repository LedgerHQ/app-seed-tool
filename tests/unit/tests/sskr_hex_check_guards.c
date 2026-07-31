/*
 * The guards bolos_ux_sskr_hex_check() holds on shares typed in as ByteWords.
 *
 * A gcov run over the whole suite gives the rejecting branch of this function
 * (seed_sskr.c, the memzero/return 0 arm) an execution count of zero: before
 * this file, no test had ever made it say no.
 *
 * What is asserted here is that a share count of zero is refused. Every check
 * the function performs -- CBOR tag, metadata shared by all shares, CRC-32 --
 * lives inside a loop bounded by that count, so zero used to fall through to
 * the accepting return and report whatever it was handed as valid without
 * reading a byte of it. The count is not a constant of the build: it comes
 * from the member-threshold nibble of the entered data, and the share entry
 * screens leave it at zero for the reserved CBOR additional-info values 25-31,
 * precisely so this function rejects them.
 *
 * The vector is the 128-bit Blockchain Commons share set already used by
 * sskr_interop_bc128_bytewords.c, in the wire form the entry screen builds:
 * CBOR tag D9 9D 75, short-form byte string header 0x55 (21 bytes), the
 * serialized shard, then the CRC-32 in network byte order. Written out as
 * bytes here rather than as ByteWords so that corrupting individual bytes of
 * it is visible in the source.
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

/* Member index 0 of the 2-of-3 set; valid on its own as far as this function
 * is concerned, which checks each share independently plus, from the second
 * one on, that the first 8 bytes match. */
static const uint8_t k_share[WIRE_LEN] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72,
    0x79, 0x90, 0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33,
    0xFD, 0xAE, 0x81, 0x59, 0x13, 0x5E, 0xF3, 0xB3, 0x38};

static void test_hex_check_accepts_reference_share(void** state) {
    (void)state;

    uint8_t wire[WIRE_LEN];
    memcpy(wire, k_share, sizeof(wire));

    /* Control: without this the rejection below would prove nothing, since a
     * function that refuses everything passes it. */
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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hex_check_accepts_reference_share),
        cmocka_unit_test(test_hex_check_rejects_zero_share_count),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
