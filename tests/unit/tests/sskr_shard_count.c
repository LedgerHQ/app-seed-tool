/*
 * Regression test for the missing upper bound on `shards_count` in
 * sskr_combine_shards().
 *
 * sskr_combine_shards() deserializes into a fixed-size stack array:
 *
 *     sskr_shard_t shards[SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT];
 *
 * SSKR_MAX_GROUP_COUNT is 1, and SSS_MAX_SHARE_COUNT is 10 on TARGET_NANOS
 * (16 elsewhere). The test derives its shard counts from those macros rather
 * than hardcoding a capacity, so it is valid for either configuration.
 *
 * Only `shards_count == 0` used to be rejected. A caller passing a larger
 * count wrote past the end of `shards` before any validation ran.
 *
 * This is reachable without an attacker and without a malformed shard: the
 * Nano S recovery UI derives its shard count from the member-threshold nibble
 * of the entered shard ((byte & 0x0F) + 1, so 1..16), while the Stax/Flex
 * generation UI accepts a threshold of up to 16. Verifying a legitimate
 * 12-of-16 backup on a Nano S therefore called this function with
 * shards_count = 12 against a 10-entry array.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "sskr.h"
#include "sss-constants.h"
#include "testutils.h"

#define SHARD_VALUE_LEN (32)
#define SHARD_LEN       (SSKR_METADATA_LENGTH_BYTES + SHARD_VALUE_LEN)
#define SHARDS_CAPACITY (SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT)

/*
 * Builds `count` well-formed serialized shards of a single 1-of-1 group whose
 * member threshold is `count`, i.e. exactly what a `count`-of-`count` backup
 * looks like on the wire. The shards are individually valid: they must survive
 * sskr_deserialize_shard() so that a failure can only come from the missing
 * bound and not from a rejected vector.
 */
static void build_shards(uint8_t raw[][SHARD_LEN], const uint8_t *ptrs[], uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        // identifier: shared by every shard of one backup
        raw[i][0] = 0xAB;
        raw[i][1] = 0xCD;
        // (group_threshold - 1) << 4 | (group_count - 1) -> a single 1-of-1 group
        raw[i][2] = 0x00;
        // group_index << 4 | (member_threshold - 1)
        raw[i][3] = (uint8_t) ((count - 1) & 0x0F);
        // reserved nibble MUST be zero | member_index
        raw[i][4] = i;
        memset(&raw[i][SSKR_METADATA_LENGTH_BYTES], 0x42 + i, SHARD_VALUE_LEN);
        ptrs[i] = raw[i];
    }
}

/*
 * A shard count above the capacity of the internal array must be rejected
 * before anything is written to it.
 */
static void test_sskr_combine_shard_count_overflow(void **state) {
    (void) state;

    const uint8_t count = SHARDS_CAPACITY + 2;
    uint8_t raw[SHARDS_CAPACITY + 2][SHARD_LEN];
    const uint8_t *shards[SHARDS_CAPACITY + 2];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    build_shards(raw, shards, count);

    int16_t result = sskr_combine_shards(shards, SHARD_LEN, count, output, sizeof(output));

    assert_int_equal(result, SSKR_ERROR_INVALID_SHARD_SET);
}

/*
 * Guards the bound against an off-by-one: a count equal to the capacity is
 * legal and must not be turned away by the new check. It still fails later on
 * (these are synthetic values, not a recoverable secret), so assert only that
 * it is not rejected as an invalid shard set.
 */
static void test_sskr_combine_shard_count_at_capacity(void **state) {
    (void) state;

    const uint8_t count = SHARDS_CAPACITY;
    uint8_t raw[SHARDS_CAPACITY][SHARD_LEN];
    const uint8_t *shards[SHARDS_CAPACITY];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    build_shards(raw, shards, count);

    int16_t result = sskr_combine_shards(shards, SHARD_LEN, count, output, sizeof(output));

    assert_int_not_equal(result, SSKR_ERROR_INVALID_SHARD_SET);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sskr_combine_shard_count_overflow),
        cmocka_unit_test(test_sskr_combine_shard_count_at_capacity)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
