/*
 * Entering an SSKR secret that takes more than one share.
 *
 * sskr_shares_complete_check() answers a question the keyboard asks after
 * every word: is the user done? For a one-share secret the answer is simply
 * "yes, once the declared length is reached". For a two-of-three or a
 * three-of-five it is not: the same buffer has to go on collecting the next
 * share behind the one just finished, and the word counter has to go back to
 * zero so the next share is entered from its first ByteWord while the bytes
 * already collected stay where they are.
 *
 * That is this branch:
 *
 *     shares.current_share_index++;
 *     if (sskr_shareindex_get() < sskr_sharecount_get()) {
 *         shares.current_word_index = (size_t) -1;
 *         return false;
 *     }
 *
 * and it was the last reachable thing in src/nbgl/sskr_shares.c that no unit
 * test entered -- both its lines carried a zero counter over the whole suite.
 * The tests around it stop at one share: sskr_cbor_additional_info.c drives
 * complete_check() repeatedly, but only ever with a header whose additional
 * info is a reserved CBOR form, which leaves the share count at 0 and so takes
 * the other exit every time.
 *
 * How the count gets there matters for reading the tests below. Nothing tells
 * the entry path in advance how many shares to expect: sskr_shares_word_add()
 * reads it out of the share itself, from the low nibble of the eighth byte,
 *
 *     case 7: count = (buffer[length] & 0x0F) + 1;
 *
 * so the first share is what decides how many more the user will be asked
 * for. The shares fed here are therefore built to the shape the switch in
 * word_add() expects -- a short-form CBOR byte-string header, then a body,
 * then the CRC32 -- rather than being invented; a share that did not reach its
 * eighth word would leave the count at 0 and never reach this branch at all.
 *
 * No defect is fixed here, and nothing under src/ is touched.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * testutils.h has to come first: it defines WIDE, which the ROM variable
 * declarations reached through common_sskr.h are written in terms of. Held in
 * place the way src/nbgl/sskr_shares.c holds its own ordered include.
 */
/* clang-format off */
#include "testutils.h"
#include "sskr/common_sskr.h"
/* clang-format on */

#include "../../../src/nbgl/bip39_mnemonic.h"
#include "../../../src/nbgl/sskr_shares.h"

/*
 * sskr_shares.c also holds sskr_shares_from_bip39_mnemonic() and
 * sskr_shares_check(), which reach into the BIP39 module and the seed
 * comparison, and from there into the os_derive_bip32_no_throw() syscall.
 * Neither is on the entry path under test; these exist only so the object
 * links, and each aborts if it is ever reached.
 */
char* bip39_mnemonic_get(void) { fail_msg("not on the entry path"); }
size_t bip39_mnemonic_length_get(void) { fail_msg("not on the entry path"); }
size_t bip39_mnemonic_final_size_get(void) {
    fail_msg("not on the entry path");
}
unsigned int compare_recovery_phrase(bool* reconstructed) {
    (void)reconstructed;
    fail_msg("not on the entry path");
}

/*
 * Declared body length of the shares fed below, in bytes: the metadata a
 * serialized shard carries plus a 128-bit secret, which is the smallest real
 * share and well inside the 0-23 the short-form CBOR header can express.
 */
#define SHARE_BODY_LENGTH (SSKR_METADATA_LENGTH_BYTES + 16)

/*
 * How many ByteWords one such share is: the 3-byte CBOR tag, the byte-string
 * header, the body, and the CRC32. The same arithmetic word_add() does for
 * final_size, spelled out here so that a change to either side shows up as a
 * disagreement rather than as two matching mistakes.
 */
#define SHARE_WORDS (3 + 1 + SHARE_BODY_LENGTH + sizeof(uint32_t))

/* Word carrying the member threshold in its low nibble; word_add() reads it
 * when the eighth word of a share arrives. */
#define THRESHOLD_WORD 8

/* ByteWord whose decoded value is `value`; the table holds 256 four-letter
 * words, so every byte has one. */
static const char* word_for(const uint8_t value) {
    return (const char*)&SSKR_WORDLIST[(size_t)value * SSKR_BYTEWORD_LENGTH];
}

/*
 * Feeds one whole share, whose eighth byte declares `share_count` shares in
 * total. Everything else is filler: nothing on this path looks at the body,
 * and bolos_ux_sskr_hex_check() -- which would -- only runs later, from
 * sskr_shares_check(), which is not reached here.
 */
