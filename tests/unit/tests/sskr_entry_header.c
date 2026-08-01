/*
 * bolos_ux_sskr_entry_header_update(): what a share's CBOR header says about
 * that share, read one entered byte at a time.
 *
 * The three word entry paths -- src/nbgl/sskr_shares.c,
 * src/bagl/nanox_enter_phrase.c and src/bagl/nanos_enter_phrase.c -- each held
 * a copy of this switch, on three different pieces of storage. Only the first
 * is compiled by any test target, and the third (Nano S) is not even built by
 * the CI matrix, so a defect in the arithmetic had two places to sit unseen:
 * the reserved additional-info values leaving the share count at zero was
 * exactly that, present in all three at once.
 *
 * These tests hold the extracted function to the behaviour those three copies
 * had, position by position. Per RFC 8949 the 4th byte of the share is a
 * byte-string header: major type 2, plus a 5-bit additional-info field
 * (buffer[3] & 0x1F) meaning
 *
 *   0-23    literal length, 0 to 23 bytes                  (short form)
 *   24      one length byte follows (0x58)                 (long form)
 *   25-31   two/four/eight-byte length, reserved, or indefinite length
 *
 * and only the first two forms have a length this code can compute. What the
 * function leaves *unwritten* matters as much as what it writes: the share
 * count is deliberately not established for a reserved form, which is what
 * makes the completed share fail bolos_ux_sskr_hex_check() downstream.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "testutils.h"
#include "sskr/common_sskr.h"

/* Values no call below can legitimately produce, so "left alone" is
 * distinguishable from "written". */
#define FINAL_SIZE_UNTOUCHED ((size_t) 0xA5A5A5)
#define COUNT_UNTOUCHED      ((uint8_t) 0xA5)

/* Where each quantity is read from, and where the header itself sits. */
#define HEADER_POSITION      3
#define LONG_LENGTH_POSITION 4
#define COUNT_POSITION_SHORT 7
#define COUNT_POSITION_LONG  8

/* A buffer long enough to hold two shares' worth of positions. */
#define BUFFER_LENGTH 64

/* CBOR tag 309 (crypto-sskr), the three bytes every share starts with. */
static void put_tag(uint8_t *buffer)
{
    buffer[0] = 0xD9;
    buffer[1] = 0x9D;
    buffer[2] = 0x75;
}

/* Byte-string header with `additional_info` in its low five bits. */
static uint8_t byte_string_header(uint8_t additional_info)
{
    return (uint8_t) (0x40 | (additional_info & 0x1F));
}

/*
 * Short form: the additional-info field is the payload length itself, and the
 * share is that many bytes plus the 4-byte CBOR header and the 4-byte CRC32.
 * Nothing else is established at this position -- in particular not the share
 * count, which comes later out of the shard's own metadata.
 */
static void test_short_form_length_is_read_from_the_header(void **state)
{
    (void) state;

    for (uint8_t additional_info = 0; additional_info <= 23; additional_info++) {
        uint8_t buffer[BUFFER_LENGTH] = {0};
        put_tag(buffer);
        buffer[HEADER_POSITION] = byte_string_header(additional_info);

        size_t final_size = FINAL_SIZE_UNTOUCHED;
        uint8_t count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(buffer, HEADER_POSITION, HEADER_POSITION,
                                          &final_size, &count);

        assert_int_equal(final_size, 4 + additional_info + 4);
        assert_int_equal(count, COUNT_UNTOUCHED);
    }
}

/*
 * Long form: additional-info 24 says one length byte follows, so the length
 * taken at the header position is provisional and the byte after it carries
 * the real one. The share is that many bytes plus the 5-byte header and the
 * 4-byte CRC32.
 */
static void test_long_form_length_is_read_from_the_byte_after_the_header(void **state)
{
    (void) state;

    uint8_t buffer[BUFFER_LENGTH] = {0};
    put_tag(buffer);
    buffer[HEADER_POSITION] = byte_string_header(24);

    size_t final_size = FINAL_SIZE_UNTOUCHED;
    uint8_t count = COUNT_UNTOUCHED;
    bolos_ux_sskr_entry_header_update(buffer, HEADER_POSITION, HEADER_POSITION,
                                      &final_size, &count);
    /* The provisional value the short-form formula gives for 24. */
    assert_int_equal(final_size, 4 + 24 + 4);

    for (unsigned int declared = 0; declared <= 0xFF; declared++) {
        buffer[LONG_LENGTH_POSITION] = (uint8_t) declared;
        final_size = FINAL_SIZE_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(buffer, LONG_LENGTH_POSITION, LONG_LENGTH_POSITION,
                                          &final_size, &count);

        size_t expected = 4 + 1 + declared + 4;
        if (expected > SSKR_SHARE_MAX_WIRE_LENGTH) {
            expected = SSKR_SHARE_MAX_WIRE_LENGTH;
        }
        assert_int_equal(final_size, expected);
        assert_true(final_size <= SSKR_SHARE_MAX_WIRE_LENGTH);
    }

    assert_int_equal(count, COUNT_UNTOUCHED);
}

