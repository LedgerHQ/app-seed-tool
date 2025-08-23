/*******************************************************************************
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

#pragma once

#if defined(SCREEN_SIZE_WALLET)

enum __attribute__((packed)) bip85_app_type {
    BIP85_APP_BIP39,
    BIP85_APP_PWD_BASE64,
    BIP85_APP_PWD_BASE85
};

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
uint8_t* bip85_app_pwd_base64_gen(void);

/*
 * Generate base85 password and return pointer to password
 */
uint8_t* bip85_app_pwd_base85_gen(void);
#endif  // SCREEN_SIZE_WALLET
