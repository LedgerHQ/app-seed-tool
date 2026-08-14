/*
 * bolos_ux_sskr_groups_from_descriptor() (seed_sskr.c): the single place the
 * flat group descriptor the UI layers hold is turned into the
 * sskr_group_descriptor_t array the SSKR core takes.
 *
 * Until that function existed, bolos_ux_sskr_size_get() and
 * bolos_ux_sskr_generate() each did it themselves, reading the same array with
 * two different index expressions:
 *
 *     groups[i].threshold =
 *         *(group_descriptor + i * sizeof(*(group_descriptor)) / groups_len);
 *     groups[i].threshold = *(group_descriptor + i * 2);
 *
 * The descriptor is an `unsigned int[N][2]` passed as a pointer to its first
 * element (nbgl/sskr_shares.c, bagl/ux_nano.h), so the second is the right one:
 * group i holds its threshold at [i * 2] and its member count at [i * 2 + 1].
 * The first is `i * sizeof(unsigned int) / groups_len`, that is `i * 4 /
 * groups_len` -- a size in bytes divided into an index in elements -- and
 * coincides with the second only for the two smallest group counts:
 *
 *     groups_len | i * 4 / groups_len | i * 2      | agree
 *     1          | 0                  | 0          | yes
 *     2          | 0, 2               | 0, 2       | yes, by coincidence
 *     3          | 0, 1, 2            | 0, 2, 4    | no
 *     4          | 0, 1, 2, 3         | 0, 2, 4, 6 | no
 *
 * Neither entry point could be held to that table. Both size their local
 * `groups[SSKR_MAX_GROUP_COUNT]` from a constant that is 1 in this port, and
 * both reject a groups_len above it, so i never left 0 and the two expressions
 * never disagreed on a call that could happen. Taking the destination and its
 * capacity as parameters, rather than assuming SSKR_MAX_GROUP_COUNT, is what
 * makes the last two rows reachable at all: the tests below supply a
 * destination of their own.
 *
 * That the extraction left the two entry points behaving as before, for the
 * single group this port allows, is what the rest of the suite already checks:
 * every seed_sskr.c target goes through one or both of them.
 *
 * Built with AddressSanitizer for test_rejects_groups_len_above_capacity: the
 * capacity bound exists to prevent a write, and the return value cannot tell a
 * working bound from a missing one -- past the end of a
 * sskr_group_descriptor_t[2] the stores are plain indexed writes, which ASan's
 * stack-buffer-overflow instrumentation does report (unlike the
 * memzero()/explicit_bzero() overruns elsewhere in this suite, which it does
 * not intercept). seed_sskr.c is compiled directly into the target rather than
 * linked from a library so its frames are instrumented too, matching
 * test_sskr_generate_bound_groups.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sskr/seed_sskr_internal.h"
#include "sskr/sskr-constants.h"

#define GROUPS_CAPACITY(array) (sizeof(array) / sizeof((array)[0]))

/* Four groups' worth of (threshold, count) pairs, all distinct and none equal
 * to its own index, so that reading the wrong element cannot pass by accident.
 * Declared the way the UI layers declare it -- `unsigned int[N][2]` -- and
 * passed as `descriptor[0]`, so what these tests exercise is the layout the
 * application actually hands over and not a hand-flattened stand-in. */
static const unsigned int descriptor[4][2] = {
    {1, 1},
    {2, 3},
    {3, 5},
    {4, 7},
};

/*
 * The one configuration this port supports: a single group, into a destination
 * sized exactly as both entry points size theirs.
 */
static void test_fills_one_group(void** state) {
    (void)state;

    sskr_group_descriptor_t groups[SSKR_MAX_GROUP_COUNT];

    assert_true(bolos_ux_sskr_groups_from_descriptor(descriptor[0], 1, groups,
                                                     SSKR_MAX_GROUP_COUNT));
    assert_int_equal(groups[0].threshold, descriptor[0][0]);
    assert_int_equal(groups[0].count, descriptor[0][1]);
}

