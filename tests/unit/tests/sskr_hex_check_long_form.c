/*
 * Where the group index sits in each of the two CBOR forms a share can be
 * entered in, and that bolos_ux_sskr_hex_check() refuses a set of shares
 * drawn from more than one group in every one of them.
 *
 * That function checks three things about each entered share: the three CBOR
 * tag bytes, the CRC-32, and -- from the second share on -- that the metadata
 * every share of one group carries is the same as the first share's. The
 * third one is what refuses a set built from two different groups, because
 * the group index is one of the bytes the shards of a set do not agree on.
 *
 * How much of the record that metadata spans depends on the CBOR form, and
 * that is what this file pins. A wire record is the tag (3 bytes), a
 * byte-string header, the serialized shard, then the CRC-32; the shard's own
 * byte 3 is the group-index/member-threshold pair. Byte strings shorter than
 * 24 bytes carry their length in the header byte itself, longer ones need one
 * more byte, and that extra byte shifts everything after it:
 *
 *   seed   share_len  byte-string header  index of the group-index byte
 *   128    21         0x55                3 + 4 = 7
 *   192    29         0x58 0x1D           3 + 5 = 8
 *   256    37         0x58 0x25           3 + 5 = 8
 *
 * A comparison of a fixed 8 bytes reaches that byte in the short form only.
 * It did exactly that until the metadata length was derived from the header
 * instead, and a set drawn from two groups of one backup was accepted at 18
 * and 24 words: the bytes such a fixed window does compare -- tag,
 * byte-string length, share identifier, group threshold and group count --
 * are the same across the two groups, so nothing inside it distinguished
 * them.
 *
 * Measured, not deduced: the first test below runs all three forms through
 * the function and asserts the verdict each one actually gives. All three
 * refuse. That assertion is the regression test on the width of the
 * comparison -- narrow it back to a constant 8 and the two long forms turn
 * green-to-red here.
 *
 * The second test keeps the layer below honest. Whatever the entry screen
 * does, sskr_combine_shards() has its own refusals, and they are what stands
 * between a multi-group set and two arrays that hold one element each:
 *
 *   - `next_group >= SSKR_MAX_GROUP_COUNT` -> SSKR_ERROR_INVALID_SHARD_SET,
 *     for shards naming two different groups. SSKR_MAX_GROUP_COUNT is 1 in
 *     this port, so `sskr_group_t groups[SSKR_MAX_GROUP_COUNT]` has one
 *     element and this is what stops a write to groups[1].
 *   - `next_group < group_threshold` -> SSKR_ERROR_NOT_ENOUGH_GROUPS, for a
 *     set whose header asks for two groups when the shards of only one were
 *     entered. gx and gy are SSKR_MAX_GROUP_COUNT long too, and without this
 *     one sss_recover_secret() is called with the entered group threshold of
 *     2 and reads gx[1].
 *
 * The second guard is the one this file demonstrates is load-bearing.
 * Turning it into `else if (0)` and building this target with
 * -fsanitize=address gives, from wire records rather than from a direct
 * library call:
 *
 *     ERROR: AddressSanitizer: stack-buffer-overflow ... READ of size 1
 *     #0 interpolate interpolate.c:168
 *     #1 sss_recover_secret sss.c:181
 *     #2 sskr_combine_shards_internal sskr.c:563
 *     #3 sskr_combine_shards sskr.c:617
 *     #4 bolos_ux_sskr_combine seed_sskr.c:127
 *     #5 test_multi_group_set_also_stops_in_the_library
 *     [48, 49) 'gx' <== Memory access at offset 49 overflows this variable
 *
 * which is why this target is built with the sanitizer. That mutant fails
 * sskr_multi_group_refusal.c as well, which reaches the same guard by calling
 * sskr_combine_shards() directly; what is added here is the frame at #4, the
 * wrapper the device reaches, which turns every failure alike into 0.
 *
 * Nothing here is a live defect and nothing in src/ changes for it. What this
 * records is that both layers refuse, and that the entry screen's refusal
 * does not depend on the seed length -- which is the property that was not
 * true before the metadata length replaced the constant 8.
 *
 * Vectors
 * -------
 * The 128-bit set is the worked example of BCR-2020-011,
 *     https://github.com/BlockchainCommons/Research/blob/master/papers/bcr-2020-011-sskr.md
 * whose published wire form for one of these shards,
 * d99d75554bbf1101025abd490ee65b6084859854ee67736e75, is what authenticates
 * the framing used for all three sets here.
 *
 * The 192-bit and 256-bit sets here are NOT published. Published SSKR vectors
 * do exist for both a 128- and a 256-bit seed, but the only multi-group one
 * among them is the 128-bit worked example above: the 256-bit vector in
 * Blockchain Commons' test vectors, the one tests/unit/tests/sskr.c uses, is a
 * single 2-of-3 group and so cannot exercise any of this.
 * They were generated outside this repository with a GF(2^8) implementation
 * written from the specification (controlled on gmul(0x57,0x83) == 0xC1, and
 * checked by reproducing the published 128-bit example above, master secret
 * included), and each was verified before being written down here: the CRC-32
 * of every record recomputed, the group secret recovered from the two group-0
 * shards with its SLIP-39 share digest checked, and the group-level digest
 * checked against the master secret. Both have group threshold 2, group 0
 * 2-of-3 and group 1 3-of-5, the same shape as the published example.
 *
 *   192-bit master secret fd550f55cb41782192752c300f4b4f3d122948c280cfb9e5,
 *                         identifier 6aa1
 *   256-bit master secret
 *       e3955cda0d8b5e2f9a1c47b6803df2e1a5c9017d4462fb38e0a7c51936d2b8f4,
 *                         identifier 4a7c
 *
 * The `_other_id` records are the group-1 share of each long-form set with
 * one bit of the identifier flipped and the CRC-32 recomputed over it. They
 * are controls, not backups: they carry a byte that differs inside the
 * compared window, which is how the first test tells "the window is not
 * checked in the long form" apart from "the byte it would have caught is no
 * longer in it".
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

#define MAX_WIRE_LEN (46)
#define CRC_LEN (4)

/* BCR-2020-011, 128-bit: shard 21 bytes, short-form header 0x55. */
static const uint8_t k_128_g0m0[29] = {
    0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x00, 0x3E,
    0x99, 0x0C, 0x1F, 0x04, 0x35, 0xE2, 0xB3, 0x3C, 0x72, 0x15,
    0x35, 0xC7, 0x46, 0x03, 0xD0, 0x52, 0x3C, 0xDA, 0x82};

