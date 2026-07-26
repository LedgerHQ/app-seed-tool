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

#include "./seed_rom_variables.h"

/**
 * @brief Encodes 64 bytes of data into a Base64 string.
 *
 * @param[in]  src Pointer to the input data.
 * @param[out] dst Pointer to the output buffer.
 *
 * @return The number of bytes written to the output buffer.
 */
uint8_t base64_encode_64bytes(const uint8_t* src, char* dst) {
    char* dst_start = dst;
    uint32_t value;
    uint8_t i;

    // Encode the complete 3-byte groups (63 of the 64 input bytes)
    for (i = 0; i + 3 <= BIP85_ENTROPY_LENGTH; i += 3) {
        // Combine three input bytes into a 24-bit value
        value =
            ((uint32_t)src[i] << 16) | ((uint32_t)src[i + 1] << 8) | src[i + 2];
        // Encode the value into 4 base64 characters
        for (uint8_t j = 0; j < 4; j++) {
            *dst++ = BASE64_TABLE[(value >> (18 - j * 6)) & 0x3F];
        }
    }
    // BIP85_ENTROPY_LENGTH is not a multiple of 3: the final group holds a
    // single byte, zero-padded, followed by two '=' padding characters.
    value = (uint32_t)src[i] << 16;
    *dst++ = BASE64_TABLE[(value >> 18) & 0x3F];
    *dst++ = BASE64_TABLE[(value >> 12) & 0x3F];
    *dst++ = '=';
    *dst++ = '=';

    return dst - dst_start;  // Return the total number of characters written
}
