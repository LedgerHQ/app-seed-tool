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

#pragma once

#define ONBOARDING_WORD_COMPLETION_MAX_ITEMS 8
#define BOLOS_UX_HASH_LENGTH                 4  // as on the blue

#define BIP39_MAX_WORD_LENGTH         8
#define SSKR_MAX_NUMBER_LENGTH        2
#define BIP85_INDEX_MAX_NUMBER_LENGTH 7

enum __attribute__((packed)) {
    BIP39_MNEMONIC_SIZE_12 = 12,
    BIP39_MNEMONIC_SIZE_18 = 18,
    BIP39_MNEMONIC_SIZE_24 = 24,
};

/*
 * What the user typed, and therefore which buffer holds it -- not what the
 * user came to do. The menu has four entries and this has three values,
 * because two of those entries, checking a recovery phrase and backing one
 * up, both enter a BIP-39 phrase and are the same thing to everything that
 * reads this. user_intent below is what tells them apart.
 *
 * Adding a value here is not a display change, and the compiler will not stop
 * it. compare_recovery_phrase() (src/common/common_seed.c) dispatches on this
 * to choose which buffer to derive the seed from; a fourth value falls
 * through both of its branches, hands 64 zero bytes to the comparison, and
 * reports every phrase as not matching the device -- with nothing on screen
 * to say the derivation never ran.
 */
enum __attribute__((packed)) { TOOL_TYPE_BIP39, TOOL_TYPE_SSKR, TOOL_TYPE_BIP85 };

/*
 * What the user came to do -- one value per entry of the menu.
 *
 * This exists because the same screens serve several of them. Entering a
 * BIP-39 phrase is a destination when checking one and a step on the way when
 * backing one up, so the verdict screen does not say the same thing in the
 * two, and does not go to the same place afterwards. Before this, the flow
 * was inferred from the tool, which could not tell those two apart.
 *
 * USER_INTENT_NB is the size of the enumeration, not a value. Tables in
 * src/nbgl/ui.c are sized on it and static-assert against it, so a fifth
 * intention is a compile error at the table that has to gain a row rather
 * than a row silently left empty.
 */
typedef enum __attribute__((packed)) user_intent_e {
    USER_INTENT_CHECK = 0,
    USER_INTENT_BACKUP,
    USER_INTENT_RECOVER,
    USER_INTENT_DERIVE,
    USER_INTENT_NB
} user_intent_e;

// State of the dynamic display
enum __attribute__((packed)) { STATIC_SCREEN, DYNAMIC_SCREEN };

#define KEYBOARD_ITEM_VALIDATED \
    1  // callback is called with the entered item index, tmp_element is precharged with element to
       // be displayed and using the common string buffer as string parameter
#define KEYBOARD_RENDER_ITEM \
    2  // callback is called with the element index, tmp_element is precharged with element to be
       // displayed and using the common string buffer as string parameter
#define KEYBOARD_RENDER_WORD \
    3  // callback is called with a -1 when requesting complete word, or the char index else,
       // returning 0 implies no char is to be displayed

#define RESTORE_WORD_ACTION_REENTER_WORD 0
#define RESTORE_WORD_ACTION_FIRST_WORD   1

#define COMMON_KEYBOARD_INDEX_UNCHANGED (-1UL)

#define BIP85_DRNG_MAX_DIGEST_SIZE 256
