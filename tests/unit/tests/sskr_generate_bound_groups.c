/*
 * Regression test for the missing upper bound on `groups_len` in
 * bolos_ux_sskr_generate() and bolos_ux_sskr_size_get() (seed_sskr.c).
 *
 * Both functions declare a fixed-size local array
 *
 *     sskr_group_descriptor_t groups[SSKR_MAX_GROUP_COUNT];
 *
 * and fill it in a loop bounded only by the caller-supplied `groups_len`,
 * never checked against SSKR_MAX_GROUP_COUNT (1 in this port) before the
 * loop runs. sskr_count_shards(), called afterwards, validates other things
 * but never this. A groups_len above SSKR_MAX_GROUP_COUNT writes past the
 * end of `groups` -- a real stack overflow -- before any other check has a
 * chance to run.
 *
 * Not reachable today: the only existing caller,
 * bolos_ux_bip39_to_sskr_convert() (same file), hard-codes groups_len = 1.
 * Both are public entry points nonetheless, same standing as the other
 * defensive bounds already added elsewhere in this file's callees (see
 * sskr_shard_count.c, sskr_generate_split_error.c).
 *
 * Verification technique: these are plain indexed writes (`groups[i].x =
 * ...`), not a memzero()/explicit_bzero() call, so -- unlike the
 * memzero-based overrun in sss_recover_erase_length.c, which this
 * toolchain's ASan does not catch -- AddressSanitizer's ordinary
 * stack-buffer-overflow instrumentation does apply here. Confirmed by
 * running this file under ASan before the fix: it aborts with a
 * stack-buffer-overflow report at the `groups[i].threshold = ...` store,
 * exactly like the existing test_sskr_combine_bounds target. A
 * canary-struct technique (sss_recover_erase_length.c's approach) does not
 * apply: `groups` is entirely local to the function under test, so no
 * caller-supplied struct can be laid out adjacent to it.
 *
 * seed_sskr.c is compiled directly into this target (rather than linked)
 * so ASan instruments its stack frames, matching test_sskr_combine_bounds.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "sskr/sskr-constants.h"
#include "testutils.h"

#define SECRET_LEN (SSKR_MIN_STRENGTH_BYTES)

/* bolos_ux_sskr_generate() and bolos_ux_sskr_size_get() are public entry
 * points (external linkage) but only ever called from within seed_sskr.c
 * itself, so neither is declared in common_sskr.h. */
extern int16_t bolos_ux_sskr_size_get(uint8_t bip39_type,
                                      uint8_t groups_threshold,
                                      unsigned int* group_descriptor,
                                      uint8_t groups_len, uint8_t* share_len);

extern unsigned int bolos_ux_sskr_generate(
    uint8_t groups_threshold, unsigned int* group_descriptor,
    uint8_t groups_len, unsigned char* seed, unsigned int seed_len,
    uint8_t* share_len, unsigned char* share_buffer,
    unsigned int share_buffer_len, uint8_t share_len_expected,
    int16_t share_count_expected);

/*
 * One more group than `groups[SSKR_MAX_GROUP_COUNT]` (and hence
 * SSKR_MAX_GROUP_COUNT itself) can hold. The values themselves do not
 * matter -- the array-filling loop runs before any of them are used --
 * only that groups_len exceeds the array's capacity.
 */
static void test_size_get_does_not_overrun_on_oversized_groups_len(
    void** state) {
    (void)state;

    const unsigned int group_descriptor[] = {2, 2, 1, 1};
    uint8_t share_len = 0;

    /* No assertion on the return value: see the file header. The point is
     * that this call completes without writing past `groups`. */
    (void)bolos_ux_sskr_size_get(BIP39_MNEMONIC_SIZE_12, 1,
                                 (unsigned int*)group_descriptor,
                                 SSKR_MAX_GROUP_COUNT + 1, &share_len);
}

static void test_generate_does_not_overrun_on_oversized_groups_len(
    void** state) {
    (void)state;

    const unsigned int group_descriptor[] = {2, 2, 1, 1};
    uint8_t seed[SECRET_LEN] = {0};
    uint8_t share_buffer[(SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * 4];
    uint8_t share_len = 0;

    (void)bolos_ux_sskr_generate(
        1, (unsigned int*)group_descriptor, SSKR_MAX_GROUP_COUNT + 1, seed,
        SECRET_LEN, &share_len, share_buffer, sizeof(share_buffer), 0, 0);
}

/*
 * A groups_len at exactly the capacity must still be accepted, so that the
 * fix cannot be an off-by-one that turns away the one configuration this
 * port supports. share_len_expected/share_count_expected are computed the
 * same way bolos_ux_bip39_to_sskr_convert() does: via bolos_ux_sskr_size_get()
 * for the same descriptor.
 */
static void test_size_get_accepts_groups_len_at_capacity(void** state) {
    (void)state;

    const unsigned int group_descriptor[] = {1, 1};
    uint8_t share_len = 0;

    int16_t share_count = bolos_ux_sskr_size_get(
        BIP39_MNEMONIC_SIZE_12, 1, (unsigned int*)group_descriptor,
        SSKR_MAX_GROUP_COUNT, &share_len);

    assert_int_equal(share_count, 1);
    assert_int_equal(
        share_len, BIP39_MNEMONIC_SIZE_12 * 4 / 3 + SSKR_METADATA_LENGTH_BYTES);
}

static void test_generate_accepts_groups_len_at_capacity(void** state) {
    (void)state;

    const unsigned int group_descriptor[] = {1, 1};
    uint8_t seed[SECRET_LEN] = {0};
    uint8_t share_buffer[(SECRET_LEN + SSKR_METADATA_LENGTH_BYTES) * 4] = {0};
    uint8_t share_len = 0;
    uint8_t share_len_expected = 0;

    int16_t share_count_expected = bolos_ux_sskr_size_get(
        BIP39_MNEMONIC_SIZE_12, 1, (unsigned int*)group_descriptor,
        SSKR_MAX_GROUP_COUNT, &share_len_expected);

    unsigned int share_count = bolos_ux_sskr_generate(
        1, (unsigned int*)group_descriptor, SSKR_MAX_GROUP_COUNT, seed,
        SECRET_LEN, &share_len, share_buffer, sizeof(share_buffer),
        share_len_expected, share_count_expected);

    assert_int_equal(share_count, 1);
    assert_int_equal(share_len, SECRET_LEN + SSKR_METADATA_LENGTH_BYTES);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(
            test_size_get_does_not_overrun_on_oversized_groups_len),
        cmocka_unit_test(
            test_generate_does_not_overrun_on_oversized_groups_len),
        cmocka_unit_test(test_size_get_accepts_groups_len_at_capacity),
        cmocka_unit_test(test_generate_accepts_groups_len_at_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
