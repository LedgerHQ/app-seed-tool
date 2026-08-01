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

#include "./common_sskr.h"

// See common_sskr.h for the contract. This is the arithmetic the three word
// entry paths -- nbgl/sskr_shares.c, bagl/nanox_enter_phrase.c and
// bagl/nanos_enter_phrase.c -- each used to carry a copy of.
void bolos_ux_sskr_entry_header_update(const uint8_t* buffer, size_t index,
                                       size_t word_number, size_t* final_size,
                                       uint8_t* count) {
    switch (word_number) {
        // 4th byte of CBOR header contains number of data bytes to follow
        case 3:
            if ((buffer[index] & 0x1F) <= 24) {
                // SSKR bytes = 4 bytes CBOR + n bytes share + 4 bytes CRC
                // checksum. This is only a literal length for 0-23; 24 (one
                // length byte follows) is corrected below once that byte is
                // read.
                *final_size = 4 + (buffer[index] & 0x1F) + sizeof(uint32_t);
            } else {
                // 25-31 are reserved CBOR additional-info values (two/four/
                // eight-byte length, reserved, or indefinite length) with no
                // literal length of their own -- there is nothing valid to
                // compute here. Force the same maximum bound used below for an
                // out-of-range declared length, so entry can still complete
                // and bolos_ux_sskr_hex_check() rejects it, instead of
                // treating the reserved value as if it encoded a
                // 25-to-31-byte payload.
                *final_size = SSKR_SHARE_MAX_WIRE_LENGTH;
            }
            break;
        case 4:
            if ((buffer[3] & 0x1F) == 24) {
                // The declared length is read as the unsigned wire byte it is:
                // callers hold the entered bytes in a `char` buffer, and
                // `char` is signed on the host that builds the unit tests and
                // unsigned on the device.
                *final_size = 4 + 1 + buffer[index] + sizeof(uint32_t);
                // A serialized shard is SSKR_METADATA_LENGTH_BYTES plus a
                // value of at most SSKR_MAX_STRENGTH_BYTES, so no share can be
                // longer than this on the wire.
                if (*final_size > SSKR_SHARE_MAX_WIRE_LENGTH) {
                    *final_size = SSKR_SHARE_MAX_WIRE_LENGTH;
                }
            }
            PRINTF("SSKR number of words: %u\n", (unsigned int)*final_size);
            break;
        // 8th byte of SSKR phrase contains member-threshold
        case 7:
            if ((buffer[3] & 0x1F) < 24) {
                *count = (buffer[index] & 0x0F) + 1;
            }
            break;
        case 8:
            if ((buffer[3] & 0x1F) == 24) {
                *count = (buffer[index] & 0x0F) + 1;
            }
            PRINTF("SSKR member threshold: %u\n", (unsigned int)*count);
            break;
    }
}