/*
 * The clamp is what keeps a declared length from setting an expected share
 * length no share can have: a shard is SSKR_METADATA_LENGTH_BYTES plus at most
 * SSKR_MAX_STRENGTH_BYTES, and the wire form adds the 5-byte header and the
 * CRC32 on top. Either side of the largest length that fits.
 */
static void test_declared_length_is_clamped_to_the_maximum_wire_length(void **state)
{
    (void) state;

    const unsigned int largest_unclamped = SSKR_SHARE_MAX_WIRE_LENGTH - 4 - 1 - 4;
    uint8_t buffer[BUFFER_LENGTH] = {0};
    put_tag(buffer);
    buffer[HEADER_POSITION] = byte_string_header(24);

    size_t final_size = FINAL_SIZE_UNTOUCHED;
    uint8_t count = COUNT_UNTOUCHED;

    buffer[LONG_LENGTH_POSITION] = (uint8_t) largest_unclamped;
    bolos_ux_sskr_entry_header_update(buffer, LONG_LENGTH_POSITION, LONG_LENGTH_POSITION,
                                      &final_size, &count);
    assert_int_equal(final_size, SSKR_SHARE_MAX_WIRE_LENGTH);

    buffer[LONG_LENGTH_POSITION] = (uint8_t) (largest_unclamped - 1);
    bolos_ux_sskr_entry_header_update(buffer, LONG_LENGTH_POSITION, LONG_LENGTH_POSITION,
                                      &final_size, &count);
    assert_int_equal(final_size, SSKR_SHARE_MAX_WIRE_LENGTH - 1);

    buffer[LONG_LENGTH_POSITION] = 0xFF;
    bolos_ux_sskr_entry_header_update(buffer, LONG_LENGTH_POSITION, LONG_LENGTH_POSITION,
                                      &final_size, &count);
    assert_int_equal(final_size, SSKR_SHARE_MAX_WIRE_LENGTH);
}

/*
 * Reserved additional-info values (25-31) have no literal length of their own.
 * There is nothing valid to compute, so the expected length is pinned to the
 * same maximum a too-long declared length is clamped to -- far enough out that
 * entry cannot be considered complete at a plausible-looking word count -- and
 * the share count is left exactly as it was at every position that could
 * otherwise establish it. That last part is the point: the count stays at
 * whatever the caller had (zero, for a fresh entry), and a share count of zero
 * is what bolos_ux_sskr_hex_check() refuses.
 */
static void test_reserved_additional_info_pins_the_length_and_sets_no_count(void **state)
{
    (void) state;

    for (uint8_t additional_info = 25; additional_info <= 31; additional_info++) {
        uint8_t buffer[BUFFER_LENGTH] = {0};
        put_tag(buffer);
        buffer[HEADER_POSITION] = byte_string_header(additional_info);
        buffer[LONG_LENGTH_POSITION] = 0x01;
        buffer[COUNT_POSITION_SHORT] = 0x22;
        buffer[COUNT_POSITION_LONG] = 0x33;

        size_t final_size = FINAL_SIZE_UNTOUCHED;
        uint8_t count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(buffer, HEADER_POSITION, HEADER_POSITION,
                                          &final_size, &count);
        assert_int_equal(final_size, SSKR_SHARE_MAX_WIRE_LENGTH);
        assert_int_equal(count, COUNT_UNTOUCHED);

        /* Neither the long-form length byte nor either count position says
         * anything once the header is a reserved form. */
        bolos_ux_sskr_entry_header_update(buffer, LONG_LENGTH_POSITION, LONG_LENGTH_POSITION,
                                          &final_size, &count);
        bolos_ux_sskr_entry_header_update(buffer, COUNT_POSITION_SHORT, COUNT_POSITION_SHORT,
                                          &final_size, &count);
        bolos_ux_sskr_entry_header_update(buffer, COUNT_POSITION_LONG, COUNT_POSITION_LONG,
                                          &final_size, &count);
        assert_int_equal(final_size, SSKR_SHARE_MAX_WIRE_LENGTH);
        assert_int_equal(count, COUNT_UNTOUCHED);
    }
}

/*
 * The share count is the member threshold nibble plus one, read out of the
 * shard's metadata -- at the 8th byte of the share for a short-form header,
 * one byte later for a long-form one, since that form spends a byte more on
 * its length. Each position speaks only for its own form.
 */
