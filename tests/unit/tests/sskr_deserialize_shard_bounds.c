/*
 * sskr_deserialize_shard() derives the length of the copy from source_len,
 * which its caller supplies, and writes into sskr_shard_t::value -- a fixed
 * 32-byte field. The length check that bounds it, sskr_check_secret_length(),
 * used to run after the memcpy, so a source_len of 60 wrote 55 bytes into
 * those 32 before returning SSKR_ERROR_SECRET_TOO_LONG.
 *
 * Why this needs its own translation unit rather than the public entry point.
 * sskr_deserialize_shard() is static, and reached the only way the application
 * reaches it -- through sskr_combine_shards() -- the overrun lands inside that
 * function's own
 *
 *     sskr_shard_t shards[SSS_MAX_SHARE_COUNT * SSKR_MAX_GROUP_COUNT];
 *
 * clobbering shards[1..] while they are still uninitialised but never leaving
 * the array. That is an intra-object overrun: AddressSanitizer places its
 * redzones between objects, not between the fields of one, and reports
 * nothing. A test driving sskr_combine_shards() under ASan would therefore
 * pass whichever order the two statements are in, and prove nothing. Building
 * sskr.c into this file instead lets the destination be an object of its own,
 * so the write is observable -- see the two overlong tests below, which check
 * it twice over: once with sentinel bytes, which hold with or without a
 * sanitizer, and once against an exact-size heap block, which ASan flags
 * immediately.
 *
 * Note what this is and is not. Through the application the bound in
 * bolos_ux_sskr_combine() keeps source_len in 21..37 before the call, so the
 * ordering was never reachable from the device, and the overrun described
 * above stayed inside the caller's array: robustness rather than memory
 * safety, with arithmetic nobody wrote down as the only thing keeping it
 * harmless. sskr_combine_shards() is public API in sskr.h, though, and a
 * caller there passes its own shard_len with nothing checking it.
 */

// clang-format off
/* The root .clang-format has duplicate keys, so its `SortIncludes: false`
 * never applies and includes get sorted -- which breaks this file, since
 * testutils.h supplies the WIDE qualifier that headers below rely on, and
 * sskr.c must come last of all. */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include <stdlib.h>
#include <string.h>

#include "testutils.h"
#include "sskr/sskr-constants.h"
#include "sskr/shard.h"

/* Built into this file on purpose: sskr_deserialize_shard() is static.
 * The target does not link libsskr, which would duplicate every symbol.
 * The directory prefix matters: an unprefixed "sskr.c" resolves to this
 * directory's own tests/sskr.c, which has a main() of its own. */
#include "sskr/sskr.c"
// clang-format on

/* sskr_check_secret_length() bounds value_len with SSKR_MAX_STRENGTH_BYTES,
 * not with sizeof(shard->value). They are the same today; if they ever stop
 * being, the check stops protecting the field and this fails to build. */
_Static_assert(
    sizeof(((sskr_shard_t*)0)->value) == SSKR_MAX_STRENGTH_BYTES,
    "sskr_check_secret_length() no longer bounds sskr_shard_t::value");

#define PAYLOAD_BYTE (0x42)
#define SENTINEL_BYTE (0xa5)
#define SENTINEL_LEN (64)

/* A serialized shard whose metadata is deliberately unremarkable: group
 * threshold 1 of 1, reserved nibble clear. The only thing under test is the
 * length, so nothing else may be a reason to refuse the input. */
static void build_shard(uint8_t* source, uint16_t source_len) {
    memset(source, PAYLOAD_BYTE, source_len);
    source[0] = 0x00;
    source[1] = 0x01;
    source[2] = 0x00; /* group_threshold 1, group_count 1 */
    source[3] = 0x00;
    source[4] = 0x00; /* reserved nibble clear, member_index 0 */
}

static void test_deserialize_shard_accepts_maximum_length(void** state) {
    (void)state;

    uint8_t source[SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_MAX_STRENGTH_BYTES);
    assert_int_equal(shard.value_len, SSKR_MAX_STRENGTH_BYTES);
    /* The copy still happens on the accepted path. */
    assert_memory_equal(shard.value, source + SSKR_METADATA_LENGTH_BYTES,
                        SSKR_MAX_STRENGTH_BYTES);
}

static void test_deserialize_shard_rejects_one_past_maximum(void** state) {
    (void)state;

    uint8_t source[SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES + 1];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_ERROR_SECRET_TOO_LONG);
}

