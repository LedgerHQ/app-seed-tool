/*
 * bolos_ux_sskr_hex_check(): the CBOR frame, the shared metadata and the
 * CRC-32 of a set of SSKR shares typed in as ByteWords.
 *
 * The two things this function derives offsets from -- the length of the share
 * buffer and the number of shares in it -- both come from what was typed: the
 * length is one byte per ByteWord entered, and the count is the
 * member-threshold nibble of the entered data. So the fuzzed input is the
 * *pair*, not the buffer alone. Feeding the buffer with a fixed count leaves
 * the division `sskr_shares_hex_length / sskr_shares_count` at one value and
 * misses the whole class of defect that lives in it.
 *
 * The buffer is a heap allocation of exactly the length passed to the
 * function, so AddressSanitizer's redzone stands where the entered data ends.
 * On the device the shares live in a fixed 3664-byte array
 * (SSKR_SHARES_MAX_LENGTH), and a read that runs past the entered bytes but
 * stays inside that array is invisible to any sanitizer; sizing the allocation
 * to the data is what turns those reads into a report.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <os.h>

#include "sskr/common_sskr.h"

/* The share buffer of the entry screens: 16 shares * 229 characters, which is
 * what one share takes written out as 46 space-separated ByteWords. Entry
 * writes one byte per ByteWord into that same buffer, so the entered length
 * never exceeds 16 * 46, but the buffer -- and hence the length a caller can
 * hand this function -- is the larger figure. Spelled out here rather than
 * included: the constant lives in nbgl/sskr_shares.h behind
 * `#if defined(SCREEN_SIZE_WALLET)`, and that header pulls the screen stack
 * in with it. */
#define SSKR_SHARES_MAX_LENGTH (3664)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* One byte of share count, at least one byte of share buffer. */
    if (size < 2) {
        return 0;
    }

    const unsigned int share_count = data[0];
    size_t length = size - 1;

    /* The entry screens cannot hold more than this, so neither does the
     * fuzzer: a longer buffer only makes the fuzzer spend its budget on
     * lengths the application cannot reach. */
    if (length > SSKR_SHARES_MAX_LENGTH) {
        length = SSKR_SHARES_MAX_LENGTH;
    }

    unsigned char *shares = malloc(length);
    if (shares == NULL) {
        return 0;
    }
    memcpy(shares, data + 1, length);

    /* The function erases its input on refusal, which is why it gets a copy
     * and why the copy is writable. */
    (void) bolos_ux_sskr_hex_check(shares, (unsigned int) length, share_count);

    free(shares);
    return 0;
}
