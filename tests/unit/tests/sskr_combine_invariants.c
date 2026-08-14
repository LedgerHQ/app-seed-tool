/*
 * Regression coverage for the shard-set invariants enforced inside
 * sskr_combine_shards_internal() (src/common/sskr/sskr.c) before shares are
 * ever handed to Shamir interpolation. All three checks already exist and
 * already return SSKR_ERROR_INVALID_SHARD_SET; grepping tests/unit/tests for
 * *.c before writing this file found no existing test that reaches any of them.
 * This is coverage on already-correct code, not a bug fix.
 *
 * - a multi-group shard set (differing group_index) is rejected because
 *   SSKR_MAX_GROUP_COUNT is 1 in this port: the second distinct group_index
 *   trips `next_group >= SSKR_MAX_GROUP_COUNT` (sskr.c:473);
 * - shards carrying a different identifier are rejected on the very first
 *   metadata comparison (sskr.c:441);
 * - shards carrying a different group_threshold are rejected by the same
 *   comparison (sskr.c:442).
 *
 * The helper below extends the one in sskr_combine_error.c (which hardcodes
 * a 1-of-1 group and group_index 0) with the parameters needed to build
 * these three scenarios: identifier, group_index, group_threshold and
 * group_count are now all caller-controlled, using the same wire layout
 * documented in sskr_serialize_shard() (sskr.c) and matched here byte for
 * byte.
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
 * CRC32. Every field of the shard's metadata is caller-controlled so the
 * same helper can build both consistent and inconsistent shard pairs. */
static void build_wire_shard(uint8_t out[WIRE_LEN], uint16_t identifier,
                             uint8_t group_index, uint8_t group_threshold,
                             uint8_t group_count, uint8_t member_index,
                             uint8_t member_threshold)
{
    out[0] = 0xD9;
    out[1] = 0x9D;
    out[2] = 0x75;                       // CBOR tag #6.40309
    out[3] = 0x40 | (SHARD_LEN & 0x1F);  // byte string, short form

    out[CBOR_LEN + 0] = (uint8_t) (identifier >> 8);
    out[CBOR_LEN + 1] = (uint8_t) (identifier & 0xFF);
    out[CBOR_LEN + 2] = (uint8_t) ((((group_threshold - 1) & 0x0F) << 4) |
                                    ((group_count - 1) & 0x0F));
    out[CBOR_LEN + 3] = (uint8_t) (((group_index & 0x0F) << 4) |
                                    ((member_threshold - 1) & 0x0F));
    out[CBOR_LEN + 4] = member_index & 0x0F;  // reserved nibble stays zero
    memset(&out[CBOR_LEN + SSKR_METADATA_LENGTH_BYTES], 0x42, SHARD_VALUE_LEN);

    /* The CRC is not checked by bolos_ux_sskr_combine() itself, but keeping
     * the frame honest documents that these bytes are a legitimate shard. */
    memset(&out[CBOR_LEN + SHARD_LEN], 0x00, CRC_LEN);
}

/* Two well-formed shards, same identifier and group_threshold, but a
 * different group_index (0 and 1) -- a two-group set. SSKR_MAX_GROUP_COUNT
 * is 1 in this port, so this must be rejected, never silently combined. */
static void test_combine_rejects_multi_group_set(void **state)
{
    (void) state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[32];

    build_wire_shard(&wire[0], 0xABCD, 0, 1, 2, 0, 2);
    build_wire_shard(&wire[WIRE_LEN], 0xABCD, 1, 1, 2, 0, 2);

    unsigned int len = bolos_ux_sskr_combine(wire, sizeof(wire), 2, output);

    assert_int_equal(len, 0);
}

/* Two well-formed shards that otherwise look interchangeable (same group,
 * distinct member indices) but come from different backups: the identifier
 * differs. Must never be combined into a bogus secret. */
static void test_combine_rejects_shards_from_different_backups(void **state)
{
    (void) state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[32];

    build_wire_shard(&wire[0], 0xABCD, 0, 1, 1, 0, 2);
    build_wire_shard(&wire[WIRE_LEN], 0xABCE, 0, 1, 1, 1, 2);

    unsigned int len = bolos_ux_sskr_combine(wire, sizeof(wire), 2, output);

    assert_int_equal(len, 0);
}

/* Two well-formed shards, same identifier, but declaring a different
 * group_threshold (1 vs 2) -- an inconsistent shard set. */
static void test_combine_rejects_inconsistent_group_threshold(void **state)
{
    (void) state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[32];

    build_wire_shard(&wire[0], 0xABCD, 0, 1, 1, 0, 2);
    build_wire_shard(&wire[WIRE_LEN], 0xABCD, 0, 2, 2, 1, 2);

    unsigned int len = bolos_ux_sskr_combine(wire, sizeof(wire), 2, output);

    assert_int_equal(len, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_rejects_multi_group_set),
        cmocka_unit_test(test_combine_rejects_shards_from_different_backups),
        cmocka_unit_test(test_combine_rejects_inconsistent_group_threshold),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
