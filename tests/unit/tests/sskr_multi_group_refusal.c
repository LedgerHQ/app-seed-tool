/*
 * The two places this port refuses a multi-group shard set, and the entry
 * point a user reaches before either of them.
 *
 * SSKR_MAX_GROUP_COUNT is 1 here (sskr-constants.h), where upstream bc-sskr
 * and BCR-2020-011 both allow 16, and sskr_combine_shards_internal()
 * declares its working array on the stack accordingly:
 *
 *     sskr_group_t groups[SSKR_MAX_GROUP_COUNT];
 *
 * Two refusals stand between that one-element array and a shard set that
 * names more than one group:
 *
 *   - sskr.c:507, `next_group >= SSKR_MAX_GROUP_COUNT` -> -8. This is the
 *     only thing that stops the ten lines below it from filling
 *     groups[next_group] with next_group == 1. Replacing it with `if (0)`
 *     and building the suite with -fsanitize=address aborts in
 *     sskr_combine_invariants.c with a stack-buffer-overflow at sskr.c:512,
 *     so that one is already held. What this file adds for it is thin, and
 *     worth naming as such: sskr_combine_invariants.c goes through
 *     bolos_ux_sskr_combine(), whose unsigned return collapses the 25 codes
 *     reachable there to 0, so the -8 is read here at the library boundary
 *     instead. sskr_shard_count.c does already read -8 out of
 *     sskr_combine_shards(), but for the shards_count bound at sskr.c:599,
 *     not for this guard.
 *   - sskr.c:524, `next_group < group_threshold` -> -14, for a set whose
 *     header asks for two groups when only one was supplied. A gcov run over
 *     the whole suite gives this one an execution count of zero: nothing
 *     reaches it. It is a bound too, not just a diagnosis: gx and gy are
 *     also SSKR_MAX_GROUP_COUNT long, and without it sss_recover_secret() is
 *     called with the header's group threshold of 2 and reads gx[1]. Turning
 *     it into `else if (0)` and running this file under
 *     -fsanitize=address gives
 *
 *         stack-buffer-overflow ... READ of size 1
 *         #0 interpolate interpolate.c:168
 *         #1 sss_recover_secret sss.c:181
 *         #2 sskr_combine_shards_internal sskr.c:563
 *         [48, 49) 'gx' (line 530) <== Memory access at offset 49 overflows
 *
 *     which is why this target is built with the sanitizer.
 *
 * Nothing here is a live defect: both refusals are present and correct. The
 * input that reaches them is ordinary user input -- shares of a multi-group
 * SSKR backup, produced by another tool and typed in on the device -- not a
 * contrived case, which is why the third test below drives the entry point
 * the device actually runs first rather than the library underneath it.
 *
 * The vector is the worked example of BCR-2020-011 itself:
 *     https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md
 * group 0 is 2-of-3, group 1 is 3-of-5, group threshold 2, master secret
 * 7daa851251002874e1a1995f0897e6b1. The 8 serialized shards are published
 * there; each 5-byte header decodes as the document annotates it, byte 2 =
 * 0x11 meaning group-threshold-1 = 1 and group-count-1 = 1. Reconstructing
 * that secret from them was checked outside this repository with a GF(2^8)
 * implementation written from the specification -- group 0 yields
 * 6015c954ff45a28b7058353d11817e11, group 1 yields
 * 3755185d7868f88a35118ae93612c73e, the two together yield the published
 * master secret, with valid SLIP-39 share digests at both levels. Nothing
 * was recomputed with this port, which would have been circular.
 *
 * The wire frames further down add the CBOR tag 40309, the byte-string
 * length header and the CRC-32 the entry screens produce. Their prefix is
 * the specification's own published wire form for the third shard,
 * d99d75554bbf1101025abd490ee65b6084859854ee67736e75, which is what
 * authenticates them.
 *
 * That published set is a 128-bit secret, so its shards are 21 bytes and its
 * byte-string header is the short CBOR form, one byte. It is only one of the
 * three seed sizes this application supports, and the size does not merely
 * lengthen the frame: at 192 and 256 bits the shard is 29 and 37 bytes, the
 * header takes the long form, 0x58 followed by a length byte, and every field
 * after it moves along by one. group-index sits at byte 7 in the short form
 * and at byte 8 in the long one.
 *
 * The last two tests cover those two sizes, at both refusals. There is no
 * published multi-group vector at either size, so their frames are built
 * here, and what they are built from is stated rather than implied: the
 * identifier, group threshold and group count of the published set, a
 * group-index that differs between the two frames, and filler for the value.
 * Filler is enough because neither refusal interpolates anything -- both
 * decide on metadata, one on the bytes every share of a group must share and
 * the other on the group index itself, before a single share byte is used.
 * The 128-bit case keeps the published shards precisely because it can.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "sskr.h"
#include "sskr-constants.h"
#include "sskr/common_sskr.h"
// clang-format on

#define SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + 16)
#define WIRE_LEN (29)  /* tag(3) + length byte(1) + shard(21) + CRC-32(4) */
#define GROUP_SIZE (3) /* how many shards of each group are used here */

