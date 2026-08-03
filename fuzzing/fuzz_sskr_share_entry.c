/*
 * bolos_ux_sskr_entry_header_update(), driven the way the entry screens drive
 * it: one entered ByteWord at a time, then the completed buffer handed to
 * bolos_ux_sskr_hex_check().
 *
 * This function is where a typed CBOR header turns into two numbers the rest
 * of the entry path trusts -- how many bytes the share will be, and how many
 * shares the set holds. Both come out of bytes the user typed, and they are
 * read at fixed positions (byte 3, byte 4, byte 7, byte 8) whose meaning
 * depends on which form of length the *first* share declared. There is no
 * reasonable way to explore that space by hand: the reachable states are the
 * product of the additional-information nibble, the declared length byte, the
 * member-threshold nibble, and how many bytes were entered before the set was
 * declared complete.
 *
 * The loop below is nbgl/sskr_shares.c: sskr_shares_word_add() appends the
 * decoded byte and calls the header update with (buffer, length,
 * words entered in the current share), then sskr_shares_complete_check()
 * decides, from final_size and count, whether the share is finished and
 * whether the set is. The BAGL screens (bagl/nanox_enter_phrase.c,
 * bagl/nanos_enter_phrase.c) do the same arithmetic. Reproducing the caller
 * rather than calling the function with independent (index, word_number)
 * pairs is what makes a finding here a finding about the application: the two
 * counters advance together, and it is their relationship that the fixed
 * positions depend on.
 *
 * The fuzzer's bytes stand for the *decoded* ByteWords, which is exactly the
 * 0..255 range bolos_ux_sskr_byteword_to_hex() produces, so no input is
 * wasted on strings that are not in the wordlist -- fuzz_sskr_bytewords.c
 * covers that step.
 *
 * The buffer is allocated at exactly the number of bytes the run will enter,
 * so AddressSanitizer's redzone stands where the entered data ends. On the
 * device it is a fixed 3664-byte array and a read past the entered bytes but
 * inside the array is invisible to any sanitizer.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <os.h>

#include "sskr/common_sskr.h"

/* See fuzz_sskr_hex_check.c for where this comes from. */
#define SSKR_SHARES_MAX_LENGTH (3664)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) {
        return 0;
    }
    if (size > SSKR_SHARES_MAX_LENGTH) {
        size = SSKR_SHARES_MAX_LENGTH;
    }

    uint8_t *buffer = malloc(size);
    if (buffer == NULL) {
        return 0;
    }

    /* sskr_shares_reset(): no word and no share entered yet. */
    size_t length = 0;
    size_t current_word_index = (size_t) -1;
    uint8_t current_share_index = (uint8_t) -1;
    size_t final_size = 0;
    uint8_t count = 0;

    for (size_t i = 0; i < size; i++) {
        /* sskr_shares_word_add() */
        buffer[length] = data[i];
        bolos_ux_sskr_entry_header_update(buffer,
                                          length,
                                          current_word_index + 1,
                                          &final_size,
                                          &count);
        length++;
        current_word_index++;

        /* sskr_shares_complete_check(): the final size is not known before the
         * fifth word, and a share is finished once as many words as it
         * declared have been entered. */
        if (current_word_index + 1 < 5 || current_word_index + 1 < final_size) {
            continue;
        }

        current_share_index++;
        if ((uint8_t) (current_share_index + 1) < count) {
            /* More shares to come: the word counter restarts, the buffer
             * keeps growing. */
            current_word_index = (size_t) -1;
            continue;
        }

        /* sskr_shares_check(): the set is complete, so the frame check runs on
         * everything entered so far. It erases its input on refusal, hence the
         * copy. */
        unsigned char *entered = malloc(length);
        if (entered != NULL) {
            memcpy(entered, buffer, length);
            (void) bolos_ux_sskr_hex_check(entered, (unsigned int) length, count);
            free(entered);
        }

        /* sskr_shares_reset() runs whether the check passed or failed; the
         * screens then start a fresh set. */
        length = 0;
        current_word_index = (size_t) -1;
        current_share_index = (uint8_t) -1;
        final_size = 0;
        count = 0;
    }

    free(buffer);
    return 0;
}
