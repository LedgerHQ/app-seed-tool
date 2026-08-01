/*
 * Coverage for the seed_len bounds in bolos_ux_sskr_generate()
 * (seed_sskr.c). No defect is fixed here: the guard
 *
 *     if (!(SSKR_MIN_STRENGTH_BYTES <= seed_len &&
 *           seed_len <= SSKR_MAX_STRENGTH_BYTES) ||
 *         (seed_len % 2 != 0)) {
 *         return 0;
 *     }
 *
 * is present and correct. Nothing held it: the three files that already call
 * bolos_ux_sskr_generate() (sskr_bound_group_count.c,
 * sskr_generate_bound_groups.c, sskr_invalid_group_descriptor.c) all vary the
 * group descriptor and always pass a well-formed seed_len, so deleting the
 * guard outright left the whole suite green.
 *
 * Why the return value alone cannot hold it, and what these tests assert
 * instead:
 *
 * sskr_generate_shards() -- the next call in the function -- opens with
 * sskr_check_secret_length(), which applies the very same three conditions
 * and returns SSKR_ERROR_SECRET_TOO_SHORT / _TOO_LONG /
 * _LENGTH_NOT_EVEN. bolos_ux_sskr_generate() then sees a negative count and
 * returns 0 as well. So an out-of-range seed_len yields 0 either way, and a
 * test that only looked at the return value would keep passing with the
 * guard deleted.
 *
 * The difference is what happens to the caller's buffer on the way out. The
 * guard returns before touching it; the failure path that catches the
 * negative count first runs memzero(share_buffer, share_buffer_len) and
 * clears the whole thing. These tests therefore fill share_buffer with a
 * sentinel and require it to come back untouched -- rejection before any
 * write, which is the guard's observable contribution.
 *
 * (The two checks are not exact duplicates for a second reason, not
 * exercised here: seed_len is an unsigned int at this level, whereas
 * sskr_generate_shards() takes a uint16_t and sskr_check_secret_length()
 * takes a uint8_t. The guard is the only one that sees the full width.)
 *
 * The two accepted boundaries, 16 and 32, are covered as well, so that
 * tightening either end by one step is caught rather than silently
 * narrowing what the function accepts.
 *
 * Target setup mirrors test_sskr_generate_bound_groups: seed_sskr.c is
 * compiled directly into this target rather than linked, under ASan, so any
 * incidental overrun these lengths might provoke is reported.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sskr/seed_sskr_internal.h"
#include "sskr/sskr-constants.h"
#include "testutils.h"

/* One maximum-size shard, which is all a 1-of-1 descriptor ever produces.
 * Comfortably below the share_buffer_len cap bolos_ux_sskr_generate()
 * enforces, so that bound stays out of the way of this one. */
#define SHARE_BUFFER_LENGTH \
    (SSKR_MAX_STRENGTH_BYTES + SSKR_METADATA_LENGTH_BYTES)

/* Long enough for the largest seed_len tried below (34, one step past the
 * upper bound), so no case can read past the end of the seed. */
#define SEED_BUFFER_LENGTH (SSKR_MAX_STRENGTH_BYTES + 2)

#define SENTINEL 0xa5

/*
 * Call bolos_ux_sskr_generate() varying nothing but seed_len. Everything
 * else is the single configuration this port supports -- a 1-of-1 group,
 * groups_len at SSKR_MAX_GROUP_COUNT -- and the expected share length and
 * count are what that descriptor yields for the seed_len given, so a
 * rejection can only come from the seed_len itself and not from a
 * descriptor the function would have turned away first.
 *
 * share_buffer and *share_len are set to the sentinel before the call, so
 * the caller can tell "returned without writing" from "wrote, then
 * returned".
 */
