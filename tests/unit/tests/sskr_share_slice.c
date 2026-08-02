/*
 * bolos_ux_sskr_share_slice(): paging through a generated SSKR set.
 *
 * bolos_ux_bip39_to_sskr_convert() writes a whole share set into one buffer,
 * back to back and with no separator. Showing share n therefore means
 * dividing: offset = n * buffer_length / share_count, length =
 * buffer_length / share_count. That arithmetic lived in two copies, one in
 * bagl/ux_sskr.c (get_next_data()) and one in nbgl/ui.c
 * (review_sskr_shares_contentGetter()), and neither file is compiled into any
 * test target -- ux_sskr.c is not merely uncovered, it is absent from the
 * lcov report entirely.
 *
 * The BAGL copy is the one that cannot be reached any other way. Speculos
 * dropped Nano S, so no emulator runs that stack, and the two other devices
 * sharing the file (Nano S+, Nano X) only reach it through a UX_FLOW.
 *
 * What is worth holding here is the division. It is guarded, in the sense
 * that the BAGL caller's `1 <= index <= share_count` test cannot pass with a
 * count of zero -- but the guard and the division lived in the same
 * expression, and the NBGL caller had no guard at all.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdint.h>
#include <stdbool.h>

#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sss-constants.h"

/* Length of one share, in bytes, in the vectors below. Arbitrary; only the
 * relation between it, the buffer length and the share count matters. */
#define SHARE_LEN (46)

/*
 * A zero share count must be refused, not divided by. This is the invariant
 * the extraction exists for: the BAGL caller kept it out with a guard whose
 * reasoning had to be reconstructed by reading it (`index >= 1` and
 * `index <= share_count` cannot both hold when share_count is 0), and the
 * NBGL caller relied on never being opened on an empty set.
 */
static void test_share_slice_rejects_zero_count(void **state) {
    (void) state;

    size_t offset = 0xAA;
    size_t length = 0xBB;

    assert_false(bolos_ux_sskr_share_slice(0, 0, 0, &offset, &length));
    /* outputs left alone on refusal */
    assert_int_equal(offset, 0xAA);
    assert_int_equal(length, 0xBB);

    assert_false(bolos_ux_sskr_share_slice(SHARE_LEN, 0, 0, &offset, &length));
    assert_int_equal(offset, 0xAA);
    assert_int_equal(length, 0xBB);
}

/* An index at or past the share count is not a share. */
static void test_share_slice_rejects_index_past_end(void **state) {
    (void) state;

    size_t offset = 0xAA;
    size_t length = 0xBB;

    assert_false(bolos_ux_sskr_share_slice(3 * SHARE_LEN, 3, 3, &offset, &length));
    assert_int_equal(offset, 0xAA);
    assert_int_equal(length, 0xBB);

    assert_false(bolos_ux_sskr_share_slice(3 * SHARE_LEN, 3, 200, &offset, &length));
    assert_int_equal(offset, 0xAA);
    assert_int_equal(length, 0xBB);
}

/* The shares of a set tile the buffer exactly, in order, with no gap and no
 * overlap -- which is the whole of what the two callers needed. */
static void test_share_slice_tiles_the_buffer(void **state) {
    (void) state;

    for (uint8_t count = 1; count <= SSS_MAX_SHARE_COUNT; ++count) {
        const size_t buffer_length = (size_t) count * SHARE_LEN;
        size_t expected_offset = 0;

        for (uint8_t index = 0; index < count; ++index) {
            size_t offset = 0;
            size_t length = 0;

            assert_true(bolos_ux_sskr_share_slice(buffer_length, count, index,
                                                  &offset, &length));
            assert_int_equal(length, SHARE_LEN);
            assert_int_equal(offset, expected_offset);
            expected_offset += length;
        }

        /* the last share ends exactly at the end of the buffer */
        assert_int_equal(expected_offset, buffer_length);
    }
}

/*
 * The first share starts at 0 and the last one ends inside the buffer. The
 * `- 1` the BAGL caller applies to its one-based index used to sit inside the
 * offset expression; getting it wrong there would have read one share too far
 * on the last page, past the end of the set.
 */
static void test_share_slice_stays_inside_the_buffer(void **state) {
    (void) state;

    const uint8_t count = SSS_MAX_SHARE_COUNT;
    const size_t buffer_length = (size_t) count * SHARE_LEN;
    size_t offset = 0;
    size_t length = 0;

    assert_true(bolos_ux_sskr_share_slice(buffer_length, count, 0, &offset, &length));
    assert_int_equal(offset, 0);

    assert_true(bolos_ux_sskr_share_slice(buffer_length, count, count - 1, &offset,
                                          &length));
    assert_true(offset + length <= buffer_length);
    assert_int_equal(offset + length, buffer_length);
}

/* A single share occupies the whole buffer: the degenerate case the 1-of-1
 * backup produces, and the one where a stray `- 1` would underflow. */
static void test_share_slice_single_share(void **state) {
    (void) state;

    size_t offset = 0;
    size_t length = 0;

    assert_true(bolos_ux_sskr_share_slice(SHARE_LEN, 1, 0, &offset, &length));
    assert_int_equal(offset, 0);
    assert_int_equal(length, SHARE_LEN);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_share_slice_rejects_zero_count),
        cmocka_unit_test(test_share_slice_rejects_index_past_end),
        cmocka_unit_test(test_share_slice_tiles_the_buffer),
        cmocka_unit_test(test_share_slice_stays_inside_the_buffer),
        cmocka_unit_test(test_share_slice_single_share)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
