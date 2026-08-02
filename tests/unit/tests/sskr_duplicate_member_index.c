/*
 * The duplicate member index guard in sskr_combine_shards_internal()
 * (src/common/sskr/sskr.c:492-494, SSKR_ERROR_DUPLICATE_MEMBER_INDEX): two
 * shards of the same group carrying the same member_index must be refused.
 *
 * The interesting part is that it was already covered, already exercised, and
 * still not held. Replacing its condition with `if (0)` left the whole suite
 * green. lcov showed its return line executed, and the test executing it is
 * even named after it: test_combine_reports_duplicate_member_index, in
 * sskr_combine_error.c. That file is a regression test for a different defect
 * -- negative error codes swallowed by a uint16_t -- and uses a duplicate
 * shard set only as a convenient way to make combination fail. It asserts
 * bolos_ux_sskr_combine() == 0, and 0 is the answer whether the guard fires or
 * not. A covered line, named in a test, holding nothing.
 *
 * So what is new here is not that the code path runs. It is that the exact
 * error code is asserted, through sskr_combine_shards() where -10 and -104 are
 * distinguishable, rather than through the wrapper where both become 0.
 *
 * What reaches the guard is the most ordinary mistake there is: the same share
 * entered twice. bolos_ux_sskr_hex_check() does not catch it -- it checks the
 * CBOR tag, that the first eight bytes agree across shares, and the CRC-32,
 * and two copies of one valid share satisfy all three, so it answers 1. The
 * guard in sskr.c is the only thing that distinguishes the case. The tests
 * below assert that hex_check() accepts, deliberately: that is a statement
 * about where the check lives, not a defect, and freezing it stops anyone
 * later reading hex_check() as protecting against duplicates.
 *
 * What the guard is worth, measured by disabling it and re-running:
 *
 *              guard present            guard disabled
 *   hex_check  1                        1
 *   combine    0                        0
 *   shards     -10 (DUPLICATE_MEMBER)   -104 (SSS_ERROR_CHECKSUM_FAILURE)
 *
 * So nothing unsafe gets through either way, and this is coverage on
 * already-correct code rather than a bug fix. Two equal x coordinates make the
 * Lagrange denominator xi[i]^xi[j] zero (sss/interpolate.c), its inverse is
 * computed as denominator^254 and stays zero, every basis coefficient is zero,
 * and the reconstruction comes out all zeros -- which the SLIP-39 digest then
 * refuses. What the guard buys is the difference between a checksum failure
 * and an error naming the actual mistake; the output buffer is left untouched
 * in the first case and zeroed in the second.
 *
 * Shard-set invariants that are not about member_index live in
 * sskr_combine_invariants.c; this file is only the duplicate guard, and it is
 * built entirely from the real bc-sskr share pair rather than synthetic
 * shards.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which the sskr headers use
 * without defining. Not sorted, hence the clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
#include "sskr/sskr.h"
// clang-format on

/* A real 2-of-3 share pair over a 128-bit secret, generated with Blockchain
 * Commons' bc-sskr: 21-byte shards, member_index 0 and 1, genuine CRC-32s --
 * which is what lets bolos_ux_sskr_hex_check() be exercised for real here.
 * Same set as tests/sskr_share_len.c and tests/sskr_interop_bc128_bytewords.c.
 */
#define HEADER_LEN (4)
#define SHARD_LEN (SSKR_MIN_SERIALIZED_LENGTH_BYTES) /* 21 */
#define CRC_LEN (4)
#define WIRE_LEN (HEADER_LEN + SHARD_LEN + CRC_LEN) /* 29 */

/* Byte 4 of the shard is the reserved nibble plus member_index. */
#define MEMBER_INDEX_OFFSET (HEADER_LEN + 4)

static const uint8_t k_shares[2 * WIRE_LEN] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72, 0x79, 0x90,
    0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33, 0xFD, 0xAE, 0x81, 0x59,
    0x13, 0x5E, 0xF3, 0xB3, 0x38, 0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00,
    0x01, 0x01, 0x1B, 0x3F, 0x98, 0x69, 0x92, 0x4E, 0x46, 0x03, 0x14, 0x4D,
    0x4A, 0x40, 0x84, 0xF0, 0x90, 0xCC, 0xAE, 0x60, 0x76, 0x65};

static const uint8_t k_secret[16] = {0x59, 0xF2, 0x29, 0x3A, 0x5B, 0xCE,
                                     0x7D, 0x4D, 0xE5, 0x9E, 0x71, 0xB4,
                                     0x20, 0x7A, 0xC5, 0xD2};

/* CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320, initial and final
 * value 0xFFFFFFFF), written out here rather than calling the cx_crc32() the
 * code under test uses. Sharing that implementation would make the resealed
 * frames agree with hex_check() by construction, which is not the same thing
 * as being correct; test_resealing_reproduces_the_published_crc() below pins
 * this against the CRCs bc-sskr actually published. */
