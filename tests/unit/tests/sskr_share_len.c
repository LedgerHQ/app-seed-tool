/*
 * Regression test for the unvalidated shard length in bolos_ux_sskr_combine().
 *
 * The function reads the length of each serialized shard straight out of the
 * CBOR header of the entered data:
 *
 *     uint8_t sskr_share_len = sskr_shares_hex[3] & 0x1F;
 *     if (sskr_share_len > 23) sskr_share_len = sskr_shares_hex[4];   // 0..255
 *
 * and hands it to sskr_combine_shards() unchecked. Down in
 * sskr_deserialize_shard() that becomes
 *
 *     shard->value_len = source_len - SSKR_METADATA_LENGTH_BYTES;
 *     memcpy(shard->value, source + SSKR_METADATA_LENGTH_BYTES, shard->value_len);
 *
 * and only then is the length validated -- the copy already happened.
 *
 * How far that copy reaches on this path is bounded, and worth writing down
 * because it is not obvious. The destination is shards[0].value inside
 * sskr_combine_shards()'s own
 *
 *     sskr_shard_t shards[SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT];
 *
 * which is 10 * 40 = 400 bytes on TARGET_NANOS; value sits at offset 8, and a
 * one-byte CBOR length cannot ask for more than 255 - 5 = 250. The write
 * therefore stays inside that array -- checked with sskr.c compiled under
 * AddressSanitizer, which reports nothing -- clobbering shards[1..6] while
 * they are still uninitialised. The loop then stops on the very error the copy
 * should have been refused by, and memzero() wipes the array on the way out.
 *
 * So this is robustness rather than memory safety: an input-derived length is
 * used before it is validated, and the only thing keeping that harmless is
 * arithmetic nobody wrote down. Raise SSS_MAX_SHARE_COUNT, widen the length
 * field or shrink sskr_shard_t and it stops holding.
 *
 * The assertion below is on the return value, which is all this can observe.
 * It is coupled to a separate defect: without the bound the call comes back
 * non-zero only because bolos_ux_sskr_combine() stores a negative error code
 * in a uint16_t, so its `output_len < 1` guard never fires. Fix that and the
 * assertion holds with or without the bound, and will need revisiting.
 *
 * A serialized shard is 5 metadata bytes plus a 16..32 byte value, so anything
 * outside 21..37 is not a shard and must be refused before any copy. Upstream
 * bc-sskr has the same ordering, but its callers are library users passing
 * their own sizes; here the value comes from data typed on the device, so the
 * bound belongs in this wrapper -- which also keeps sskr.c diffable against
 * upstream.
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

#define OVERLONG_VALUE_LEN (200)
#define CBOR_LONG_HEADER   (5) /* tag(3) + 0x58 + one length byte */

static void test_combine_rejects_overlong_shard_length(void **state)
{
    (void) state;

    /* One shard whose CBOR header claims a 200-byte body: well past the 37
     * bytes a real serialized shard can occupy. */
    uint8_t wire[CBOR_LONG_HEADER + OVERLONG_VALUE_LEN + 4];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    memset(wire, 0x42, sizeof(wire));
    wire[0] = 0xD9;
    wire[1] = 0x9D;
    wire[2] = 0x75;                      // CBOR tag #6.40309
    wire[3] = 0x58;                      // byte string, 1-byte length follows
    wire[4] = OVERLONG_VALUE_LEN;        // -> sskr_share_len = 200

    /* Shard metadata starts after the header; keep the reserved nibble zero so
     * the only thing wrong with this input is its length. */
    wire[CBOR_LONG_HEADER + 2] = 0x00;
    wire[CBOR_LONG_HEADER + 3] = 0x00;
    wire[CBOR_LONG_HEADER + 4] = 0x00;

    unsigned int len = bolos_ux_sskr_combine(wire, sizeof(wire), 1, output);

    /* Must be refused. Without the bound, sskr_deserialize_shard() copies
     * 195 bytes into a 32-byte field first. */
    assert_int_equal(len, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_rejects_overlong_shard_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
