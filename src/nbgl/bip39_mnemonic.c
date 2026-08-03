/*******************************************************************************
 *   Ledger Seed Tool application
 *   (c) 2016-2026 Ledger SAS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#include <os.h>
#include <string.h>

#include "../common/bip39/common_bip39.h"
#include "../common/common.h"
#include "../common/sskr/common_sskr.h"
/* clang-format off */
#include "./bip39_mnemonic.h"
/* clang-format on */
#include "./sskr_shares.h"

#if defined(SCREEN_SIZE_WALLET)

typedef struct bip39_buffer_struct {
    // the mnemonic passphrase, built over time
    char buffer[BIP39_MNEMONIC_MAX_LENGTH];
    // current length of the mnemonic passphrase
    size_t length;
    // index of the current word ((size_t)-1 mean there is no word currently)
    size_t current_word_index;
    // array of every stored word lengths (used for removing them if needed)
    size_t word_lengths[BIP39_MNEMONIC_SIZE_24];
    // expected number of word in the final mnemonic (12 or 18 or 24)
    size_t final_size;
} bip39_buffer_t;

static bip39_buffer_t mnemonic = {0};

size_t bip39_mnemonic_shrink(const size_t size) {
    if (size == 0 || size > mnemonic.length) {
        // shrink all
        mnemonic.length = 0;
    } else {
        mnemonic.length -= size;
    }
    memzero(&mnemonic.buffer[mnemonic.length],
            BIP39_MNEMONIC_MAX_LENGTH - mnemonic.length);
    return mnemonic.length;
}

void bip39_mnemonic_final_size_set(const size_t size) {
    mnemonic.final_size = size;
}

size_t bip39_mnemonic_final_size_get(void) { return mnemonic.final_size; }

size_t bip39_mnemonic_current_word_number_get(void) {
    return mnemonic.current_word_index + 1;
}

void bip39_mnemonic_reset(void) {
    memzero(&mnemonic, sizeof(mnemonic));
    mnemonic.current_word_index = (size_t)-1;
}

bool bip39_mnemonic_word_remove(void) {
    PRINTF("Removing a word, currently there is '%d' of them\n",
           mnemonic.current_word_index + 1);
    if (mnemonic.current_word_index == (size_t)-1) {
        return false;
    }
    const size_t current_length =
        mnemonic.word_lengths[mnemonic.current_word_index];
    mnemonic.current_word_index--;
    // removing previous word from mnemonic buffer (+ 1 blank space)
    bip39_mnemonic_shrink(current_length + 1);
    PRINTF("Number of remaining words in the mnemonic: '%d'\n",
           mnemonic.current_word_index + 1);
    return true;
}

size_t bip39_mnemonic_word_add(const char* const buffer, const size_t size) {
    if (mnemonic.current_word_index != (size_t)-1) {
        // adding an extra white space ' ' between words
        mnemonic.buffer[mnemonic.length++] = ' ';
        mnemonic.buffer[mnemonic.length] = '\0';
    }
    memcpy(&mnemonic.buffer[0] + mnemonic.length, buffer, size);
    mnemonic.length += size;
    mnemonic.current_word_index++;
    mnemonic.word_lengths[mnemonic.current_word_index] = size;
    PRINTF("Number of words in the mnemonic: '%d'\n",
           bip39_mnemonic_current_word_number_get());
    PRINTF("Current mnemonic length: %zu characters\n", mnemonic.length);
    return bip39_mnemonic_current_word_number_get();
}

bool bip39_mnemonic_complete_check(void) {
    return (mnemonic.final_size == 0 ? false
                                     : (mnemonic.current_word_index + 1) >=
                                           bip39_mnemonic_final_size_get());
}

bool bip39_mnemonic_check(bool* match) {
    *match = false;
    if (!bip39_mnemonic_complete_check()) {
        return false;
    }
    PRINTF("Checking the mnemonic: %zu characters\n", mnemonic.length);

    if (bolos_ux_bip39_mnemonic_check((unsigned char*)&mnemonic.buffer[0],
                                      mnemonic.length) == false) {
        bip39_mnemonic_reset();
        return false;
    }

    bool reconstructed = true;
    // Against VERDICT_MATCH, not "not zero": every other value, corrupted or
    // not, is a mismatch. See common.h.
    *match = (compare_recovery_phrase(&reconstructed) == VERDICT_MATCH);
    // Don't clear the mnemonic just yet as we may need it to generate SSKR
    // shares
    //    bip39_mnemonic_reset();

    return true;
}

bool bip39_mnemonic_from_sskr_shares(unsigned char* seed) {
    // mnemonic.length is a size_t, like every other field of this struct,
    // while every length in the common bolos_ux_* API is an unsigned int.
    // They are the same type on the 32-bit ARM targets, which is why handing
    // &mnemonic.length straight to an unsigned int * parameter went unnoticed;
    // on a 64-bit host it gives the callee the address of an 8-byte object it
    // only writes 4 bytes of. Convert at the boundary rather than move either
    // convention.
    unsigned int words_buffer_length = BIP39_MNEMONIC_MAX_LENGTH;

    bolos_ux_sskr_to_seed_convert(
        (unsigned char*)sskr_shares_get(), sskr_shares_length_get(),
        sskr_sharecount_get(), (unsigned char*)bip39_mnemonic_get(),
        &words_buffer_length, seed);

    mnemonic.length = words_buffer_length;

    if (mnemonic.length > 0) {
        PRINTF("BIP39 mnemonic: %zu characters\n", mnemonic.length);
    }

    // a zero length means the shards could not be combined
    return mnemonic.length > 0;
}

void bip39_mnemonic_encode(const uint8_t* seed, uint8_t seed_len) {
    // convert input bytes to a mnemonic phrase return 0 if mnemonic is invalid
    mnemonic.length = bolos_ux_bip39_mnemonic_encode(
        seed, seed_len, (unsigned char*)bip39_mnemonic_get(),
        BIP39_MNEMONIC_MAX_LENGTH);

    if (mnemonic.length > 0) {
        PRINTF("BIP39 mnemonic: %zu characters\n", mnemonic.length);
    }
}

// Used for BIP39 <-> SSKR roundtrip
char* bip39_mnemonic_get(void) { return mnemonic.buffer; }

size_t bip39_mnemonic_length_get(void) { return mnemonic.length; }

#endif
