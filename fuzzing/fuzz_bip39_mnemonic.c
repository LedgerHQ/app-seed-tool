/*
 * bolos_ux_bip39_mnemonic_decode() and the BIP-39 wordlist searches behind the
 * keyboard.
 *
 * The decoder splits a typed phrase on spaces and compares each word against
 * the 2048-entry table, which is stored as one 11068-byte blob plus an offset
 * array -- so every comparison is an offset arithmetic problem, and the words
 * are 3 to 8 characters while the buffer it copies them into is 10. The
 * comparison is constant time, so it reads its full length whatever the entry
 * holds; a read past the last entry of the blob is exactly the shape of defect
 * this table invites, and one has already been found there by hand.
 *
 * `bitslength` is fixed at 33, not fuzzed. Every caller in the application
 * passes 32 + 1, and the decoder writes `bits[bi / 8]` for bi up to 11 words'
 * worth of bits without consulting that length -- so fuzzing it would report
 * an overflow the application cannot reach, and drown the findings that
 * matter. What is fuzzed is the phrase, which is what the user types.
 *
 * The prefix searches are the other half: they are called on every keystroke,
 * with a prefix as long as what has been typed so far, and the one that
 * answers with the next possible letters writes into a buffer whose size it is
 * never given.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <os.h>

/* ALPHABET_LENGTH: the size of the next-letter buffer the callers allocate. */
#include "common.h"
#include "bip39/common_bip39.h"

/* What every caller of the decoder passes: 32 bytes of entropy plus the
 * checksum byte. */
#define BIP39_BITS_LENGTH (32 + 1)

/* The longest word in the list is 8 characters; the decoder copies into a
 * 10-byte buffer and refuses anything longer. Prefixes are bounded here at
 * the same figure the keyboard can produce. */
#define BIP39_MAX_PREFIX (10)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) {
        return 0;
    }

    const uint8_t selector = data[0] % 3;
    const uint8_t *body = data + 1;
    const size_t body_len = size - 1;

    switch (selector) {
        case 0: {
            /* A typed phrase. The buffer is sized to it exactly, so the
             * decoder's own reads past the end of the phrase are reported
             * rather than landing in whatever follows on the device. */
            unsigned char *mnemonic = malloc(body_len);
            unsigned char *bits = malloc(BIP39_BITS_LENGTH);
            if (mnemonic == NULL || bits == NULL) {
                free(mnemonic);
                free(bits);
                return 0;
            }
            memcpy(mnemonic, body, body_len);
            memset(bits, 0, BIP39_BITS_LENGTH);

            (void) bolos_ux_bip39_mnemonic_decode(mnemonic,
                                                  (unsigned int) body_len,
                                                  bits,
                                                  BIP39_BITS_LENGTH);
            free(mnemonic);
            free(bits);
            break;
        }
        case 1: {
            /* The same phrase through the entry point the screens call, which
             * allocates the bit buffer itself. */
            unsigned char *mnemonic = malloc(body_len);
            if (mnemonic == NULL) {
                return 0;
            }
            memcpy(mnemonic, body, body_len);

            (void) bolos_ux_bip39_mnemonic_check(mnemonic,
                                                 (unsigned int) body_len);
            free(mnemonic);
            break;
        }
        default: {
            /* One keystroke's worth of prefix, through the three searches the
             * keyboard runs on it. */
            const size_t prefix_len =
                body_len > BIP39_MAX_PREFIX ? BIP39_MAX_PREFIX : body_len;
            unsigned char *prefix = malloc(prefix_len);
            unsigned char *next_letters = malloc(ALPHABET_LENGTH);
            if (prefix == NULL || next_letters == NULL) {
                free(prefix);
                free(next_letters);
                return 0;
            }
            memcpy(prefix, body, prefix_len);
            memset(next_letters, 0, ALPHABET_LENGTH);

            const unsigned int index = bolos_ux_bip39_get_word_idx_starting_with(
                prefix, (unsigned int) prefix_len);
            (void) bolos_ux_bip39_get_word_count_starting_with(
                prefix, (unsigned int) prefix_len);
            (void) bolos_ux_bip39_get_word_next_letters_starting_with(
                prefix, (unsigned int) prefix_len, next_letters);

            /* And the copy of whichever word came back, into a buffer sized
             * for the longest word plus its terminator. */
            unsigned char *word = malloc(8 + 1);
            if (word != NULL) {
                (void) bolos_ux_bip39_idx_strcpy(index, word);
                free(word);
            }

            free(prefix);
            free(next_letters);
            break;
        }
    }

    return 0;
}