static void feed_one_share(const uint8_t share_count) {
    assert_true(share_count >= 1 && share_count <= 16);

    sskr_shares_word_add(word_for(0xD9)); /* CBOR tag                  */
    sskr_shares_word_add(word_for(0x9D));
    sskr_shares_word_add(word_for(0x75));
    /* major type 2, short form, declaring SHARE_BODY_LENGTH bytes */
    sskr_shares_word_add(word_for((uint8_t)(0x40 | SHARE_BODY_LENGTH)));

    for (size_t word = 5; word <= SHARE_WORDS; word++) {
        const uint8_t value =
            word == THRESHOLD_WORD ? (uint8_t)(share_count - 1) : (uint8_t)word;
        sskr_shares_word_add(word_for(value));
    }

    assert_int_equal(sskr_shares_current_word_number_get(), SHARE_WORDS);
    assert_int_equal(sskr_sharecount_get(), share_count);
}

/*
 * The property the branch exists for, on the smallest secret that needs it.
 * The two things that have to happen together are checked separately: the word
 * counter goes back to zero, so the next share is entered from its first
 * ByteWord, and the buffer does not, so the share just finished is still there
 * when the last one arrives.
 */
static void test_a_finished_share_reopens_entry_for_the_next_one(void** state) {
    (void)state;

    sskr_shares_reset();
    /* Nothing has been completed yet, and nothing is expected yet either. */
    assert_int_equal(sskr_shareindex_get(), 0);
    assert_int_equal(sskr_sharecount_get(), 0);

    feed_one_share(2);
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS);

    /* Complete as a share, but not as an entry. */
    assert_false(sskr_shares_complete_check());
    assert_int_equal(sskr_shareindex_get(), 1);
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS);

    feed_one_share(2);
    assert_int_equal(sskr_shares_length_get(), 2 * SHARE_WORDS);

    /* The count is reached: the entry is done. */
    assert_true(sskr_shares_complete_check());
    assert_int_equal(sskr_shareindex_get(), 2);
    assert_int_equal(sskr_shares_length_get(), 2 * SHARE_WORDS);
}

/*
 * The same, asked for repeatedly rather than once: three shares means the
 * branch is taken twice and the far exit once. A comparison that stopped one
 * share early, or ran one share late, disagrees with the running length here.
 */
static void test_entry_keeps_asking_until_the_declared_count_is_reached(
    void** state) {
    (void)state;

    static const uint8_t share_count = 3;

    sskr_shares_reset();

    for (uint8_t share = 1; share <= share_count; share++) {
        feed_one_share(share_count);
        assert_int_equal(sskr_shares_length_get(), share * SHARE_WORDS);

        if (share < share_count) {
            assert_false(sskr_shares_complete_check());
            /* Reopened at the first word of the next share. */
            assert_int_equal(sskr_shares_current_word_number_get(), 0);
        } else {
            assert_true(sskr_shares_complete_check());
            /* And the last share's words are still counted, not cleared. */
            assert_int_equal(sskr_shares_current_word_number_get(),
                             SHARE_WORDS);
        }
        assert_int_equal(sskr_shareindex_get(), share);
    }

    assert_int_equal(sskr_shares_length_get(), share_count * SHARE_WORDS);
}

/*
 * The other side of the same comparison, and the reason it is a strict one: a
 * secret held in a single share is finished the moment that share is, with no
 * second one asked for. This is what a `<=` in place of the `<` would break,
 * silently, by demanding a share the user does not have.
 */
static void test_a_single_share_entry_completes_at_once(void** state) {
    (void)state;

    sskr_shares_reset();
    feed_one_share(1);

    assert_true(sskr_shares_complete_check());
    assert_int_equal(sskr_shareindex_get(), 1);
    assert_int_equal(sskr_shares_current_word_number_get(), SHARE_WORDS);
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS);
}

/*
 * What the reopened state actually is, spelled out rather than left implied.
 * The branch writes (size_t)-1, not 0, which is exactly the value
 * sskr_shares_reset() leaves -- so between two shares the entry looks empty to
 * sskr_shares_word_remove(), and a removal there is refused even though the
 * buffer is far from empty. Backing out therefore cannot reach into the share
 * already finished; it only ever takes back words of the share being entered.
 * That is current behaviour, pinned here because it is the observable
 * difference between writing (size_t)-1 and writing 0, which the word counter
 * alone does not distinguish.
 */
static void test_removal_after_a_finished_share_is_refused(void** state) {
    (void)state;

    sskr_shares_reset();
    feed_one_share(2);
    assert_false(sskr_shares_complete_check());

    /* No word of the second share has been entered yet. */
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
    assert_false(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS);

    /* Once one has, it is that one that goes. */
    sskr_shares_word_add(word_for(0xD9));
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS + 1);
    assert_true(sskr_shares_word_remove());
    assert_int_equal(sskr_shares_length_get(), SHARE_WORDS);
    assert_int_equal(sskr_shares_current_word_number_get(), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_a_finished_share_reopens_entry_for_the_next_one),
        cmocka_unit_test(
            test_entry_keeps_asking_until_the_declared_count_is_reached),
        cmocka_unit_test(test_a_single_share_entry_completes_at_once),
        cmocka_unit_test(test_removal_after_a_finished_share_is_refused),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
