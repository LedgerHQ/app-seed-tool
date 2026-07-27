/*
 * Memory-safety regression test for the shard-count bounds, built with
 * AddressSanitizer.
 *
 * Two arrays are sized from the same pair of macros, and both are filled from
 * a count the caller supplies:
 *
 *   seed_sskr.c  const uint8_t *ptr_sskr_shares[SSKR_MAX_GROUP_COUNT *
 *                                               SSS_MAX_SHARE_COUNT];
 *   sskr.c       sskr_shard_t shards[SSS_MAX_SHARE_COUNT *
 *                                    SSKR_MAX_GROUP_COUNT];
 *
 * bolos_ux_sskr_combine() fills the first before it calls
 * sskr_combine_shards(), so a bound placed only in sskr.c still leaves the
 * wrapper's array exposed. This test enters through the wrapper, which is the
 * path the recovery UI takes.
 *
 * It asserts on the sanitizer rather than on the return value, deliberately.
 * The return value cannot separate the two states for long: once the
 * swallowed-error defect is fixed (output_len is a uint16_t today, so the
 * `output_len < 1` guard never fires), that guard will return 0 whether or not
 * the bound exists, while the overflow will still have happened. A test
 * written against the return value would then pass on unbounded code.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>
#include <cx.h>

#include "testutils.h"
#include "constants.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
#include "sss-constants.h"

#define SHARDS_CAPACITY (SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT)

/* A 32-byte value is what a 24-word backup shards into, so the CBOR header is
 * the long form: tag(3) + 0x58 + one length byte. */
#define SHARD_VALUE_LEN (SSKR_MAX_STRENGTH_BYTES)
#define SHARD_LEN       (SSKR_METADATA_LENGTH_BYTES + SHARD_VALUE_LEN)
#define CBOR_LEN        (5)
#define CRC_LEN         (4)
#define WIRE_LEN        (CBOR_LEN + SHARD_LEN + CRC_LEN)

/*
 * One shard of a single-group backup, in the wire form the UI collects:
 * CBOR header, serialized shard, CRC32. member_threshold is what the Nano S
 * entry path turns into its shard count, so it is set to the number of shards
 * the caller will pass.
 */
static void build_wire_shard(uint8_t *wire, uint8_t member_threshold, uint8_t member_index) {
    memset(wire, 0, WIRE_LEN);
    wire[0] = 0xD9;  // CBOR tag #6.40309
    wire[1] = 0x9D;
    wire[2] = 0x75;
    wire[3] = 0x58;       // byte string, one length byte follows
    wire[4] = SHARD_LEN;  // 37, so the long form
    wire[5] = 0xAB;       // identifier, shared across the backup
    wire[6] = 0xCD;
    wire[7] = 0x00;  // (group_threshold - 1) << 4 | (group_count - 1)
    wire[8] = (uint8_t) ((member_threshold - 1) & 0x0F);  // group_index 0
    wire[9] = member_index;  // reserved nibble stays zero
    memset(&wire[CBOR_LEN + SSKR_METADATA_LENGTH_BYTES], 0x42 + member_index, SHARD_VALUE_LEN);

    /* The application compares against os_swap_u32(cx_crc32(...)) on a
     * little-endian host, i.e. the CRC big-endian on the wire. */
    uint32_t crc = cx_crc32(wire, CBOR_LEN + SHARD_LEN);
    wire[WIRE_LEN - 4] = (uint8_t) (crc >> 24);
    wire[WIRE_LEN - 3] = (uint8_t) (crc >> 16);
    wire[WIRE_LEN - 2] = (uint8_t) (crc >> 8);
    wire[WIRE_LEN - 1] = (uint8_t) crc;
}

/*
 * One more shard than either array holds. Nothing here is malformed: this is
 * what an (n+1)-of-16 backup looks like when it is checked on a Nano S, where
 * SSS_MAX_SHARE_COUNT is 10.
 */
static void test_combine_does_not_overrun_on_oversized_count(void **state) {
    (void) state;

    const uint8_t count = SHARDS_CAPACITY + 1;
    uint8_t wire[(SHARDS_CAPACITY + 1) * WIRE_LEN];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    for (uint8_t i = 0; i < count; i++) {
        build_wire_shard(&wire[i * WIRE_LEN], count, i);
    }

    /* The gate the UI runs first must let this through, otherwise the test
     * would not reach the code it is about. */
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), count), 1);

    /* No assertion on the return value: see the file header. The point is
     * that this call completes without writing past either array. */
    (void) bolos_ux_sskr_combine(wire, sizeof(wire), count, output);
}

/*
 * A set at exactly the capacity must still be accepted by the bounds, so that
 * the fix cannot be an off-by-one that turns away a supported backup.
 */
static void test_combine_accepts_a_set_at_capacity(void **state) {
    (void) state;

    const uint8_t count = SHARDS_CAPACITY;
    uint8_t wire[SHARDS_CAPACITY * WIRE_LEN];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    for (uint8_t i = 0; i < count; i++) {
        build_wire_shard(&wire[i * WIRE_LEN], count, i);
    }

    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), count), 1);
    (void) bolos_ux_sskr_combine(wire, sizeof(wire), count, output);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_does_not_overrun_on_oversized_count),
        cmocka_unit_test(test_combine_accepts_a_set_at_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