/* BCR-2020-011 "Example/Test Vector", serialized shards, verbatim.
 * Header bytes: 4b bf | 11 | gi mt-1 | mi. */
static const uint8_t k_group0[GROUP_SIZE][SHARD_LEN] = {
    {0x4B, 0xBF, 0x11, 0x01, 0x00, 0x3E, 0x99, 0x0C, 0x1F, 0x04, 0x35,
     0xE2, 0xB3, 0x3C, 0x72, 0x15, 0x35, 0xC7, 0x46, 0x03, 0xD0},
    {0x4B, 0xBF, 0x11, 0x01, 0x01, 0x0C, 0x8B, 0xA3, 0x9A, 0x75, 0x02,
     0xA3, 0x25, 0xED, 0x07, 0xB8, 0xD5, 0x97, 0xD1, 0xB8, 0x0F},
    {0x4B, 0xBF, 0x11, 0x01, 0x02, 0x5A, 0xBD, 0x49, 0x0E, 0xE6, 0x5B,
     0x60, 0x84, 0x85, 0x98, 0x54, 0xEE, 0x67, 0x73, 0x6E, 0x75},
};

static const uint8_t k_group1[GROUP_SIZE][SHARD_LEN] = {
    {0x4B, 0xBF, 0x11, 0x12, 0x00, 0x44, 0xEF, 0x45, 0x3F, 0x66, 0x92,
     0x3D, 0x32, 0x65, 0x3B, 0x37, 0x7D, 0xE5, 0xC9, 0x4B, 0x39},
    {0x4B, 0xBF, 0x11, 0x12, 0x01, 0x6F, 0xFB, 0x1B, 0x0C, 0xC5, 0xAB,
     0x48, 0x5F, 0x5A, 0x67, 0x13, 0x6C, 0x80, 0x2B, 0xC6, 0x7B},
    {0x4B, 0xBF, 0x11, 0x12, 0x02, 0xA3, 0x76, 0x31, 0x55, 0xFC, 0xFD,
     0xB5, 0x88, 0x7A, 0xBC, 0xE6, 0xEE, 0x69, 0xC4, 0xBB, 0xCD},
};

/* The same shards in the form the share entry screens hand to
 * bolos_ux_sskr_hex_check(): CBOR tag D9 9D 75, short-form byte-string
 * header 0x55 (21 bytes), the shard, then the CRC-32 in network byte order
 * over the 25 preceding bytes. */
static const uint8_t k_group0_wire[GROUP_SIZE][WIRE_LEN] = {
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x00, 0x3E,
     0x99, 0x0C, 0x1F, 0x04, 0x35, 0xE2, 0xB3, 0x3C, 0x72, 0x15,
     0x35, 0xC7, 0x46, 0x03, 0xD0, 0x52, 0x3C, 0xDA, 0x82},
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x01, 0x0C,
     0x8B, 0xA3, 0x9A, 0x75, 0x02, 0xA3, 0x25, 0xED, 0x07, 0xB8,
     0xD5, 0x97, 0xD1, 0xB8, 0x0F, 0x47, 0xF4, 0x7B, 0x79},
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x02, 0x5A,
     0xBD, 0x49, 0x0E, 0xE6, 0x5B, 0x60, 0x84, 0x85, 0x98, 0x54,
     0xEE, 0x67, 0x73, 0x6E, 0x75, 0x86, 0x82, 0xBD, 0xB2},
};