static const uint8_t k_128_g0m1[29] = {
    0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x01, 0x01, 0x0C,
    0x8B, 0xA3, 0x9A, 0x75, 0x02, 0xA3, 0x25, 0xED, 0x07, 0xB8,
    0xD5, 0x97, 0xD1, 0xB8, 0x0F, 0x47, 0xF4, 0x7B, 0x79};

static const uint8_t k_128_g1m0[29] = {
    0xD9, 0x9D, 0x75, 0x55, 0x4B, 0xBF, 0x11, 0x12, 0x00, 0x44,
    0xEF, 0x45, 0x3F, 0x66, 0x92, 0x3D, 0x32, 0x65, 0x3B, 0x37,
    0x7D, 0xE5, 0xC9, 0x4B, 0x39, 0xBE, 0xD9, 0x49, 0x68};

/* 192-bit: shard 29 bytes, long-form header 0x58 0x1D. */
static const uint8_t k_192_g0m0[38] = {
    0xD9, 0x9D, 0x75, 0x58, 0x1D, 0x6A, 0xA1, 0x11, 0x01, 0x00,
    0x0B, 0x2E, 0x54, 0xCB, 0x8A, 0xD4, 0x8A, 0x6D, 0xF3, 0x08,
    0xB9, 0x5B, 0xC6, 0x67, 0x57, 0x55, 0x6A, 0x58, 0xC3, 0x44,
    0x40, 0xB5, 0xA6, 0x4F, 0x47, 0x58, 0x64, 0xB8};

