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

#include "../common/bip85/common_bip85.h"
#include "../common/common.h"
#include "./bip39_mnemonic.h"
// Its own header, so the compiler checks these definitions against the
// declarations rather than only their callers doing so.
#include "./bip85_app.h"

#if defined(SCREEN_SIZE_WALLET)

typedef struct bip85_buffer_struct {
    // buffer to hold BIP85 app output
    uint8_t buffer[BASE64_ENCODE_LENGTH];
    uint8_t length;
    // type of BIP85 app we are using
    uint8_t type;
    // BIP85 derivation path index
    unsigned int index;
} bip85_buffer_t;

static bip85_buffer_t app_data = {0};

void bip85_length_set(uint8_t length) { app_data.length = length; }

uint8_t bip85_length_get(void) { return app_data.length; }

void bip85_type_set(const uint8_t type) { app_data.type = type; }

uint8_t bip85_type_get(void) { return app_data.type; }

void bip85_index_set(const uint32_t index) { app_data.index = index; }

uint32_t bip85_index_get(void) { return app_data.index; }

void bip85_app_reset(void) { memzero(&app_data, sizeof(app_data)); }

/*
 * The arguments every BIP-85 derivation in this application is made with.
 *
 * These exist as functions, and not as expressions repeated at each call site,
 * because the review screen added alongside them displays the derivation path
 * built from exactly these values *before* the derivation runs. Two readings
 * of "how many words" or "which length" is precisely how a review comes to
 * promise a path that the derivation does not then take -- and a BIP-85 path
 * that is off by one component still derives a perfectly well-formed secret,
 * just not the one that was announced.
 */
#define BIP85_LANGUAGE_ENGLISH 0

static uint8_t bip85_app_bip39_words(void) {
    return bip39_mnemonic_final_size_get();
}

bool bip85_app_path_format(char* out, size_t out_len) {
    // Switch without a default, on the same enumeration the generators below
    // dispatch on and in the same order: a fourth application has to say here
    // what path it derives over, rather than inheriting a blank one. Each arm
    // calls the formatter that lives beside the derivation it pairs with, so
    // the builder and its arguments are chosen once per application rather
    // than once per purpose.
    switch ((enum bip85_app_type)app_data.type) {
        case BIP85_APP_BIP39:
            return bolos_ux_bip85_bip39_path_format(
                BIP85_LANGUAGE_ENGLISH, bip85_app_bip39_words(), app_data.index,
                out, out_len);
        case BIP85_APP_PWD_BASE64:
            return bolos_ux_bip85_pwd_base64_path_format(
                app_data.length, app_data.index, out, out_len);
        case BIP85_APP_PWD_BASE85:
            return bolos_ux_bip85_pwd_base85_path_format(
                app_data.length, app_data.index, out, out_len);
    }
    if (out != NULL && out_len > 0) {
        out[0] = '\0';
    }
    return false;
}

void bip85_app_bip39_gen(void) {
    app_data.length =
        bolos_ux_bip85_bip39(app_data.buffer, BIP85_LANGUAGE_ENGLISH,
                             bip85_app_bip39_words(), app_data.index);
    bip39_mnemonic_encode(app_data.buffer, app_data.length);
}

uint8_t* bip85_app_pwd_base64_gen(void) {
    app_data.length = bolos_ux_bip85_pwd_base64(
        (char*)app_data.buffer, app_data.length, app_data.index);
    return app_data.buffer;
}
uint8_t* bip85_app_pwd_base85_gen(void) {
    app_data.length = bolos_ux_bip85_pwd_base85(
        (char*)app_data.buffer, app_data.length, app_data.index);
    return app_data.buffer;
}
#endif
