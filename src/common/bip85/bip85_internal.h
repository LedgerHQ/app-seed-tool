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

// Internals of this directory: helpers that the application reaches only from
// within it, but that keep external linkage so the unit suite can reach them
// too. That intent is already written on bip85_dice_bits_per_roll() -- "with
// external linkage, purely so it can be linked into a unit test" -- so making
// them static is not the answer; giving them a prototype is.
//
// Before this header, each of them was declared by hand, with `extern`, in
// whichever test file needed it, and nothing checked those declarations
// against the definitions: no translation unit saw both. This header is the
// single place they are declared, included by the file that defines them so
// the compiler checks the definitions, and by the tests so it checks their
// use.
//
// Nothing here is part of the application's BIP-85 interface. That is
// common_bip85.h, and it is what callers outside this directory should use.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "./seed_rom_variables.h"

// Derivation-path builders. Each fills `path` with the BIP-32 path of the
// matching BIP-85 application and returns the number of components written.
unsigned int bip85_path_drng(unsigned int *path, unsigned int index);
unsigned int bip85_path_bip39(unsigned int *path,
                              uint8_t language,
                              uint8_t words,
                              unsigned int index);
unsigned int bip85_path_hex(unsigned int *path, uint8_t num_bytes, unsigned int index);
unsigned int bip85_path_pwd_base64(unsigned int *path, uint8_t pwd_len, unsigned int index);
unsigned int bip85_path_pwd_base85(unsigned int *path, uint8_t pwd_len, unsigned int index);
unsigned int bip85_path_dice(unsigned int *path,
                             uint32_t sides,
                             uint32_t rolls,
                             unsigned int index);

// Parameter validation, one per application.
bool bip85_bip39_words_valid(uint8_t words);
bool bip85_hex_num_bytes_valid(uint8_t num_bytes);
bool bip85_pwd_base64_len_valid(uint8_t pwd_len);
bool bip85_pwd_base85_len_valid(uint8_t pwd_len);
bool bip85_dice_sides_valid(uint32_t sides);
bool bip85_dice_rolls_valid(uint32_t rolls);

// Entropy and output shaping.
uint8_t bip85_bip39_entropy_len(uint8_t words);
bool bip85_entropy_from_key(const uint8_t key[32], uint8_t *out, size_t out_len);
bool bolos_ux_bip85_drng_with_seed(uint8_t *seed,
                                   size_t seed_length,
                                   uint8_t *digest,
                                   size_t digest_length);
uint8_t bip85_finalize_pwd(const char *buffer_pwd, char *pwd, uint8_t pwd_len);

// DICE.
uint8_t bip85_dice_bits_per_roll(uint32_t sides);
int32_t bip85_dice_roll(uint32_t *out,
                        size_t out_capacity,
                        uint32_t sides,
                        uint32_t rolls,
                        const uint8_t seed[BIP85_ENTROPY_LENGTH]);

// The password encoders, defined in base64.c and base85.c. Each consumes
// exactly 64 bytes from `src` and writes BASE64_ENCODE_LENGTH /
// BASE85_ENCODE_LENGTH characters to `dst`, unterminated, returning that count.
// bip85_finalize_pwd() above is what truncates and terminates the result.
uint8_t base64_encode_64bytes(const uint8_t *src, char *dst);
uint8_t base85_encode_64bytes(const uint8_t *src, char *dst);
