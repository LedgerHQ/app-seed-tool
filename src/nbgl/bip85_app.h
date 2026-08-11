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

#if defined(SCREEN_SIZE_WALLET)

/*
 * The BIP-85 applications this interface derives, named as the specification
 * names them.
 *
 * BIP85_APP_DICE is the primitive -- `m/83696968'/89101'/sides'/rolls'/index'`
 * -- and not the use it is put to. A PIN is not an application of its own in
 * BIP-85 and must not become one here: it is DICE with a ten-sided die and
 * one roll per digit, which is exactly why a PIN derived on this device can
 * be derived again by any other implementation of the specification. What the
 * derivation is being asked for is a separate question, and has a separate
 * field: see `enum bip85_dice_use` below.
 */
enum __attribute__((packed)) bip85_app_type {
    BIP85_APP_BIP39,
    BIP85_APP_PWD_BASE64,
    BIP85_APP_PWD_BASE85,
    BIP85_APP_DICE
};

/*
 * What a DICE derivation is being used for, which is what the screens name.
 *
 * One value today, and the enumeration exists for the second one: a generic
 * DICE journey -- any number of sides, any number of rolls, displayed as
 * rolls -- is the obvious next entry, and it would share every line of the
 * derivation with this one while sharing none of the wording. Storing the use
 * rather than inferring it from `sides == 10` is what keeps "which secret was
 * asked for" a fact the flow recorded instead of a guess a label makes.
 */
enum __attribute__((packed)) bip85_dice_use { BIP85_DICE_USE_PIN, BIP85_DICE_USE_NB };

/*
 * The PIN preset, in one place.
 *
 * PIN(length, index) = DICE(sides = 10, rolls = length, index = index). The
 * ten sides are what make each roll a decimal digit; changing this number
 * does not produce a PIN in another alphabet, it produces a different secret
 * from a different derivation path.
 *
 * The digit counts offered are 4, 6 and 8, and the maximum below is what
 * every buffer holding a PIN is sized from.
 */
#define BIP85_DICE_PIN_SIDES      10
#define BIP85_DICE_PIN_DIGITS_MIN 4
#define BIP85_DICE_PIN_DIGITS_MAX 8

/*
 * Sets the length of the data in the app buffer
 */
void bip85_length_set(const uint8_t length);

/*
 * Returns the length of the data in the app buffer
 */
uint8_t bip85_length_get(void);

/*
 * Sets the BIP85 app type we are using
 */
void bip85_type_set(const uint8_t type);

/*
 * Returns the BIP85 app type we are using
 */
uint8_t bip85_type_get(void);

/*
 * Sets the BIP85 derivation path index
 */
void bip85_index_set(const uint32_t index);

/*
 * Returns the BIP85 derivation path index
 */
uint32_t bip85_index_get(void);

/*
 * Sets the number of dice rolls a DICE derivation asks for, which for the PIN
 * preset is the number of digits.
 *
 * Its own field rather than the length above, and deliberately: `length` is
 * how much data the app buffer holds, and it is also what the password
 * screens collect. A roll count stored there would be a third meaning for one
 * variable -- words, characters, rolls -- read by whichever screen happened to
 * ask, and the review announces a derivation path built from it.
 */
void bip85_dice_rolls_set(const uint8_t rolls);

/*
 * Returns the number of dice rolls a DICE derivation asks for
 */
uint8_t bip85_dice_rolls_get(void);

/*
 * Sets what the DICE derivation is being used for (see enum bip85_dice_use)
 */
void bip85_dice_use_set(const uint8_t use);

/*
 * Returns what the DICE derivation is being used for
 */
uint8_t bip85_dice_use_get(void);

/*
 * Erase all BIP85 app information
 */
void bip85_app_reset(void);

/*
 * Generate BIP39 phrase
 */
void bip85_app_bip39_gen(void);

/*
 * Sets the BIP85 child password length
 */
void bip85_password_length_set(const uint8_t length);

/*
 * Gets the BIP85 child password length
 */
uint8_t bip85_get_get();

/*
 * Generate base64 password and return pointer to password
 */
uint8_t *bip85_app_pwd_base64_gen(void);

/*
 * Generate base85 password and return pointer to password
 */
uint8_t *bip85_app_pwd_base85_gen(void);

/*
 * Derives the PIN preset of DICE and returns the digits, null-terminated.
 *
 * Returns NULL, having erased everything it touched, if the derivation
 * produced anything other than exactly the number of rolls asked for or if
 * the digits could not be rendered. The caller must display nothing in that
 * case: a PIN missing a digit is a different PIN, not a shorter one, and the
 * user cannot tell the two apart on screen.
 */
const char *bip85_app_pin_gen(void);

/*
 * Writes the BIP-85 derivation path of the currently selected application,
 * with its currently entered index and length, as "m/83696968'/39'/0'/24'/42'".
 *
 * Dispatches to the formatter that sits beside the matching derivation in
 * src/common/bip85/seed_bip85.c, with the same arguments the generator above
 * will be called with -- so the review cannot announce a path the derivation
 * does not take.
 *
 * Returns false, and sets `out` to the empty string, if the path does not fit
 * or if no application has been selected. A caller must show nothing rather
 * than a path that has been cut short.
 */
bool bip85_app_path_format(char *out, size_t out_len);
#endif  // SCREEN_SIZE_WALLET
