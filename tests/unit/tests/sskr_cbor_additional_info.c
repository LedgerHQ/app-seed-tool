/*
 * Regression test for reserved/invalid CBOR additional-info bytes being
 * misinterpreted as a literal share length in sskr_shares_word_add().
 *
 * The 4th byte of the SSKR CBOR header (buffer[3]) is a byte-string header:
 * major type 2, plus a 5-bit additional-info field (buffer[3] & 0x1F).
 * Per RFC 8949, that field means:
 *
 *   0-23    literal length, 0 to 23 bytes                  (short form)
 *   24      one length byte follows (0x58)                 (long form)
 *   25      two length bytes follow (0x59)                 (reserved here)
 *   26      four length bytes follow (0x5A)                (reserved here)
 *   27      eight length bytes follow (0x5B)                (reserved here)
 *   28-30   reserved, no defined meaning
 *   31      indefinite length (0x5F)                        (reserved here)
 *
 * sskr_shares_word_add() only ever special-cases 0-23 (case 3, taken
 * literally) and 24 (case 4, corrected once the extra length byte is read).
 * 25-31 fall through case 3's literal-length formula unmodified:
 *
 *     final_size = 4 + (buffer[3] & 0x1F) + sizeof(uint32_t);
 *
 * treating, say, additional-info 27 as if it meant "27 literal bytes follow",
 * which is not what that CBOR encoding means at all -- there is no valid
 * share of that shape, and there is no length to compute here. The entry
 * still looks "complete" to sskr_shares_complete_check() once the (bogus,
 * small) final_size is reached, well before the share is verified by
 * bolos_ux_sskr_hex_check().
 *
 * No memory overflow results (final_size stays far below
 * SSKR_SHARES_MAX_LENGTH either way -- this is not issue #62/PR #63's bug),
 * but the entry is accepted at a word count that cannot correspond to any
 * valid SSKR share, instead of being pinned to a bound that leaves the
 * downstream CRC/tag check in bolos_ux_sskr_hex_check() a chance to reject
 * it. The BAGL paths (src/bagl/nanos_enter_phrase.c,
 * src/bagl/nanox_enter_phrase.c) contain the identical switch and take the
 * same fix, verified by compilation only -- see the note in
 * sskr_entry_bounds.c on why they cannot be driven from this harness.
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
#include "sskr/sskr-constants.h"
#include "../../../src/nbgl/sskr_shares.h"

extern unsigned char const SSKR_WORDLIST[];

/*
 * sskr_shares.c also holds sskr_shares_from_bip39_mnemonic() and
 * sskr_shares_check(), which reach into the BIP39 module and the seed
 * comparison. Neither is on the entry path under test; these exist only so
 * the object links, and each aborts if it is ever reached.
 */
char *bip39_mnemonic_get(void) { fail_msg("not on the entry path"); }
size_t bip39_mnemonic_length_get(void) { fail_msg("not on the entry path"); }
uint8_t bip39_mnemonic_final_size_get(void) { fail_msg("not on the entry path"); }
unsigned int compare_recovery_phrase(void) { fail_msg("not on the entry path"); }

/* Largest a serialized share can be on the wire: CBOR long-form header
 * (tag + 0x58 + length byte), the shard itself, and the CRC32. Also the
 * bound the fix must fall back to for a reserved additional-info value. */
#define MAX_SHARE_WIRE_LEN \
    (5 + SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES + 4)

/* ByteWord whose decoded value is `b`; the table holds 256 four-letter words. */
static const char *word_for(uint8_t b)
{
    return (const char *) &SSKR_WORDLIST[(size_t) b * SSKR_BYTEWORD_LENGTH];
}

/*
 * Feeds the 3-byte CBOR tag followed by a byte-string header whose
 * additional-info field is `additional_info` (25-31: reserved forms with no
 * literal length of their own). Returns how many words it consumed.
 */
static size_t feed_reserved_additional_info_header(uint8_t additional_info)
{
    assert_true(additional_info >= 25 && additional_info <= 31);

    sskr_shares_word_add(word_for(0xD9)); /* CBOR tag */
    sskr_shares_word_add(word_for(0x9D));
    sskr_shares_word_add(word_for(0x75));
    /* major type 2 (0x40) | additional-info */
    sskr_shares_word_add(word_for((uint8_t)(0x40 | additional_info)));
    return 4;
}

/*
 * A reserved additional-info value (27 here: the 8-byte-length form, 0x5B)
 * has no literal length to compute. Entry must not be considered complete
 * before the same maximum wire length that a too-long declared long-form
 * length is already clamped to elsewhere in this switch -- and must not
 * complete any earlier than that, which is what today's literal-length
 * fallback (final_size = 4 + 27 + 4 = 35) does.
 */
static void test_reserved_additional_info_is_not_taken_as_literal_length(void **state)
{
    (void) state;

    sskr_shares_reset();
    size_t fed = feed_reserved_additional_info_header(27);

    for (size_t target = fed + 1; target <= MAX_SHARE_WIRE_LEN; target++) {
        assert_false(sskr_shares_complete_check());
        sskr_shares_word_add(word_for(0x00));
        assert_int_equal(sskr_shares_current_word_number_get(), target);
    }
    assert_true(sskr_shares_complete_check());
}

/*
 * Same property for the other reserved/invalid forms named in the CBOR
 * spec: 25 (0x59, two length bytes), 26 (0x5A, four length bytes), 28-30
 * (reserved, no meaning), and 31 (0x5F, indefinite length).
 */
static void test_all_reserved_additional_info_values_reach_max_wire_length(void **state)
{
    (void) state;

    const uint8_t reserved[] = {25, 26, 28, 29, 30, 31};
    for (size_t i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        sskr_shares_reset();
        size_t fed = feed_reserved_additional_info_header(reserved[i]);

        for (size_t target = fed + 1; target < MAX_SHARE_WIRE_LEN; target++) {
            assert_false(sskr_shares_complete_check());
            sskr_shares_word_add(word_for(0x00));
        }
        assert_false(sskr_shares_complete_check());
        sskr_shares_word_add(word_for(0x00));
        assert_true(sskr_shares_complete_check());
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_reserved_additional_info_is_not_taken_as_literal_length),
        cmocka_unit_test(test_all_reserved_additional_info_values_reach_max_wire_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