/*
 * Every group gets the pair at its own offset. One test per row of the table in
 * the file header rather than a loop over the four counts, so that a run names
 * the counts that disagree: cmocka stops a test at its first failed assertion,
 * and "groups_len 3 and 4 are wrong while 1 and 2 are right" is the whole point
 * here.
 *
 * With `i * 4 / groups_len` the groups_len 3 and 4 calls read [0, 1, 2] and
 * [0, 1, 2, 3] instead of [0, 2, 4] and [0, 2, 4, 6], so group 1 comes out as
 * (descriptor[0][1], descriptor[1][0]) -- the member count of the first group
 * paired with the threshold of the second.
 */
static void assert_expansion(uint8_t groups_len) {
    sskr_group_descriptor_t groups[4];
    memset(groups, 0xAA, sizeof(groups));

    assert_true(bolos_ux_sskr_groups_from_descriptor(
        descriptor[0], groups_len, groups, GROUPS_CAPACITY(groups)));

    for (uint8_t i = 0; i < groups_len; i++) {
        assert_int_equal(groups[i].threshold, descriptor[i][0]);
        assert_int_equal(groups[i].count, descriptor[i][1]);
    }
}

static void test_fills_two_groups(void** state) {
    (void)state;
    assert_expansion(2);
}

static void test_fills_three_groups(void** state) {
    (void)state;
    assert_expansion(3);
}

static void test_fills_four_groups(void** state) {
    (void)state;
    assert_expansion(4);
}

/*
 * A groups_len at exactly the capacity must still be accepted, so that the
 * bound cannot be an off-by-one that turns away the largest configuration a
 * caller legitimately asks for.
 */
static void test_accepts_groups_len_at_capacity(void** state) {
    (void)state;

    sskr_group_descriptor_t groups[2];

    assert_true(bolos_ux_sskr_groups_from_descriptor(
        descriptor[0], GROUPS_CAPACITY(groups), groups,
        GROUPS_CAPACITY(groups)));
    assert_int_equal(groups[1].threshold, descriptor[1][0]);
    assert_int_equal(groups[1].count, descriptor[1][1]);
}

/*
 * One group more than the destination holds. The function must refuse rather
 * than write past the end of it, and must leave what is already there alone:
 * the sentinel says "wrote nothing", the sanitizer says "wrote nothing past the
 * end".
 */
static void test_rejects_groups_len_above_capacity(void** state) {
    (void)state;

    sskr_group_descriptor_t groups[2];
    memset(groups, 0xAA, sizeof(groups));

    assert_false(bolos_ux_sskr_groups_from_descriptor(
        descriptor[0], GROUPS_CAPACITY(groups) + 1, groups,
        GROUPS_CAPACITY(groups)));
    for (size_t i = 0; i < GROUPS_CAPACITY(groups); i++) {
        assert_int_equal(groups[i].threshold, 0xAA);
        assert_int_equal(groups[i].count, 0xAA);
    }
}

/*
 * Zero groups is accepted and writes nothing, which is what both entry points
 * did before the extraction: their loop simply did not run, and
 * sskr_count_shards() is left to reject the count. Pinned here so the
 * extraction cannot quietly turn it into a rejection.
 */
static void test_zero_groups_len_writes_nothing(void** state) {
    (void)state;

    sskr_group_descriptor_t groups[1];
    memset(groups, 0xAA, sizeof(groups));

    assert_true(bolos_ux_sskr_groups_from_descriptor(descriptor[0], 0, groups,
                                                     GROUPS_CAPACITY(groups)));
    assert_int_equal(groups[0].threshold, 0xAA);
    assert_int_equal(groups[0].count, 0xAA);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_fills_one_group),
        cmocka_unit_test(test_fills_two_groups),
        cmocka_unit_test(test_fills_three_groups),
        cmocka_unit_test(test_fills_four_groups),
        cmocka_unit_test(test_accepts_groups_len_at_capacity),
        cmocka_unit_test(test_rejects_groups_len_above_capacity),
        cmocka_unit_test(test_zero_groups_len_writes_nothing),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
