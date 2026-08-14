/*
 * Coverage for the sskr_count_shards() (src/common/sskr/sskr.c) rejections
 * that are already implemented correctly but were never exercised by any
 * existing test:
 *
 *   - group_threshold > groups_len -> SSKR_ERROR_INVALID_GROUP_THRESHOLD
 *   - a group's member threshold > its member count ->
 *     SSKR_ERROR_INVALID_MEMBER_THRESHOLD
 *   - groups_len < 1 -> SSKR_ERROR_INVALID_GROUP_LENGTH
 *   - a group with no members -> SSKR_ERROR_INVALID_GROUP_COUNT
 *   - a 1-of-many group -> SSKR_ERROR_INVALID_SINGLETON_MEMBER
 *
 * The first two were covered when this file was written; a gcov run over the
 * whole suite showed the last three still at an execution count of zero, so
 * they are covered here too rather than in a file of their own.
 *
 * This is coverage on already-correct code, not a bug fix -- every check
 * reads exactly as intended in sskr_count_shards().
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "sskr-constants.h"
#include "sskr.h"
#include "testutils.h"

static void test_count_shards_rejects_group_threshold_above_group_count(
    void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 1, .count = 1}};

    int16_t result = sskr_count_shards(2, groups, 1);

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_THRESHOLD);
}

static void test_count_shards_rejects_member_threshold_above_member_count(
    void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 3, .count = 2}};

    int16_t result = sskr_count_shards(1, groups, 1);

    assert_int_equal(result, SSKR_ERROR_INVALID_MEMBER_THRESHOLD);
}

static void test_count_shards_rejects_empty_group_list(void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 1, .count = 1}};

    /* Checked before anything else, so the descriptor above is never read. */
    int16_t result = sskr_count_shards(0, groups, 0);

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_LENGTH);
}

static void test_count_shards_rejects_group_without_members(void** state) {
    (void)state;

    const sskr_group_descriptor_t groups[] = {{.threshold = 1, .count = 0}};

    int16_t result = sskr_count_shards(1, groups, 1);

    assert_int_equal(result, SSKR_ERROR_INVALID_GROUP_COUNT);
}

static void test_count_shards_rejects_singleton_member_threshold(void** state) {
    (void)state;

    /* A 1-of-2 group: the second shard would be a full copy of the first, so
     * the configuration is rejected rather than silently duplicating. */
    const sskr_group_descriptor_t groups[] = {{.threshold = 1, .count = 2}};

    int16_t result = sskr_count_shards(1, groups, 1);

    assert_int_equal(result, SSKR_ERROR_INVALID_SINGLETON_MEMBER);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_count_shards_rejects_group_threshold_above_group_count),
        cmocka_unit_test(
            test_count_shards_rejects_member_threshold_above_member_count),
        cmocka_unit_test(test_count_shards_rejects_empty_group_list),
        cmocka_unit_test(test_count_shards_rejects_group_without_members),
        cmocka_unit_test(test_count_shards_rejects_singleton_member_threshold),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