static const uint8_t k_group1_wire[GROUP_SIZE][WIRE_LEN] = {
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x12, 0x00, 0x44,
     0xEF, 0x45, 0x3F, 0x66, 0x92, 0x3D, 0x32, 0x65, 0x3B, 0x37,
     0x7D, 0xE5, 0xC9, 0x4B, 0x39, 0xBE, 0xD9, 0x49, 0x68},
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x12, 0x01, 0x6F,
     0xFB, 0x1B, 0x0C, 0xC5, 0xAB, 0x48, 0x5F, 0x5A, 0x67, 0x13,
     0x6C, 0x80, 0x2B, 0xC6, 0x7B, 0xC4, 0xE1, 0xF2, 0x20},
    {0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x12, 0x02, 0xA3,
     0x76, 0x31, 0x55, 0xFC, 0xFD, 0xB5, 0x88, 0x7A, 0xBC, 0xE6,
     0xEE, 0x69, 0xC4, 0xBB, 0xCD, 0x82, 0xD1, 0x9A, 0xB8},
};

/* The two seed sizes whose shards need the long-form CBOR byte-string
 * header, i.e. every shard of 24 bytes or more. */
#define VALUE_LEN_192 (24)
#define VALUE_LEN_256 (32)
#define MAX_SHARD_LEN (SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES)
#define MAX_WIRE_LEN  (5 + MAX_SHARD_LEN + 4) /* long header + shard + CRC */

/* CRC-32 (IEEE 802.3: reflected, polynomial 0xEDB88320, initial and final
 * value 0xFFFFFFFF), written out rather than calling the cx_crc32() the code
 * under test uses: sharing that implementation would make the built frames
 * agree with hex_check() by construction. It is checked against the published
 * CRCs of k_group0_wire below rather than trusted. */
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

/* One serialized shard: identifier, group-threshold/group-count,
 * group-index/member-threshold, member-index, then the value. Everything but
 * group-index and the value is taken from the published set: identifier
 * 0x4BBF, and 0x11 for a group threshold and count of 2 each. */
static void build_shard(uint8_t* shard, uint8_t value_len,
                        uint8_t group_index) {
    shard[0] = 0x4B;
    shard[1] = 0xBF;
    shard[2] = 0x11;
    shard[3] = (uint8_t)((group_index & 0x0F) << 4 | 0x01); /* mt - 1 = 1 */
    shard[4] = 0x00;                                        /* member index */
    for (uint8_t i = 0; i < value_len; i++) {
        shard[SSKR_METADATA_LENGTH_BYTES + i] =
            (uint8_t)(0x30 + group_index * 0x40 + i);
    }
}

/* The same shard in the form the entry screens hand to
 * bolos_ux_sskr_hex_check(). Returns the wire length. */
static unsigned int build_wire(uint8_t* wire, uint8_t value_len,
                               uint8_t group_index) {
    const uint8_t shard_len =
        (uint8_t)(SSKR_METADATA_LENGTH_BYTES + value_len);
    unsigned int n = 0;

    wire[n++] = 0xD9;
    wire[n++] = 0x9D;
    wire[n++] = 0x75;
    if (shard_len < 24) {
        wire[n++] = (uint8_t)(0x40 | shard_len);
    } else {
        wire[n++] = 0x58;
        wire[n++] = shard_len;
    }
    build_shard(&wire[n], value_len, group_index);
    n += shard_len;

    const uint32_t crc = crc32_ieee(wire, n);
    wire[n++] = (uint8_t)(crc >> 24);
    wire[n++] = (uint8_t)(crc >> 16);
    wire[n++] = (uint8_t)(crc >> 8);
    wire[n++] = (uint8_t)crc;

    return n;
}

/*
 * The refusal at sskr.c:507, read at the library boundary rather than
 * through bolos_ux_sskr_combine(). Both shards carry the same identifier,
 * group threshold, group count and value length, so the common-metadata
 * check above it passes and they are genuinely sorted into groups; they
 * differ only in group_index, which is exactly the case a one-element
 * groups[] cannot hold. Without the guard this is a write to groups[1].
 *
 * The lightest of the three tests here: sskr_combine_invariants.c already
 * reaches this branch, and only sees the 0 that bolos_ux_sskr_combine()
 * turns every failure into. -8 rather than, say, -10 is what says the set
 * names more than one group.
 */
static void test_combine_refuses_shards_from_two_groups(void** state) {
    (void)state;

    const uint8_t* shards[] = {k_group0[0], k_group1[0]};
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    const int16_t result =
        sskr_combine_shards(shards, SHARD_LEN, 2, output, sizeof(output));

    assert_int_equal(result, SSKR_ERROR_INVALID_SHARD_SET);
}

