/*
 * bolos_ux_sskr_combine() against the length of the buffer it was handed.
 *
 * sskr_combine_bounds.c holds the other bound of the same function, on the
 * share count. This one is about the buffer that count divides.
 *
 * The function reads byte 3 of the frame, and byte 4 when the CBOR header is
 * the long form, before anything has said the frame is that long. It then
 * hands sskr_combine_shards() one pointer per share and a shard length taken
 * out of those bytes, and that function reads the whole shard from each
 * pointer. Neither of the bounds already there says anything about how much
 * buffer exists: the share-count bound constrains the divisor of the stride,
 * and the shard-length bound says the declared length is one a shard may have,
 * not one this frame can hold.
 *
 * bolos_ux_sskr_hex_check() bounds the same quotient for the same reason and
 * says so where it does it -- "Reading byte 3 is in bounds because of the
 * stride bound" -- but that guard was never carried over to this function.
 *
 * Nothing in the application reaches it: both entry paths call hex_check()
 * first, and its bounds are stricter. That is why this file calls
 * bolos_ux_sskr_combine() directly, the same way sskr_hex_check_guards.c calls
 * hex_check() directly to hold a bound its own callers make unreachable. A
 * guard nothing can currently violate is still one the next caller relies on.
 *
 * Every frame below is allocated at exactly its own length rather than inside
 * a padded array, so that a read one byte past it is a heap-buffer-overflow
 * AddressSanitizer reports rather than a read of adjacent padding it cannot
 * see. Run against the function before these bounds existed, the two
 * truncated-header cases abort under ASan instead of merely answering wrongly.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * sskr/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "sskr/common_sskr.h"
#include "sskr/sskr-constants.h"
// clang-format on

/* The same 2-of-3 share set over a 128-bit secret that sskr_share_len.c uses,
 * generated with Blockchain Commons' bc-sskr: 21-byte shards, so the short
 * CBOR header, plus four bytes of CRC-32 each. The control at the end of this
 * file combines it, so that the bounds added here are shown to refuse
 * malformed frames without refusing well-formed ones. */
#define SHORT_WIRE (4 + 21 + 4)

static const uint8_t k_valid_shares[2 * SHORT_WIRE] = {
    0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00, 0x01, 0x00, 0x72, 0x79, 0x90,
    0x3D, 0xCB, 0x83, 0x3F, 0x4B, 0xF7, 0xD4, 0x33, 0xFD, 0xAE, 0x81, 0x59,
    0x13, 0x5E, 0xF3, 0xB3, 0x38, 0xD9, 0x9D, 0x75, 0x55, 0x01, 0x00, 0x00,
    0x01, 0x01, 0x1B, 0x3F, 0x98, 0x69, 0x92, 0x4E, 0x46, 0x03, 0x14, 0x4D,
    0x4A, 0x40, 0x84, 0xF0, 0x90, 0xCC, 0xAE, 0x60, 0x76, 0x65};

static const uint8_t k_secret[16] = {0x59, 0xF2, 0x29, 0x3A, 0x5B, 0xCE,
                                     0x7D, 0x4D, 0xE5, 0x9E, 0x71, 0xB4,
                                     0x20, 0x7A, 0xC5, 0xD2};

/* Calls bolos_ux_sskr_combine() on a buffer owning exactly `length` bytes. */
static unsigned int combine_exactly(const uint8_t *frame, size_t length,
                                    unsigned int share_count) {
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};
    /* malloc(0) may return a pointer that owns nothing, which is exactly what
     * the zero-length case has to hand over. */
    uint8_t *wire = malloc(length ? length : 1);
    assert_non_null(wire);
    memcpy(wire, frame, length);

    unsigned int result =
        bolos_ux_sskr_combine(wire, (unsigned int) length, share_count, output);

    free(wire);
    return result;
}

static void test_combine_refuses_a_frame_shorter_than_its_header(void **state) {
    (void) state;

    /* Every truncation of what is otherwise the start of a real share. Below
     * length 4 byte 3 does not exist; at exactly 4 it does and byte 4 does
     * not. */
    for (size_t length = 0; length <= 4; length++) {
        assert_int_equal(combine_exactly(k_valid_shares, length, 1), 0);
    }
}

static void test_combine_refuses_a_long_form_length_it_cannot_read(
    void **state) {
    (void) state;

    /* CBOR additional information 24, "one length byte follows". That byte is
     * byte 4, and this frame ends at byte 3. The shard-length bound cannot
     * prevent this read: it is applied to the value read, which is too late. */
    const uint8_t truncated_long_form[4] = {0xD9, 0x9D, 0x75, 0x58};

    assert_int_equal(
        combine_exactly(truncated_long_form, sizeof(truncated_long_form), 1),
        0);
}

static void test_combine_refuses_a_shard_longer_than_its_frame(void **state) {
    (void) state;

    /* A well-formed header declaring the shortest shard SSKR allows, 21 bytes,
     * in a frame with nowhere near that much room. The shard-length bound
     * accepts 21 -- it is SSKR_MIN_SERIALIZED_LENGTH_BYTES exactly -- so
     * without a bound tying the declared length to the frame,
     * sskr_combine_shards() is handed a pointer at offset 4 and told to read
     * 21 bytes out of a buffer holding ten. This is the wider of the two
     * overreads: eleven bytes past the end rather than one. */
    const uint8_t short_frame[10] = {0xD9, 0x9D, 0x75, 0x55, 0x01,
                                     0x00, 0x00, 0x01, 0x00, 0x72};

    assert_int_equal(combine_exactly(short_frame, sizeof(short_frame), 1), 0);
}

static void test_combine_refuses_reserved_cbor_additional_info(void **state) {
    (void) state;

    /* 25 to 31 are the two-, four- and eight-byte lengths, the reserved value
     * and the indefinite form. None is a length this function can act on, and
     * none may appear in a share; hex_check() already refuses them. Treated as
     * merely "greater than 23", the length would be taken from byte 4, which
     * is not where a two-byte length lives.
     *
     * The frame is long enough that nothing is overread either way, so this
     * asserts the refusal itself rather than a bound. */
    uint8_t reserved_form[SHORT_WIRE];
    memcpy(reserved_form, k_valid_shares, sizeof(reserved_form));
    reserved_form[3] = 0x59;

    assert_int_equal(combine_exactly(reserved_form, sizeof(reserved_form), 1),
                     0);
}

static void test_combine_still_accepts_a_well_formed_set(void **state) {
    (void) state;

    /* The control. Bounds that refused everything would satisfy every
     * assertion above. */
    uint8_t wire[sizeof(k_valid_shares)];
    uint8_t output[SSKR_MAX_STRENGTH_BYTES] = {0};

    memcpy(wire, k_valid_shares, sizeof(wire));

    assert_int_equal(bolos_ux_sskr_combine(wire, sizeof(wire), 2, output),
                     sizeof(k_secret));
    assert_memory_equal(output, k_secret, sizeof(k_secret));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_combine_refuses_a_frame_shorter_than_its_header),
        cmocka_unit_test(
            test_combine_refuses_a_long_form_length_it_cannot_read),
        cmocka_unit_test(test_combine_refuses_a_shard_longer_than_its_frame),
        cmocka_unit_test(test_combine_refuses_reserved_cbor_additional_info),
        cmocka_unit_test(test_combine_still_accepts_a_well_formed_set),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
