/*******************************************************************************
 *   Ledger Seed Tool application
 *   (c) 2016-2025 Ledger SAS
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
uint8_t base64_encode_64bytes(const uint8_t *src, char *dst) {
    const uint8_t *src_end = src + BIP85_ENTROPY_LENGTH;
    char *dst_start = dst;
    uint32_t value;

    // Loop through the input in chunks of 3 bytes
    while (src < src_end) {
        // Combine three input bytes into a 24-bit value
        value = (src[0] << 16) | (src[1] << 8) | src[2];

        // Encode the value into 4 base64 characters
        for (uint8_t i = 0; i < 4; i++) {
            *dst++ = BASE64_TABLE[(value >> (18 - i * 6)) & 0x3F];
        }

        src += 3;  // Advance source pointer by 3 bytes
    }

    // Add the fixed padding (for 64-byte input)
    *(dst - 1) = '=';
    *(dst - 2) = '=';

    return dst - dst_start;  // Return the total number of characters written
}