/*
 * The refusal at sskr.c:524, the one nothing reached. Here the member
 * threshold of group 0 is satisfied -- two shards of a 2-of-3 group, enough
 * to recover that group's own secret -- but the header of both says two
 * groups are required, and only one was supplied. The distinct code matters:
 * -14 says "bring shares from the other group", where -11 (not enough member
 * shards) or -8 would send the user looking for the wrong thing. What is
 * behind it matters more: the group threshold this compares against is read
 * out of the entered shards, and everything the recombination indexes by it
 * is one element long.
 */
static void test_combine_refuses_a_single_group_of_a_two_group_set(
    void** state) {
    (void)state;

    const uint8_t* shards[] = {k_group0[0], k_group0[1]};
    uint8_t output[SSKR_MAX_STRENGTH_BYTES];

    const int16_t result =
        sskr_combine_shards(shards, SHARD_LEN, 2, output, sizeof(output));

    assert_int_equal(result, SSKR_ERROR_NOT_ENOUGH_GROUPS);
}

/* bolos_ux_sskr_hex_check() wipes what it rejects, so every call works on a
 * fresh copy. */
static unsigned int hex_check_copy(const uint8_t* frames,
                                   unsigned int share_count) {
    uint8_t buffer[GROUP_SIZE * 2 * WIRE_LEN];
    const unsigned int length = share_count * WIRE_LEN;

    assert_true(length <= sizeof(buffer));
    memcpy(buffer, frames, length);

    return bolos_ux_sskr_hex_check(buffer, length, share_count);
}

/*
 * The path the user actually reaches: the shares are typed in, and
 * bolos_ux_sskr_hex_check() runs before sskr_combine_shards() ever sees
 * them. Its "the first 8 bytes of every share must match" test is what
 * refuses this set, because for a 128-bit secret the byte carrying the group
 * index falls inside those 8 -- D9 9D 75 55 4B BF 11 01 against
 * ... 11 12, differing at byte 7.
 *
 * That is a property of this vector's wire form, not a general property of
 * the function, and the difference is the whole of tests/unit/tests/
 * sskr_hex_check_long_form.c: a 21-byte shard takes a one-byte CBOR
 * byte-string header (0x55) and puts the group index at byte 7, while the
 * 29- and 37-byte shards of an 18- or 24-word seed take a two-byte one
 * (0x58 0x1D, 0x58 0x25) and push it to byte 8, out of the compared window.
 * The assertion below holds for what it is given; it does not generalize to
 * the other two seed sizes, where the same mixed-group set is accepted here
 * and refused later, by sskr_combine_shards().
 *
 * The three controls come first and are not decoration: this function
 * rejects on a bad CRC-32 too, and a wrong CRC in the frames above would
 * make the rejection below pass for the wrong reason. Each group on its own
 * is accepted, so the CRCs are right and the tag is right, and the only
 * thing left to reject the mixed set is the byte comparison.
 */
static void test_hex_check_refuses_two_groups_in_the_short_form(void** state) {
    (void)state;

    /* controls: each group, on its own, is well-formed */
    assert_int_equal(hex_check_copy(&k_group0_wire[0][0], GROUP_SIZE), 1);
    assert_int_equal(hex_check_copy(&k_group1_wire[0][0], GROUP_SIZE), 1);
    assert_int_equal(hex_check_copy(&k_group0_wire[0][0], 1), 1);
    assert_int_equal(hex_check_copy(&k_group1_wire[0][0], 1), 1);

    /* the frames differ for the first time at byte 7 -- the group index of a
     * 21-byte shard -- inside the 8 bytes compared, and nowhere earlier */
    assert_memory_equal(k_group0_wire[0], k_group1_wire[0], 7);
    assert_int_not_equal(k_group0_wire[0][7], k_group1_wire[0][7]);

    uint8_t mixed[2 * WIRE_LEN];
    memcpy(&mixed[0], k_group0_wire[0], WIRE_LEN);
    memcpy(&mixed[WIRE_LEN], k_group1_wire[0], WIRE_LEN);

    assert_int_equal(hex_check_copy(mixed, 2), 0);
}

/* The built frames are only worth anything if the CRC-32 written here is the
 * one bolos_ux_sskr_hex_check() recomputes. Pinned against the published wire
 * form of the third shard of group 0 rather than against cx_crc32(). */
