/*
 * Regression test for the swallowed error codes in bolos_ux_sskr_combine().
 *
 * sskr_combine_shards() reports failures as negative int16_t values. The result
 * used to be stored in a uint16_t, so every one of the 25 error codes became a
 * large positive number and the `if (output_len < 1)` guard below it never
 * fired. bolos_ux_sskr_combine() therefore returned a bogus length instead of 0
 * on failure, and the caller could not tell "these shards cannot be combined"
 * apart from "this seed does not match the device".
 *
 * The scenario used here is the one a user actually hits: the same shard
 * entered twice. Both copies are well-formed and share their leading metadata,
 * so bolos_ux_sskr_hex_check() accepts them; sskr_combine_shards() then fails
 * with SSKR_ERROR_DUPLICATE_MEMBER_INDEX (-10), which a uint16_t turns into
 * 65526.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"

#define SHARD_VALUE_LEN (16)
#define SHARD_LEN       (SSKR_METADATA_LENGTH_BYTES + SHARD_VALUE_LEN)
/* 3-byte CBOR tag + 1-byte header, then the shard, then the CRC32 */
#define CBOR_LEN        (4)
#define CRC_LEN         (4)
#define WIRE_LEN        (CBOR_LEN + SHARD_LEN + CRC_LEN)

/* Builds the wire form bolos_ux_sskr_combine() expects: CBOR header, shard,
 * CRC32. member_index is the only field that differs between shards. */
static void build_wire_shard(uint8_t out[WIRE_LEN], uint8_t member_index, uint8_t member_threshold)
{
    out[0] = 0xD9;
    out[1] = 0x9D;
    out[2] = 0x75;                       // CBOR tag #6.40309
    out[3] = 0x40 | (SHARD_LEN & 0x1F);  // byte string, short form

    out[CBOR_LEN + 0] = 0xAB;            // identifier, shared by the backup
    out[CBOR_LEN + 1] = 0xCD;
    out[CBOR_LEN + 2] = 0x00;            // 1-of-1 group
    out[CBOR_LEN + 3] = (uint8_t) ((member_threshold - 1) & 0x0F);
    out[CBOR_LEN + 4] = member_index;    // reserved nibble stays zero
    memset(&out[CBOR_LEN + SSKR_METADATA_LENGTH_BYTES], 0x42, SHARD_VALUE_LEN);

    /* The CRC is not checked by bolos_ux_sskr_combine() itself, but keeping the
     * frame honest documents that these bytes are a legitimate shard. */
    memset(&out[CBOR_LEN + SHARD_LEN], 0x00, CRC_LEN);
}

static void test_combine_reports_duplicate_member_index(void **state)
{
    (void) state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[32];

    /* the SAME shard twice: identical member_index -> duplicate */
    build_wire_shard(&wire[0], 0, 2);
    build_wire_shard(&wire[WIRE_LEN], 0, 2);

    unsigned int len = bolos_ux_sskr_combine(wire, sizeof(wire), 2, output);

    /* A failed reconstruction must be reported as 0, not as a bogus length. */
    assert_int_equal(len, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_reports_duplicate_member_index),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