static const uint8_t k_192_g0m1[38] = {
    0xD9, 0x9D, 0x75, 0x58, 0x1D, 0x6A, 0xA1, 0x11, 0x01, 0x01,
    0xBE, 0x14, 0xBA, 0xF4, 0x86, 0x2D, 0x28, 0x0A, 0xE1, 0x00,
    0xAA, 0xA9, 0x3B, 0x02, 0x2A, 0xD0, 0xA2, 0x98, 0x08, 0x36,
    0x77, 0xE1, 0x0E, 0x11, 0x5F, 0x33, 0xBC, 0x84};

static const uint8_t k_192_g1m0[38] = {
    0xD9, 0x9D, 0x75, 0x58, 0x1D, 0x6A, 0xA1, 0x11, 0x12, 0x00,
    0x33, 0xCE, 0x18, 0x2F, 0xBC, 0x46, 0x08, 0x4E, 0xE7, 0x1F,
    0xAC, 0x57, 0x99, 0xF2, 0x43, 0x1F, 0xCD, 0x6D, 0xCB, 0x74,
    0x0D, 0xA7, 0x01, 0x83, 0x7D, 0x80, 0x7C, 0xC5};

static const uint8_t k_192_g1m0_other_id[38] = {
    0xD9, 0x9D, 0x75, 0x58, 0x1D, 0x6A, 0xA0, 0x11, 0x12, 0x00,
    0x33, 0xCE, 0x18, 0x2F, 0xBC, 0x46, 0x08, 0x4E, 0xE7, 0x1F,
    0xAC, 0x57, 0x99, 0xF2, 0x43, 0x1F, 0xCD, 0x6D, 0xCB, 0x74,
    0x0D, 0xA7, 0x01, 0x83, 0x7C, 0x35, 0x81, 0xD8};

/* 256-bit: shard 37 bytes, long-form header 0x58 0x25. */
static const uint8_t k_256_g0m0[46] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x4A, 0x7C, 0x11, 0x01, 0x00, 0x21, 0xC6,
    0xEF, 0x19, 0x29, 0x43, 0x45, 0x3F, 0x66, 0x51, 0x06, 0x08, 0xA9, 0xBC,
    0xA8, 0x9C, 0xB9, 0x11, 0xDC, 0xE6, 0x50, 0xC9, 0xCD, 0x6D, 0xED, 0x90,
    0x33, 0x88, 0x5C, 0x90, 0x97, 0x61, 0xB2, 0xBE, 0xB1, 0x20};

static const uint8_t k_256_g0m1[46] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x4A, 0x7C, 0x11, 0x01, 0x01, 0xC7, 0xCF,
    0xF3, 0x0B, 0xA7, 0x33, 0x1B, 0xB2, 0xB7, 0x3D, 0x80, 0x5B, 0x2C, 0xBA,
    0xC1, 0x2D, 0x61, 0x1B, 0x66, 0xEC, 0xF8, 0xC9, 0xA7, 0x60, 0x22, 0x05,
    0x9B, 0xC5, 0xA5, 0x9F, 0xBF, 0x22, 0x0C, 0x16, 0x94, 0x97};

static const uint8_t k_256_g1m0[46] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x4A, 0x7C, 0x11, 0x12, 0x00, 0xB4, 0x9A,
    0xBD, 0x10, 0xFF, 0xE6, 0xED, 0x38, 0xA2, 0xDA, 0x67, 0x12, 0x97, 0xDC,
    0xAD, 0x35, 0xA5, 0xCF, 0x99, 0x0A, 0x20, 0x11, 0x80, 0xE8, 0x9A, 0xD9,
    0xE5, 0x09, 0xB2, 0x67, 0xBC, 0xC3, 0xBA, 0x6C, 0x29, 0xD9};

