/*******************************************************************************
 *   (c) 2016-2022 Ledger SAS
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

#endif  // SCREEN_SIZE_WALLET
