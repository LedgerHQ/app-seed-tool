/*
 * Coverage for the input guards of src/common/sskr/sskr.c that a gcov run over
 * the whole suite shows with an execution count of zero. All of them are
 * reached through the public sskr_generate_shards() / sskr_combine_shards()
 * entry points; the two helpers involved are static.
 *
 * sskr_check_secret_length() -- distinct from the SSS-level check of the same
 * shape in sss.c, and reached first, so it is what a caller actually gets back:
 *   - len < SSKR_MIN_STRENGTH_BYTES -> SSKR_ERROR_SECRET_TOO_SHORT
 *   - len > SSKR_MAX_STRENGTH_BYTES -> SSKR_ERROR_SECRET_TOO_LONG
 *   - odd len                       -> SSKR_ERROR_SECRET_LENGTH_NOT_EVEN
 *
 * sskr_deserialize_shard(), on metadata read out of the serialized shard:
 *   - group threshold above group count -> SSKR_ERROR_INVALID_GROUP_THRESHOLD
 *   - non-zero reserved nibble          -> SSKR_ERROR_INVALID_RESERVED_BITS
 *
 * sskr_generate_shards(), once the shard count is known:
 *   - output buffer too small -> SSKR_ERROR_INSUFFICIENT_SPACE
 *
 * Two further zero-count lines, both in sskr_generate_shards_internal(), are
 * deliberately left uncovered because they cannot be reached:
 *
 *   - `if (shards_size < total_shards)`: its only caller passes the result of
 *     the same sskr_count_shards() call, with the same arguments, as
 *     shards_size -- the two are always equal.
 *   - `if (group_threshold > groups_len)`: sskr_count_shards() is called
 *     earlier in the same function and already returns
 *     SSKR_ERROR_INVALID_GROUP_THRESHOLD for exactly that condition, so the
 *     later check is unreachable whatever the caller does.
 *
 * Reaching either would mean calling a static function directly with arguments
 * no caller can produce, which would test the test harness rather than the
 * code. The reachable insufficient-space rejection, in sskr_generate_shards()
 * itself, is covered below instead.
 *
 * This is coverage on already-correct code, not a bug fix.
 */

#include <cmocka.h>
#include <lcx_rng.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sskr.h"
#include "testutils.h"

#define SECRET_CAPACITY (64)
#define SHARD_VALUE_LEN (32)
#define SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SHARD_VALUE_LEN)
#define MEMBER_COUNT (3)

static void generate_expecting(uint16_t master_secret_len, int16_t expected) {
    const sskr_group_descriptor_t groups[] = {{.threshold = 2, .count = 3}};
    uint8_t master_secret[SECRET_CAPACITY];
    uint8_t output[SHARD_LEN * MEMBER_COUNT];
    uint8_t shard_len = 0;

    memset(master_secret, 0x5A, sizeof(master_secret));
    memset(output, 0x00, sizeof(output));

    int16_t result =
        sskr_generate_shards(1, groups, 1, master_secret, master_secret_len,
                             &shard_len, output, sizeof(output), test_rng);

    assert_int_equal(result, expected);
}

static void test_generate_rejects_secret_shorter_than_min(void** state) {
    (void)state;

    /* Even, so only the lower-bound check can fire. */
    generate_expecting(SSKR_MIN_STRENGTH_BYTES - 2,
                       SSKR_ERROR_SECRET_TOO_SHORT);
}

static void test_generate_rejects_secret_longer_than_max(void** state) {
    (void)state;

    generate_expecting(SSKR_MAX_STRENGTH_BYTES + 2, SSKR_ERROR_SECRET_TOO_LONG);
}

static void test_generate_rejects_odd_secret_length(void** state) {
    (void)state;

    /* In range, so the two bounds checks pass and only the parity one is left.
     */
    generate_expecting(SSKR_MIN_STRENGTH_BYTES + 1,
                       SSKR_ERROR_SECRET_LENGTH_NOT_EVEN);
}

static void test_generate_rejects_undersized_output_buffer(void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {
        {.threshold = 2, .count = MEMBER_COUNT}};
    uint8_t master_secret[SHARD_VALUE_LEN];
    uint8_t output[SHARD_LEN * MEMBER_COUNT];
    uint8_t shard_len = 0;

    memset(master_secret, 0x5A, sizeof(master_secret));
    memset(output, 0x00, sizeof(output));

    /* One byte short of what the three serialized shards need. */
    int16_t result =
        sskr_generate_shards(1, groups, 1, master_secret, sizeof(master_secret),
                             &shard_len, output, sizeof(output) - 1, test_rng);

    assert_int_equal(result, SSKR_ERROR_INSUFFICIENT_SPACE);
}

/* Builds the serialized form sskr_combine_shards() deserializes: two
 * identifier bytes, then the packed group and member metadata, then the value.
 * The two callers below each corrupt one metadata byte. */
static void build_shard(uint8_t out[SHARD_LEN], uint8_t group_byte,
                        uint8_t member_byte) {
    out[0] = 0xAB;
    out[1] = 0xCD;
    out[2] = group_byte;
    out[3] = 0x01; /* group index 0, member threshold 2 */
    out[4] = member_byte;
    memset(&out[SSKR_METADATA_LENGTH_BYTES], 0x42, SHARD_VALUE_LEN);
}

static void test_combine_rejects_group_threshold_above_group_count(
    void** state) {
    (void)state;

    uint8_t shard[SHARD_LEN];
    uint8_t output[SHARD_VALUE_LEN];

    /* High nibble + 1 is the group threshold, low nibble + 1 the group count:
     * 2-of-1, which no generator can produce. */
    build_shard(shard, 0x10, 0x00);

    const uint8_t* shards[] = {shard};

    int16_t result =
        sskr_combine_shards(shards, SHARD_LEN, 1, output, sizeof(output));

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_THRESHOLD);
}

static void test_combine_rejects_non_zero_reserved_bits(void** state) {
    (void)state;

    uint8_t shard[SHARD_LEN];
    uint8_t output[SHARD_VALUE_LEN];

    /* Valid 1-of-1 group, but the reserved high nibble of the member byte is
     * set. */
    build_shard(shard, 0x00, 0x10);

    const uint8_t* shards[] = {shard};

    int16_t result =
        sskr_combine_shards(shards, SHARD_LEN, 1, output, sizeof(output));

    assert_int_equal(result, SSKR_ERROR_INVALID_RESERVED_BITS);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_generate_rejects_secret_shorter_than_min),
        cmocka_unit_test(test_generate_rejects_secret_longer_than_max),
        cmocka_unit_test(test_generate_rejects_odd_secret_length),
        cmocka_unit_test(test_generate_rejects_undersized_output_buffer),
        cmocka_unit_test(
            test_combine_rejects_group_threshold_above_group_count),
        cmocka_unit_test(test_combine_rejects_non_zero_reserved_bits),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