static uint32_t crc32_ieee(const uint8_t* buf, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

/* Recomputes the trailing CRC-32 of one wire frame, in the byte order
 * bolos_ux_sskr_hex_check() compares against: it takes crc32_nbo(), the
 * checksum in network byte order, and memcmps the raw bytes, so the frame
 * carries the big-endian form. Written out byte by byte rather than swapping a
 * uint32_t, so it does not depend on the host's endianness. */
static void reseal(uint8_t* wire) {
    uint32_t crc = crc32_ieee(wire, WIRE_LEN - CRC_LEN);
    uint8_t* out = wire + WIRE_LEN - CRC_LEN;

    out[0] = (uint8_t)(crc >> 24);
    out[1] = (uint8_t)(crc >> 16);
    out[2] = (uint8_t)(crc >> 8);
    out[3] = (uint8_t)crc;
}

/* How the pair is loaded into the caller's buffer. */
typedef enum {
    PAIR_AS_PUBLISHED, /* member_index 0 and 1 */
    PAIR_DUPLICATED,   /* share 0 twice: the same share entered twice */
    PAIR_SAME_INDEX    /* both shares, both relabelled member_index 0 */
} pair_shape_t;

/* Reloaded before every call, because bolos_ux_sskr_combine() memzeroes the
 * buffer it was given on any failure path. */
static void load(uint8_t wire[2 * WIRE_LEN], pair_shape_t shape) {
    memcpy(wire, k_shares, 2 * WIRE_LEN);

    if (shape == PAIR_DUPLICATED) {
        memcpy(&wire[WIRE_LEN], &k_shares[0], WIRE_LEN);
    } else if (shape == PAIR_SAME_INDEX) {
        /* Keep the second shard's own Shamir payload, give it the first
         * shard's member_index, then make the frame well-formed again so the
         * only thing wrong with the pair is the collision itself. */
        wire[WIRE_LEN + MEMBER_INDEX_OFFSET] = 0x00;
        reseal(&wire[WIRE_LEN]);
    }
}

/* Validates the helper the test below depends on: resealing a frame nobody
 * touched must reproduce the CRC-32 bytes bc-sskr published. */
static void test_resealing_reproduces_the_published_crc(void** state) {
    (void)state;

    uint8_t wire[2 * WIRE_LEN];

    memcpy(wire, k_shares, sizeof(wire));
    reseal(&wire[0]);
    reseal(&wire[WIRE_LEN]);

    assert_memory_equal(wire, k_shares, sizeof(wire));
}

/* The whole chain the device runs when one share is entered twice, in the
 * order it runs it. */
static void test_the_same_shard_entered_twice_is_refused(void** state) {
    (void)state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    /* Two copies of one valid share agree on their first eight bytes and each
     * carry a correct CRC-32, so the framing check has nothing to object to. */
    load(wire, PAIR_DUPLICATED);
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 2), 1);

    load(wire, PAIR_DUPLICATED);
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output), 0);

    /* The exact code, not merely a negative one: it is what tells the caller
     * which mistake was made. */
    load(wire, PAIR_DUPLICATED);
    const uint8_t* shards[2] = {&wire[HEADER_LEN],
                                &wire[WIRE_LEN + HEADER_LEN]};
    assert_int_equal(
        sskr_combine_shards(shards, SHARD_LEN, 2, output, sizeof(output)),
        SSKR_ERROR_DUPLICATE_MEMBER_INDEX);
}

/* The case that isolates the guard. The duplicated pair above is two identical
 * byte strings, so a plain memcmp between shares would be enough to catch it.
 * Here the two shards keep their own distinct Shamir payloads and collide only
 * on member_index, which nothing but this guard looks at -- and the frames are
 * resealed, so they are well-formed all the way through hex_check(). */
static void test_distinct_shards_sharing_a_member_index_are_refused(
    void** state) {
    (void)state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    load(wire, PAIR_SAME_INDEX);
    assert_memory_not_equal(&wire[0], &wire[WIRE_LEN], WIRE_LEN);
    assert_int_equal(wire[MEMBER_INDEX_OFFSET],
                     wire[WIRE_LEN + MEMBER_INDEX_OFFSET]);

    /* Nothing in the framing is wrong, so this too is accepted here. */
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 2), 1);

    load(wire, PAIR_SAME_INDEX);
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output), 0);

    load(wire, PAIR_SAME_INDEX);
    const uint8_t* shards[2] = {&wire[HEADER_LEN],
                                &wire[WIRE_LEN + HEADER_LEN]};
    assert_int_equal(
        sskr_combine_shards(shards, SHARD_LEN, 2, output, sizeof(output)),
        SSKR_ERROR_DUPLICATE_MEMBER_INDEX);
}

/* Non-regression: the same pair, left alone, still reconstructs. Without this
 * the tests above would be satisfied by a guard that refused everything. */
static void test_a_set_without_duplicates_still_combines(void** state) {
    (void)state;

    uint8_t wire[2 * WIRE_LEN];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    load(wire, PAIR_AS_PUBLISHED);
    assert_int_equal(bolos_ux_sskr_hex_check(wire, sizeof(wire), 2), 1);

    load(wire, PAIR_AS_PUBLISHED);
    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output),
                     sizeof(k_secret));
    assert_memory_equal(output, k_secret, sizeof(k_secret));

    load(wire, PAIR_AS_PUBLISHED);
    const uint8_t* shards[2] = {&wire[HEADER_LEN],
                                &wire[WIRE_LEN + HEADER_LEN]};
    assert_int_equal(
        sskr_combine_shards(shards, SHARD_LEN, 2, output, sizeof(output)),
        (int)sizeof(k_secret));
    assert_memory_equal(output, k_secret, sizeof(k_secret));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_resealing_reproduces_the_published_crc),
        cmocka_unit_test(test_the_same_shard_entered_twice_is_refused),
        cmocka_unit_test(
            test_distinct_shards_sharing_a_member_index_are_refused),
        cmocka_unit_test(test_a_set_without_duplicates_still_combines),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
