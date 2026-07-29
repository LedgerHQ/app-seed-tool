/*
 * Coverage for two sskr_count_shards() (src/common/sskr/sskr.c) rejections
 * that are already implemented correctly but, per a grep of every existing
 * test source file, never exercised by any existing test:
 *
 *   - group_threshold > groups_len -> SSKR_ERROR_INVALID_GROUP_THRESHOLD
 *   - a group's member threshold > its member count ->
 *     SSKR_ERROR_INVALID_MEMBER_THRESHOLD
 *
 * This is coverage on already-correct code, not a bug fix -- both checks
 * read exactly as intended in sskr_count_shards().
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

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_count_shards_rejects_group_threshold_above_group_count),
        cmocka_unit_test(
            test_count_shards_rejects_member_threshold_above_member_count),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