static void test_deserialize_shard_accepts_minimum_length(void** state) {
    (void)state;

    uint8_t source[SSKR_MIN_SERIALIZED_LENGTH_BYTES];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_MIN_STRENGTH_BYTES);
    assert_int_equal(shard.value_len, SSKR_MIN_STRENGTH_BYTES);
    assert_memory_equal(shard.value, source + SSKR_METADATA_LENGTH_BYTES,
                        SSKR_MIN_STRENGTH_BYTES);
}

static void test_deserialize_shard_rejects_one_below_minimum(void** state) {
    (void)state;

    uint8_t source[SSKR_MIN_SERIALIZED_LENGTH_BYTES - 1];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_ERROR_NOT_ENOUGH_SERIALIZED_BYTES);
}

/* The third code sskr_check_secret_length() can produce, so that moving the
 * call is shown not to have lost any of them. */
static void test_deserialize_shard_rejects_odd_value_length(void** state) {
    (void)state;

    uint8_t source[SSKR_MIN_SERIALIZED_LENGTH_BYTES + 1];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_ERROR_SECRET_LENGTH_NOT_EVEN);
}

/* 60 bytes: the value the overrun was measured at, 55 bytes into a 32-byte
 * field. Sentinels rather than a sanitizer, so this holds even in a build
 * without one -- and the value field itself is checked too, since the fix
 * means nothing at all is written on the refused path. */
#define OVERLONG_SOURCE_LEN (60)

static void test_deserialize_shard_overlong_writes_nothing(void** state) {
    (void)state;

    uint8_t source[OVERLONG_SOURCE_LEN];
    uint8_t block[sizeof(sskr_shard_t) + SENTINEL_LEN];
    sskr_shard_t* shard = (sskr_shard_t*)block;

    build_shard(source, sizeof(source));
    memset(block, SENTINEL_BYTE, sizeof(block));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), shard),
                     SSKR_ERROR_SECRET_TOO_LONG);

    /* Nothing landed in the value field. */
    for (size_t i = 0; i < sizeof(shard->value); i++) {
        assert_int_equal(shard->value[i], SENTINEL_BYTE);
    }
    /* Nor past the end of the structure it lives in. */
    for (size_t i = sizeof(sskr_shard_t); i < sizeof(block); i++) {
        assert_int_equal(block[i], SENTINEL_BYTE);
    }
}

/* Same input, destination alone in a heap block sized exactly to the
 * structure, so the write has no neighbour to hide in and AddressSanitizer
 * reports it directly rather than through a surviving sentinel. */
static void test_deserialize_shard_overlong_stays_in_its_allocation(
    void** state) {
    (void)state;

    uint8_t source[OVERLONG_SOURCE_LEN];
    sskr_shard_t* shard = malloc(sizeof(sskr_shard_t));

    assert_non_null(shard);
    build_shard(source, sizeof(source));
    memset(shard, 0, sizeof(sskr_shard_t));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), shard),
                     SSKR_ERROR_SECRET_TOO_LONG);

    free(shard);
}

/* Moving the check must not let it overtake the two that already ran before
 * the copy. Both inputs are overlong as well, so if the length check now came
 * first these would report a length error instead. */
static void test_deserialize_shard_reports_reserved_bits_first(void** state) {
    (void)state;

    uint8_t source[OVERLONG_SOURCE_LEN];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    source[4] = 0x10; /* reserved nibble set */
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_ERROR_INVALID_RESERVED_BITS);
}

static void test_deserialize_shard_reports_group_threshold_first(void** state) {
    (void)state;

    uint8_t source[OVERLONG_SOURCE_LEN];
    sskr_shard_t shard;

    build_shard(source, sizeof(source));
    source[2] = 0x10; /* group threshold 2 of group count 1 */
    memset(&shard, 0, sizeof(shard));

    assert_int_equal(sskr_deserialize_shard(source, sizeof(source), &shard),
                     SSKR_ERROR_INVALID_GROUP_THRESHOLD);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_deserialize_shard_accepts_maximum_length),
        cmocka_unit_test(test_deserialize_shard_rejects_one_past_maximum),
        cmocka_unit_test(test_deserialize_shard_accepts_minimum_length),
        cmocka_unit_test(test_deserialize_shard_rejects_one_below_minimum),
        cmocka_unit_test(test_deserialize_shard_rejects_odd_value_length),
        cmocka_unit_test(test_deserialize_shard_overlong_writes_nothing),
        cmocka_unit_test(
            test_deserialize_shard_overlong_stays_in_its_allocation),
        cmocka_unit_test(test_deserialize_shard_reports_reserved_bits_first),
        cmocka_unit_test(test_deserialize_shard_reports_group_threshold_first),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