static const uint8_t k_256_g1m0_other_id[46] = {
    0xD9, 0x9D, 0x75, 0x58, 0x25, 0x4A, 0x7D, 0x11, 0x12, 0x00, 0xB4, 0x9A,
    0xBD, 0x10, 0xFF, 0xE6, 0xED, 0x38, 0xA2, 0xDA, 0x67, 0x12, 0x97, 0xDC,
    0xAD, 0x35, 0xA5, 0xCF, 0x99, 0x0A, 0x20, 0x11, 0x80, 0xE8, 0x9A, 0xD9,
    0xE5, 0x09, 0xB2, 0x67, 0xBC, 0xC3, 0x2F, 0x1C, 0xFD, 0x4C};

struct wire_form {
    unsigned int secret_len; /* 16, 24 or 32 bytes of seed entropy */
    unsigned int wire_len;
    unsigned int header_len; /* CBOR tag plus byte-string header */
    const uint8_t* g0m0;
    const uint8_t* g0m1;
    const uint8_t* g1m0;
    /* NULL for the short form, where the point it controls does not arise */
    const uint8_t* g1m0_other_id;
};

static const struct wire_form k_forms[] = {
    /* 12 words, short form */
    {16, 29, 4, k_128_g0m0, k_128_g0m1, k_128_g1m0, NULL},
    /* 18 words, long form */
    {24, 38, 5, k_192_g0m0, k_192_g0m1, k_192_g1m0, k_192_g1m0_other_id},
    /* 24 words, long form */
    {32, 46, 5, k_256_g0m0, k_256_g0m1, k_256_g1m0, k_256_g1m0_other_id},
};

#define FORM_COUNT (sizeof(k_forms) / sizeof(k_forms[0]))

/* bolos_ux_sskr_hex_check() wipes what it rejects, and bolos_ux_sskr_combine()
 * wipes what it cannot combine, so every call works on a fresh copy. */
static void load(uint8_t* buffer, const struct wire_form* form,
                 const uint8_t* first, const uint8_t* second) {
    memcpy(buffer, first, form->wire_len);
    if (second != NULL) {
        memcpy(buffer + form->wire_len, second, form->wire_len);
    }
}

static unsigned int hex_check_pair(const struct wire_form* form,
                                   const uint8_t* first,
                                   const uint8_t* second) {
    uint8_t buffer[2 * MAX_WIRE_LEN];
    const unsigned int count = (second == NULL) ? 1u : 2u;

    load(buffer, form, first, second);

    return bolos_ux_sskr_hex_check(buffer, count * form->wire_len, count);
}

/*
 * The whole of the measurement, one row of the table in the header per
 * iteration.
 *
 * The controls come first and are not decoration: this function rejects on a
 * wrong CRC-32 or a wrong tag too, so a set of records with a bad CRC would
 * be refused for a reason that has nothing to do with the group index, and
 * every row below would pass while proving nothing. Each group of each set is
 * accepted on its own, in all three forms, which leaves the metadata
 * comparison as the only check that can be deciding anything here.
 *
 * The `_other_id` control pins the comparison from the other side: a byte
 * that differs inside the compared span is caught in the long form too, so a
 * refusal there cannot be read as the function having stopped comparing.
 */
