/*
 * The SSKR ByteWords table: turning what was typed into bytes, and back.
 *
 * Four entry points, all reached directly from the keyboard, all indexing the
 * same 1024-byte table of 256 four-character words:
 *
 *   - bolos_ux_sskr_byteword_to_hex() compares a typed word against every
 *     entry. The comparison is constant time, so it reads all four bytes of
 *     the candidate whatever the candidate holds.
 *   - bolos_ux_sskr_share_hex_decode() goes the other way, indexing the table
 *     at 4 * value for a value it is handed. A 256-entry table and a byte
 *     index leave no slack: the bound that matters is the output buffer's,
 *     not the table's.
 *   - the two prefix searches behind the suggestion buttons and the keyboard
 *     mask read `prefixlength` bytes of the prefix and, for the second one,
 *     write one byte per distinct following letter into a caller's buffer
 *     whose size it is never told.
 *
 * Every buffer below is a heap allocation of exactly the size the call is
 * allowed to touch, so AddressSanitizer's redzone sits at the boundary each
 * function is supposed to respect. That is the whole assertion: none of these
 * functions returns anything that would reveal an overrun.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <os.h>

/* ALPHABET_LENGTH: what the keyboard screens allocate for the next-letter
 * answer. Sizing the allocation to it is what makes an overrun of that answer
 * visible; it is the tightest of the two bounds in the application, the NBGL
 * keyboard mask using a stack array of exactly that size. */
#include "common.h"
#include "sskr/common_sskr.h"
#include "sskr/seed_rom_variables.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) {
        return 0;
    }

    /* First byte selects the entry point, so one corpus entry does not have to
     * serve all four. */
    const uint8_t selector = data[0] % 4;
    const uint8_t *body = data + 1;
    size_t body_len = size - 1;

    switch (selector) {
        case 0: {
            /* A typed ByteWord, four characters, read in full by the
             * constant-time comparison against every table entry. */
            unsigned char *word = malloc(SSKR_BYTEWORD_LENGTH);
            if (word == NULL) {
                return 0;
            }
            memset(word, 0, SSKR_BYTEWORD_LENGTH);
            memcpy(word,
                   body,
                   body_len < SSKR_BYTEWORD_LENGTH ? body_len
                                                   : SSKR_BYTEWORD_LENGTH);

            uint8_t value = 0;
            (void) bolos_ux_sskr_byteword_to_hex(word, &value);
            free(word);
            break;
        }
        case 1: {
            /* A decoded share back to space-separated ByteWords. The output
             * length is fuzzed alongside the input: it is what the function
             * checks its writes against, and the two are independent on the
             * device (one comes from the entered share, the other from the
             * display buffer). */
            const size_t out_len = body[0];
            const uint8_t *input = body + 1;
            const size_t input_len = body_len - 1;

            unsigned char *output = malloc(out_len == 0 ? 1 : out_len);
            unsigned char *in_copy = malloc(input_len == 0 ? 1 : input_len);
            if (output == NULL || in_copy == NULL) {
                free(output);
                free(in_copy);
                return 0;
            }
            memcpy(in_copy, input, input_len);

            (void) bolos_ux_sskr_share_hex_decode(in_copy,
                                                  (unsigned int) input_len,
                                                  output,
                                                  (unsigned int) out_len);
            free(output);
            free(in_copy);
            break;
        }
        case 2: {
            /* The two searches that only read. The prefix is exactly as long
             * as the fuzzer made it, including longer than a ByteWord. */
            unsigned char *prefix = malloc(body_len);
            if (prefix == NULL) {
                return 0;
            }
            memcpy(prefix, body, body_len);

            const unsigned int index =
                bolos_ux_sskr_get_word_idx_starting_with(prefix,
                                                         (unsigned int) body_len);
            (void) bolos_ux_sskr_get_word_count_starting_with(
                prefix, (unsigned int) body_len);

            /* And the copy out of the table at whatever index came back,
             * into a buffer sized for one word plus its terminator. */
            unsigned char *word = malloc(SSKR_BYTEWORD_LENGTH + 1);
            if (word != NULL) {
                (void) bolos_ux_sskr_idx_strcpy(index, word);
                free(word);
            }
            free(prefix);
            break;
        }
        default: {
            /* The search that writes. next_letters_buffer is a plain pointer
             * the function walks forward, one step per distinct following
             * letter, with no length to stop at. */
            unsigned char *prefix = malloc(body_len);
            unsigned char *next_letters = malloc(ALPHABET_LENGTH);
            if (prefix == NULL || next_letters == NULL) {
                free(prefix);
                free(next_letters);
                return 0;
            }
            memcpy(prefix, body, body_len);
            memset(next_letters, 0, ALPHABET_LENGTH);

            (void) bolos_ux_sskr_get_word_next_letters_starting_with(
                prefix, (unsigned int) body_len, next_letters);
            free(prefix);
            free(next_letters);
            break;
        }
    }

    return 0;
}
