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
 * @brief Encodes 64 bytes of data into a Base85 string.
 *
 * @param[in]  src Pointer to the input data.
 * @param[out] dst Pointer to the output buffer.
 *
 * @return The number of bytes written to the output buffer.
 */
uint8_t base85_encode_64bytes(const uint8_t *src, char *dst) {
    const uint8_t *src_end = src + BIP85_ENTROPY_LENGTH;  // Mark the end of the source array
    char *dst_start = dst;  // Save the starting address of the destination array
    uint32_t value;

    while (src < src_end) {
        // Load 4 bytes into a 32-bit value
        value = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];

        // Convert the value into 5 base85 characters
        for (int8_t i = 4; i >= 0; i--) {
            dst[i] = BASE85_TABLE[value % BASE85_TABLE_LENGTH];  // Select character
            value /= BASE85_TABLE_LENGTH;  // Divide by table length for next digit
        }

        dst += 5;  // Advance destination pointer
        src += 4;  // Advance source pointer
    }

    // Return the total number of characters written
    return dst - dst_start;
}