static void test_hex_check_verdict_by_cbor_form(void** state) {
    (void)state;

    for (size_t i = 0; i < FORM_COUNT; i++) {
        const struct wire_form* form = &k_forms[i];
        /* tag(3) + byte-string header + shard + CRC-32 */
        const unsigned int shard_len =
            form->wire_len - form->header_len - CRC_LEN;
        /* byte 3 of the shard holds group-index and member-threshold */
        const unsigned int group_index_byte = form->header_len + 3;

        assert_int_equal(shard_len,
                         SSKR_METADATA_LENGTH_BYTES + form->secret_len);
        assert_int_equal(form->header_len, (shard_len < 24) ? 4 : 5);
        assert_int_equal(group_index_byte, (shard_len < 24) ? 7 : 8);

        /* the two shares really do come from different groups, and differ
         * nowhere before that byte */
        assert_memory_equal(form->g0m0, form->g1m0, group_index_byte);
        assert_int_equal(form->g0m0[group_index_byte] >> 4, 0);
        assert_int_equal(form->g1m0[group_index_byte] >> 4, 1);

        /* controls: each group, on its own, is well-formed in this form */
        assert_int_equal(hex_check_pair(form, form->g0m0, NULL), 1);
        assert_int_equal(hex_check_pair(form, form->g1m0, NULL), 1);
        assert_int_equal(hex_check_pair(form, form->g0m0, form->g0m1), 1);

        /* the measurement: refused, in every form */
        assert_int_equal(hex_check_pair(form, form->g0m0, form->g1m0), 0);

        /* control: a byte that differs inside the compared window is still
         * caught in this form */
        if (form->g1m0_other_id != NULL) {
            assert_int_equal(hex_check_pair(form, form->g1m0_other_id, NULL),
                             1);
            assert_int_not_equal(memcmp(form->g0m0, form->g1m0_other_id, 8), 0);
            assert_int_equal(
                hex_check_pair(form, form->g0m0, form->g1m0_other_id), 0);
        }
    }
}

/*
 * The layer below the entry screen, kept honest on its own terms. Both cases
 * go through bolos_ux_sskr_combine() -- the entry point the device reaches
 * next -- and both are refused there; the error codes underneath name which
 * of the two guards did it, since that wrapper turns every failure alike
 * into 0.
 *
 * These calls do not run through bolos_ux_sskr_hex_check() first, which is
 * the point: the guards below have to hold whatever the gate above them
 * accepts, and they are what this file mutates to show they are load-bearing.
 * All three forms are exercised, since none of this depends on the gate's
 * verdict.
 */
static void test_multi_group_set_also_stops_in_the_library(void** state) {
    (void)state;

    for (size_t i = 0; i < FORM_COUNT; i++) {
        const struct wire_form* form = &k_forms[i];

        const unsigned int shard_len =
            form->wire_len - form->header_len - CRC_LEN;
        uint8_t buffer[2 * MAX_WIRE_LEN];
        uint8_t output[SSKR_MAX_STRENGTH_BYTES];

        /* shards of two different groups: refused at entry, and refused here
         * too */
        assert_int_equal(hex_check_pair(form, form->g0m0, form->g1m0), 0);

        load(buffer, form, form->g0m0, form->g1m0);
        assert_int_equal(
            bolos_ux_sskr_combine(buffer, 2 * form->wire_len, 2, output), 0);

        const uint8_t* mixed[2] = {form->g0m0 + form->header_len,
                                   form->g1m0 + form->header_len};
        assert_int_equal(sskr_combine_shards(mixed, (uint8_t)shard_len, 2,
                                             output, sizeof(output)),
                         SSKR_ERROR_INVALID_SHARD_SET);

        /* the shards of one group of a two-group set: enough to recover that
         * group's own secret, and the group threshold of 2 read out of the
         * entered shards is what refuses them. Without that guard this is the
         * call that reads gx[1]. */
        assert_int_equal(hex_check_pair(form, form->g0m0, form->g0m1), 1);

        load(buffer, form, form->g0m0, form->g0m1);
        assert_int_equal(
            bolos_ux_sskr_combine(buffer, 2 * form->wire_len, 2, output), 0);

        const uint8_t* one_group[2] = {form->g0m0 + form->header_len,
                                       form->g0m1 + form->header_len};
        assert_int_equal(sskr_combine_shards(one_group, (uint8_t)shard_len, 2,
                                             output, sizeof(output)),
                         SSKR_ERROR_NOT_ENOUGH_GROUPS);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hex_check_verdict_by_cbor_form),
        cmocka_unit_test(test_multi_group_set_also_stops_in_the_library),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
