/*
 * A 3-of-5 generate/combine round trip.
 *
 * Every other test in this suite uses a member threshold of 2 (and, with
 * SSKR_MAX_GROUP_COUNT == 1, a group threshold of 1). That leaves one loop in
 * sss_split_secret() (src/common/sskr/sss/sss.c) with a gcov iteration count of
 * zero:
 *
 *     for (uint8_t i = 0; i < threshold - 2; ++i, share += secret_length) {
 *         random_generator(share, secret_length);
 *         ...
 *
 * It produces the random shares beyond the first two, so it only runs from a
 * threshold of 3 upwards. The surrounding `else` branch is well covered; this
 * is about the loop inside it, and about sss_recover_secret() interpolating
 * over three shares rather than two.
 *
 * Rather than freezing a vector, this generates with a real threshold of 3 and
 * combines three of the five shards back, checking the recovered secret against
 * the original. The generator is the suite's deterministic cx_rng, the same one
 * tests/sskr.c uses, so the shards are reproducible.
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

#define MEMBER_THRESHOLD (3)
#define MEMBER_COUNT (5)
#define SECRET_LEN (32)
#define SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SECRET_LEN)

static const uint8_t master_secret[SECRET_LEN] = {
    0xE3, 0x95, 0x5C, 0xDA, 0x30, 0x47, 0x71, 0xC0, 0x03, 0x18, 0x95,
    0x63, 0x7F, 0x55, 0xC3, 0xAB, 0xE4, 0x51, 0x53, 0xC8, 0x7A, 0xBD,
    0x81, 0xC5, 0x1E, 0xD1, 0x4E, 0x8A, 0xAF, 0xA1, 0xAF, 0x13};

/* Generates the 3-of-5 backup and hands back the serialized shards. */
static void generate_three_of_five(uint8_t output[SHARD_LEN * MEMBER_COUNT],
                                   uint8_t* shard_len) {
    const sskr_group_descriptor_t groups[] = {
        {.threshold = MEMBER_THRESHOLD, .count = MEMBER_COUNT}};

    int16_t shard_count = sskr_generate_shards(
        1, groups, 1, master_secret, sizeof(master_secret), shard_len, output,
        SHARD_LEN * MEMBER_COUNT, cx_rng);

    assert_int_equal(shard_count, MEMBER_COUNT);
    assert_int_equal(*shard_len, SHARD_LEN);
}

static void test_three_of_five_round_trip(void** state) {
    (void)state;

    uint8_t output[SHARD_LEN * MEMBER_COUNT];
    uint8_t shard_len;

    generate_three_of_five(output, &shard_len);

    /* Three shards out of the five, and not the first three, so the member
     * indexes are not contiguous. */
    const uint8_t* shards[] = {&output[0 * SHARD_LEN], &output[2 * SHARD_LEN],
                               &output[4 * SHARD_LEN]};
    uint8_t recovered[SECRET_LEN];

    memset(recovered, 0x00, sizeof(recovered));

    int16_t recovered_len = sskr_combine_shards(
        shards, shard_len, MEMBER_THRESHOLD, recovered, sizeof(recovered));

    assert_int_equal(recovered_len, sizeof(master_secret));
    assert_memory_equal(recovered, master_secret, sizeof(master_secret));
}

static void test_two_shards_are_not_enough(void** state) {
    (void)state;

    uint8_t output[SHARD_LEN * MEMBER_COUNT];
    uint8_t shard_len;

    generate_three_of_five(output, &shard_len);

    /* One short of the threshold: confirms the threshold really is 3, so the
     * round trip above is not passing for some weaker reason. */
    const uint8_t* shards[] = {&output[0 * SHARD_LEN], &output[2 * SHARD_LEN]};
    uint8_t recovered[SECRET_LEN];

    memset(recovered, 0x5A, sizeof(recovered));

    int16_t recovered_len =
        sskr_combine_shards(shards, shard_len, 2, recovered, sizeof(recovered));

    assert_int_equal(recovered_len, SSKR_ERROR_NOT_ENOUGH_MEMBER_SHARDS);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_three_of_five_round_trip),
        cmocka_unit_test(test_two_shards_are_not_enough),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