static unsigned int generate_with_seed_len(unsigned int seed_len,
                                           uint8_t* share_buffer,
                                           uint8_t* share_len) {
    unsigned int group_descriptor[2] = {1, 1};
    unsigned char seed[SEED_BUFFER_LENGTH] = {0};

    memset(share_buffer, SENTINEL, SHARE_BUFFER_LENGTH);
    *share_len = SENTINEL;

    return bolos_ux_sskr_generate(
        1, group_descriptor, SSKR_MAX_GROUP_COUNT, seed, seed_len, share_len,
        share_buffer, SHARE_BUFFER_LENGTH,
        (uint8_t)(seed_len + SSKR_METADATA_LENGTH_BYTES), 1);
}

static void assert_untouched(const uint8_t* share_buffer, uint8_t share_len) {
    for (size_t i = 0; i < SHARE_BUFFER_LENGTH; i++) {
        assert_int_equal(share_buffer[i], SENTINEL);
    }
    assert_int_equal(share_len, SENTINEL);
}

/*
 * Below SSKR_MIN_STRENGTH_BYTES. Even, and so within the other two
 * conditions -- only the lower bound rejects it.
 */
static void test_generate_rejects_seed_len_below_minimum(void** state) {
    (void)state;

    uint8_t share_buffer[SHARE_BUFFER_LENGTH];
    uint8_t share_len;

    assert_int_equal(generate_with_seed_len(14, share_buffer, &share_len), 0);
    assert_untouched(share_buffer, share_len);
}

/*
 * Above SSKR_MAX_STRENGTH_BYTES, and even -- only the upper bound rejects
 * it.
 */
static void test_generate_rejects_seed_len_above_maximum(void** state) {
    (void)state;

    uint8_t share_buffer[SHARE_BUFFER_LENGTH];
    uint8_t share_len;

    assert_int_equal(generate_with_seed_len(34, share_buffer, &share_len), 0);
    assert_untouched(share_buffer, share_len);
}

/*
 * Inside [SSKR_MIN_STRENGTH_BYTES, SSKR_MAX_STRENGTH_BYTES] but odd -- only
 * the parity condition rejects it. SSKR splits the secret into two halves,
 * so an odd length has no valid split.
 */
static void test_generate_rejects_odd_seed_len(void** state) {
    (void)state;

    uint8_t share_buffer[SHARE_BUFFER_LENGTH];
    uint8_t share_len;

    assert_int_equal(generate_with_seed_len(17, share_buffer, &share_len), 0);
    assert_untouched(share_buffer, share_len);
}

/*
 * The two accepted ends. A 12-word phrase is 16 bytes of entropy and a
 * 24-word phrase 32, so both are lengths the application really passes;
 * tightening either end by one step would break it.
 */
static void test_generate_accepts_seed_len_at_minimum(void** state) {
    (void)state;

    uint8_t share_buffer[SHARE_BUFFER_LENGTH];
    uint8_t share_len;

    assert_int_equal(generate_with_seed_len(SSKR_MIN_STRENGTH_BYTES,
                                            share_buffer, &share_len),
                     1);
    assert_int_equal(share_len,
                     SSKR_MIN_STRENGTH_BYTES + SSKR_METADATA_LENGTH_BYTES);
}

static void test_generate_accepts_seed_len_at_maximum(void** state) {
    (void)state;

    uint8_t share_buffer[SHARE_BUFFER_LENGTH];
    uint8_t share_len;

    assert_int_equal(generate_with_seed_len(SSKR_MAX_STRENGTH_BYTES,
                                            share_buffer, &share_len),
                     1);
    assert_int_equal(share_len,
                     SSKR_MAX_STRENGTH_BYTES + SSKR_METADATA_LENGTH_BYTES);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_generate_rejects_seed_len_below_minimum),
        cmocka_unit_test(test_generate_rejects_seed_len_above_maximum),
        cmocka_unit_test(test_generate_rejects_odd_seed_len),
        cmocka_unit_test(test_generate_accepts_seed_len_at_minimum),
        cmocka_unit_test(test_generate_accepts_seed_len_at_maximum),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
