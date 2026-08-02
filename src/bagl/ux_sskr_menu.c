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

#include "./ux_sskr_menu.h"

// os.h is here for bolos_target.h, which is where TARGET_NANOS comes from on
// the device -- the application build passes no -DTARGET_NANOS of its own.
// sss-constants.h below is guarded on it too, so a translation unit that saw
// one without the other would size the table for one device and the bound
// that has to hold it for another. It has to be included before both.
#include <os.h>

#include "../common/sskr/sss/sss-constants.h"

static const char* const sskr_descriptor_values[] = {
    "1",  "2", "3",  "4",  "5",  "6",  "7",
#ifndef TARGET_NANOS
    "8",  "9", "10", "11", "12", "13", "14",
    "15", "16"
#endif
};

#define SSKR_DESCRIPTOR_COUNT \
    (sizeof(sskr_descriptor_values) / sizeof(sskr_descriptor_values[0]))

// The share-count menu offers 1..SSKR_DESCRIPTOR_COUNT, and every share the
// user asks for is one the Shamir layer then has to produce -- where
// sss_validate_parameters() refuses any count above SSS_MAX_SHARE_COUNT.
// Nothing else ties the two together: the table is guarded on TARGET_NANOS
// here, SSS_MAX_SHARE_COUNT is guarded on TARGET_NANOS in sss-constants.h,
// and the second configuration has no slack at all (16 and 16).
_Static_assert(SSKR_DESCRIPTOR_COUNT <= SSS_MAX_SHARE_COUNT,
               "the SSKR share-count menu offers more shares than "
               "SSS_MAX_SHARE_COUNT lets sss_split_secret() produce");

unsigned int sskr_descriptor_count(void) { return SSKR_DESCRIPTOR_COUNT; }

const char* sskr_descriptor_label(unsigned int idx, unsigned int count) {
    if (idx < count) {
        return sskr_descriptor_values[idx];
    }
    return NULL;
}
