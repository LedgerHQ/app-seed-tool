/*
 * The two SSKR menus of the BAGL stack: how many shares, and what threshold.
 *
 * Both are a list of consecutive numbers starting at 1, read out of one table
 * of labels by a UX_STEP_MENULIST getter. The table and its bound used to sit
 * in src/bagl/ux_sskr.c, which is compiled into no test target at all -- so
 * it does not appear in the coverage report at 0 %, it does not appear in it.
 * And no emulator covers Nano S, the device whose table is the narrower of
 * the two, so nothing else exercised this either.
 *
 * The property worth holding is not the lookup, it is the agreement between
 * two constants that live in different files under the same guard used twice:
 *
 *   - sskr_descriptor_values[] has 7 entries under TARGET_NANOS, 16 otherwise
 *     (src/bagl/ux_sskr_menu.c);
 *   - SSS_MAX_SHARE_COUNT is 10 under TARGET_NANOS, 16 otherwise
 *     (src/common/sskr/sss/sss-constants.h).
 *
 * Every share the share-count menu offers is a share sss_split_secret() is
 * then asked to produce, and it refuses a count above SSS_MAX_SHARE_COUNT.
 * The second configuration has no slack whatever: 16 and 16. A static
 * assertion in ux_sskr_menu.c holds that at compile time for the device
 * builds; this file holds the same relation on the host, in both
 * configurations, and holds the chain the threshold menu depends on.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>

#include "bagl/ux_sskr_menu.h"
#include "sss-constants.h"

/*
 * The share-count menu never offers a count the Shamir layer would refuse.
 * The same relation the static assertion holds, replayed here so that the
 * TARGET_NANOS build of this file states it for the device that has no
 * emulator.
 */
static void test_menu_never_exceeds_sss_max_share_count(void **state) {
    (void) state;

    assert_true(sskr_descriptor_count() >= 1);
    assert_true(sskr_descriptor_count() <= SSS_MAX_SHARE_COUNT);
}

/*
 * A menu of `count` entries answers for 0..count-1 and stops after that.
 * NULL is what UX_STEP_MENULIST reads as the end of the list, so returning a
 * label one entry too far would add a selectable share count that nothing
 * downstream expects.
 */
static void test_menu_ends_at_count(void **state) {
    (void) state;

    for (unsigned int count = 1; count <= sskr_descriptor_count(); ++count) {
        for (unsigned int idx = 0; idx < count; ++idx) {
            assert_non_null(sskr_descriptor_label(idx, count));
        }
        assert_null(sskr_descriptor_label(count, count));
        assert_null(sskr_descriptor_label(count + 1, count));
    }
}

/* An empty menu has no entries at all, including the first. */
static void test_menu_of_zero_is_empty(void **state) {
    (void) state;

    assert_null(sskr_descriptor_label(0, 0));
}

/*
 * Entry `idx` reads "idx + 1", which is what makes the selectors' `idx + 1`
 * agree with what the user saw. A shifted table would let someone pick "3"
 * and get a 4-of-n backup.
 */
static void test_menu_labels_are_one_based(void **state) {
    (void) state;

    for (unsigned int idx = 0; idx < sskr_descriptor_count(); ++idx) {
        char expected[12];

        assert_true(idx + 1 <= 99);
        (void) snprintf(expected, sizeof(expected), "%u", idx + 1);
        assert_string_equal(sskr_descriptor_label(idx, sskr_descriptor_count()),
                            expected);
    }
}

/*
 * A threshold can never exceed the number of shares it would be taken out of.
 * That is not enforced anywhere downstream with a message of its own -- a
 * 3-of-2 backup would be turned away by sss_validate_parameters() much later,
 * behind a generic error screen. What holds it is that the threshold menu is
 * built with the share count the user just picked as its length, so this
 * walks the whole chain: every share count the first menu offers, and every
 * threshold the second menu then offers for it.
 */
static void test_threshold_menu_never_exceeds_chosen_share_count(void **state) {
    (void) state;

    for (unsigned int idx = 0; idx < sskr_descriptor_count(); ++idx) {
        /* what sskr_shares_number_selector() stores for entry `idx` */
        const unsigned int share_count = idx + 1;

        /* what sskr_threshold_getter() then offers */
        for (unsigned int t = 0; t < share_count; ++t) {
            assert_non_null(sskr_descriptor_label(t, share_count));
        }
        /* and what it refuses: a threshold of share_count + 1 */
        assert_null(sskr_descriptor_label(share_count, share_count));

        /* the largest threshold selectable is exactly the share count */
        assert_true(share_count <= sskr_descriptor_count());
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_menu_never_exceeds_sss_max_share_count),
        cmocka_unit_test(test_menu_ends_at_count),
        cmocka_unit_test(test_menu_of_zero_is_empty),
        cmocka_unit_test(test_menu_labels_are_one_based),
        cmocka_unit_test(test_threshold_menu_never_exceeds_chosen_share_count)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
