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

// See common_sskr.h for the contract. This is the arithmetic both share
// display paths -- bagl/ux_sskr.c and nbgl/ui.c -- each carried a copy of.
bool bolos_ux_sskr_share_slice(size_t buffer_length, uint8_t share_count,
                               uint8_t share_index, size_t* offset,
                               size_t* length) {
    // One bound covers both things that can go wrong. An index past the last
    // share is not a share; and a share count of zero, which is what would
    // make the division below undefined, is a count no index can be below --
    // so it leaves through the same door. Spelling the zero out separately
    // adds a condition no input can reach, and no test can hold.
    if (share_index >= share_count) {
        return false;
    }

    // Multiply before dividing, which is what both call sites did: `index *
    // buffer_length / share_count` groups left to right in C. The two forms
    // only differ when the buffer is not an exact multiple of the share
    // count, which never happens for a generated set -- every share of a set
    // has the same length -- but preserving the order keeps this a move
    // rather than a change.
    *offset = ((size_t)share_index * buffer_length) / share_count;
    *length = buffer_length / share_count;
    return true;
}