static void test_share_count_is_read_at_the_position_the_length_form_dictates(void **state)
{
    (void) state;

    for (unsigned int metadata = 0; metadata <= 0xFF; metadata++) {
        const uint8_t expected = (uint8_t) ((metadata & 0x0F) + 1);

        uint8_t short_form[BUFFER_LENGTH] = {0};
        put_tag(short_form);
        short_form[HEADER_POSITION] = byte_string_header(23);
        short_form[COUNT_POSITION_SHORT] = (uint8_t) metadata;
        short_form[COUNT_POSITION_LONG] = (uint8_t) ~metadata;

        size_t final_size = FINAL_SIZE_UNTOUCHED;
        uint8_t count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(short_form, COUNT_POSITION_SHORT, COUNT_POSITION_SHORT,
                                          &final_size, &count);
        assert_int_equal(count, expected);
        assert_int_equal(final_size, FINAL_SIZE_UNTOUCHED);

        /* The long-form position says nothing about a short-form share. */
        count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(short_form, COUNT_POSITION_LONG, COUNT_POSITION_LONG,
                                          &final_size, &count);
        assert_int_equal(count, COUNT_UNTOUCHED);

        uint8_t long_form[BUFFER_LENGTH] = {0};
        put_tag(long_form);
        long_form[HEADER_POSITION] = byte_string_header(24);
        long_form[COUNT_POSITION_SHORT] = (uint8_t) ~metadata;
        long_form[COUNT_POSITION_LONG] = (uint8_t) metadata;

        count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(long_form, COUNT_POSITION_SHORT, COUNT_POSITION_SHORT,
                                          &final_size, &count);
        assert_int_equal(count, COUNT_UNTOUCHED);

        bolos_ux_sskr_entry_header_update(long_form, COUNT_POSITION_LONG, COUNT_POSITION_LONG,
                                          &final_size, &count);
        assert_int_equal(count, expected);
        assert_int_equal(final_size, FINAL_SIZE_UNTOUCHED);
    }
}

/*
 * Every other position in a share carries payload, and payload says nothing
 * about the share's shape. A byte that would look like a header or a metadata
 * byte elsewhere must not be read as one here.
 */
static void test_other_positions_establish_nothing(void **state)
{
    (void) state;

    uint8_t buffer[BUFFER_LENGTH];
    memset(buffer, 0xFF, sizeof(buffer));
    put_tag(buffer);
    buffer[HEADER_POSITION] = byte_string_header(23);

    for (size_t word_number = 0; word_number < BUFFER_LENGTH; word_number++) {
        if (word_number == HEADER_POSITION || word_number == LONG_LENGTH_POSITION ||
            word_number == COUNT_POSITION_SHORT || word_number == COUNT_POSITION_LONG) {
            continue;
        }

        size_t final_size = FINAL_SIZE_UNTOUCHED;
        uint8_t count = COUNT_UNTOUCHED;
        bolos_ux_sskr_entry_header_update(buffer, word_number, word_number, &final_size, &count);
        assert_int_equal(final_size, FINAL_SIZE_UNTOUCHED);
        assert_int_equal(count, COUNT_UNTOUCHED);
    }
}

/*
 * Position in the buffer and position in the share are two different things.
 * They only coincide while the first share of a set is being entered: for
 * every share after it the word number restarts and the buffer index keeps
 * counting, and the entry paths read the *first* share's byte-string header to
 * decide which form the set uses. Every share in a set has the same shape, so
 * this holds; it is checked here because the extraction has to keep taking the
 * two apart the same way.
 */
static void test_word_number_and_buffer_index_are_independent(void **state)
{
    (void) state;

    const size_t second_share = 32;
    uint8_t buffer[BUFFER_LENGTH] = {0};
    put_tag(buffer);
    buffer[HEADER_POSITION] = byte_string_header(20);
    put_tag(buffer + second_share);
    /* A byte that would be read as a long-form header, were the second share's
     * own header the one consulted. */
    buffer[second_share + HEADER_POSITION] = byte_string_header(24);
    buffer[second_share + COUNT_POSITION_SHORT] = 0x04;

    size_t final_size = FINAL_SIZE_UNTOUCHED;
    uint8_t count = COUNT_UNTOUCHED;

    /* Fourth word of the second share: the length comes from the byte just
     * entered, wherever in the buffer it landed. */
    bolos_ux_sskr_entry_header_update(buffer, second_share + HEADER_POSITION, HEADER_POSITION,
                                      &final_size, &count);
    assert_int_equal(final_size, 4 + 24 + 4);

    /* Eighth word of the second share: the form comes from the first share's
     * header, which is the short form, so this position does establish the
     * count. */
    bolos_ux_sskr_entry_header_update(buffer, second_share + COUNT_POSITION_SHORT,
                                      COUNT_POSITION_SHORT, &final_size, &count);
    assert_int_equal(count, 5);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_short_form_length_is_read_from_the_header),
        cmocka_unit_test(test_long_form_length_is_read_from_the_byte_after_the_header),
        cmocka_unit_test(test_declared_length_is_clamped_to_the_maximum_wire_length),
        cmocka_unit_test(test_reserved_additional_info_pins_the_length_and_sets_no_count),
        cmocka_unit_test(test_share_count_is_read_at_the_position_the_length_form_dictates),
        cmocka_unit_test(test_other_positions_establish_nothing),
        cmocka_unit_test(test_word_number_and_buffer_index_are_independent),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