static void test_crc_helper_reproduces_the_published_crc(void** state) {
    (void)state;

    const uint32_t crc = crc32_ieee(k_group0_wire[2], WIRE_LEN - 4);
    const uint8_t expected[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16),
                                 (uint8_t)(crc >> 8), (uint8_t)crc};

    assert_memory_equal(&k_group0_wire[2][WIRE_LEN - 4], expected, 4);
}

/*
 * The same refusal as test_hex_check_refuses_shares_from_two_groups(), at the
 * two seed sizes whose byte-string header takes the long form. There the
 * shard is 29 or 37 bytes, the header is 0x58 plus a length byte, and
 * group-index lands at byte 8 rather than byte 7 -- outside a metadata
 * comparison whose length is written as a constant 8, and inside one derived
 * from the header.
 *
 * The two controls are not decoration: this function refuses a bad CRC-32
 * too, so without them a refusal here could come from a frame that is simply
 * malformed, and the fix would look right for the wrong reason. Each group on
 * its own has to be accepted first.
 */
static void test_hex_check_refuses_two_groups_at_the_long_header_sizes(
    void** state) {
    (void)state;

    const uint8_t value_lens[] = {VALUE_LEN_192, VALUE_LEN_256};

    for (unsigned int v = 0; v < sizeof(value_lens); v++) {
        uint8_t group0[MAX_WIRE_LEN];
        uint8_t group1[MAX_WIRE_LEN];
        uint8_t scratch[2 * MAX_WIRE_LEN];

        const unsigned int wire_len = build_wire(group0, value_lens[v], 0);
        (void)build_wire(group1, value_lens[v], 1);

        /* the long form, and group-index one byte past where the short form
         * puts it */
        assert_int_equal(group0[3], 0x58);
        assert_int_equal(group0[8] >> 4, 0);
        assert_int_equal(group1[8] >> 4, 1);
        assert_memory_equal(group0, group1, 8);

        memcpy(scratch, group0, wire_len);
        assert_int_equal(bolos_ux_sskr_hex_check(scratch, wire_len, 1), 1);
        memcpy(scratch, group1, wire_len);
        assert_int_equal(bolos_ux_sskr_hex_check(scratch, wire_len, 1), 1);

        memcpy(scratch, group0, wire_len);
        memcpy(scratch + wire_len, group1, wire_len);
        assert_int_equal(bolos_ux_sskr_hex_check(scratch, 2 * wire_len, 2), 0);
    }
}

/*
 * The guard at sskr.c:507 at those same two sizes. Until the metadata
 * comparison above covered group-index there, this was the only thing between
 * a two-group set typed on the device and a write to groups[1] of a
 * one-element array -- and no test reached it at either size, only at 128
 * bits. Both layers have to hold on their own, so it is read here at the
 * library boundary whether or not the layer above it lets anything through.
 */
static void test_combine_refuses_two_groups_at_the_long_header_sizes(
    void** state) {
    (void)state;

    const uint8_t value_lens[] = {VALUE_LEN_192, VALUE_LEN_256};

    for (unsigned int v = 0; v < sizeof(value_lens); v++) {
        uint8_t shard0[MAX_SHARD_LEN];
        uint8_t shard1[MAX_SHARD_LEN];
        uint8_t output[SSKR_MAX_STRENGTH_BYTES];

        build_shard(shard0, value_lens[v], 0);
        build_shard(shard1, value_lens[v], 1);

        const uint8_t* shards[] = {shard0, shard1};
        const int16_t result = sskr_combine_shards(
            shards, (uint8_t)(SSKR_METADATA_LENGTH_BYTES + value_lens[v]), 2,
            output, sizeof(output));

        assert_int_equal(result, SSKR_ERROR_INVALID_SHARD_SET);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_refuses_shards_from_two_groups),
        cmocka_unit_test(
            test_combine_refuses_a_single_group_of_a_two_group_set),
        cmocka_unit_test(test_hex_check_refuses_shares_from_two_groups),
        cmocka_unit_test(test_crc_helper_reproduces_the_published_crc),
        cmocka_unit_test(
            test_hex_check_refuses_two_groups_at_the_long_header_sizes),
        cmocka_unit_test(
            test_combine_refuses_two_groups_at_the_long_header_sizes),
        cmocka_unit_test(test_hex_check_refuses_two_groups_in_the_short_form),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
