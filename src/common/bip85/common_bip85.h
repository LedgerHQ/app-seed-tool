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

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// BIP85 applications
#include "./seed_rom_variables.h"

/**
 * @brief Generates BIP85 BIP39 mnemonic words using a specified BIP32 path.
 *
 * @details This function derives a root key from the device's seed using a BIP32 path of the form:
 *          m / purpose' / app_no' / language' / words' / index'.
 *          It then calculates BIP85 entropy from this root key and converts it to BIP39 mnemonic
 *          words using the specified language and number of words.
 *
 * @param[out] hex_out  Pointer to a buffer where the generated BIP85 BIP39 mnemonic words in
 *                      hexadecimal format will be stored.
 * @param[in]  language Language code for the mnemonic words.
 * @param[in]  words    Number of mnemonic words to generate.
 * @param[in]  index    Index to be used in the BIP32 path.
 *
 * @return The number of bytes written to the output buffer.
 */
uint8_t bolos_ux_bip85_bip39(uint8_t *hex_out, uint8_t language, uint8_t words, unsigned int index);

/**
 * @brief Generates BIP85 HEX output using a specified BIP32 path.
 *
 * @details This function derives a root key from the device's seed using a BIP32 path of the form:
 *          m / purpose' / app_no' / num_bytes' / index'.
 *          It then calculates BIP85 entropy from this root key and returns the first `num_bytes` as
 *          hexadecimal output.
 *
 * @param[out] hex_out   Pointer to a buffer where the generated BIP85 HEX output will be stored.
 * @param[in]  num_bytes Number of bytes to generate.
 * @param[in]  index     Index to be used in the BIP32 path.
 */
void bolos_ux_bip85_hex(uint8_t *hex_out, uint8_t num_bytes, unsigned int index);

/**
 * @brief Generates BIP85 Base64 password using a specified BIP32 path.
 *
 * @details This function derives a root key from the device's seed using a BIP32 path of the form:
 *          m / purpose' / app_no' / pwd_len' / index'.
 *          It then calculates BIP85 entropy from this root key and encodes it to Base64, truncating
 *          to the specified password length.
 *
 * @param[out] pwd     Pointer to a buffer where the generated BIP85 Base64 password will be stored.
 * @param[in]  pwd_len Length of the password in bytes.
 * @param[in]  index   Index to be used in the BIP32 path.
 */
uint8_t bolos_ux_bip85_pwd_base64(char *pwd, uint8_t pwd_len, unsigned int index);

/**
 * @brief Generates BIP85 Base85 password using a specified BIP32 path.
 *
 * @details This function derives a root key from the device's seed using a BIP32 path of the form:
 *          m / purpose' / app_no' / pwd_len' / index'.
 *          It then calculates BIP85 entropy from this root key and encodes it to Base85, truncating
 *          to the specified password length.
 *
 * @param[out] pwd     Pointer to a buffer where the generated BIP85 Base85 password will be stored.
 * @param[in]  pwd_len Length of the password in bytes.
 * @param[in]  index   Index to be used in the BIP32 path.
 *
 * @return The number of bytes written to the output buffer.
 */
uint8_t bolos_ux_bip85_pwd_base85(char *pwd, uint8_t pwd_len, unsigned int index);

/**
 * @brief Generates a series of random dice rolls using BIP85.
 *
 * @details This function simulates the rolling of dice with the specified number of sides and
 *          rolls. It utilizes the BIP85 standard to ensure cryptographic security and randomness.
 *
 *          The DRNG output is a finite SHAKE256 digest: rejection sampling on it is not
 *          guaranteed to produce `rolls` valid values from a single digest, so this function
 *          re-extends the digest (re-deriving a longer one from the same seed, not resuming
 *          mid-stream) a bounded number of times before giving up. It always reports how many
 *          rolls it actually produced rather than silently returning fewer than requested.
 *
 * @param[out] out          Pointer to an array of `uint32_t` to store the generated dice rolls.
 * @param[in]  out_capacity Capacity of `out`, in elements.
 * @param[in]  sides        Number of sides on each die (must be between 2 and UINT32_MAX >> 1).
 * @param[in]  rolls        Number of dice rolls to generate (must be between 1 and UINT32_MAX >>
 * 1).
 * @param[in]  index        Index to be used in the BIP32 path.
 *
 * @return `rolls` on success. A negative value if `out_capacity < rolls`, if entropy derivation
 *         or the SHAKE256 call failed, or if the DRNG stream could not be extended far enough to
 *         produce `rolls` valid results.
 */
int32_t bolos_ux_bip85_dice(uint32_t *out,
                            size_t out_capacity,
                            uint32_t sides,
                            uint32_t rolls,
                            uint32_t index);

/*
 * The derivation path, written the way the specification writes it.
 *
 * A BIP-85 result is only worth anything if it can be reproduced somewhere
 * else -- that is the whole point of deriving rather than storing. Reproducing
 * it needs the path, and no screen in this application has ever shown one.
 *
 * The longest path this application builds has five components, each a 32-bit
 * value whose hardening bit is stripped before printing, so at most ten digits
 * and an apostrophe apiece, after a leading "m".
 */
#define BIP85_PATH_STRING_MAX_LENGTH (1 + 5 * (1 + 10 + 1) + 1)

/**
 * @brief Renders a built BIP-85 path as "m/83696968'/39'/0'/24'/42'".
 *
 * @details Takes the component array a `bip85_path_*()` builder filled rather
 *          than the values a screen collected, so what is displayed is what
 *          was built. Every component of a BIP-85 path is hardened; the
 *          hardening bit is removed for display and reported by the trailing
 *          apostrophe, exactly as BIP-32 notation does.
 *
 *          Does not use snprintf(): its return value is unusable on one of the
 *          SDKs this repository targets (nanos returns 0 from every exit), and
 *          a bound that cannot be checked is not a bound.
 *
 * @param[in]  path     Components, as written by a `bip85_path_*()` builder.
 * @param[in]  path_len Number of components in `path`.
 * @param[out] out      Destination, always null-terminated on success.
 * @param[in]  out_len  Capacity of `out`, in bytes.
 *
 * @return true if the whole path was written. false, with `out` set to the
 *         empty string, if it would not fit or if `path_len` is 0 -- a caller
 *         must never display a path that has been silently cut short, since a
 *         truncated path is a wrong path rather than an incomplete one.
 */
bool bip85_path_format(const unsigned int *path, unsigned int path_len, char *out, size_t out_len);

/**
 * @brief Renders the path `bolos_ux_bip85_bip39()` derives over, for the same
 *        arguments.
 *
 * @return What bip85_path_format() returned.
 */
bool bolos_ux_bip85_bip39_path_format(uint8_t language,
                                      uint8_t words,
                                      unsigned int index,
                                      char *out,
                                      size_t out_len);

/**
 * @brief Renders the path `bolos_ux_bip85_pwd_base64()` derives over, for the
 *        same arguments.
 */
bool bolos_ux_bip85_pwd_base64_path_format(uint8_t pwd_len,
                                           unsigned int index,
                                           char *out,
                                           size_t out_len);

/**
 * @brief Renders the path `bolos_ux_bip85_pwd_base85()` derives over, for the
 *        same arguments.
 */
bool bolos_ux_bip85_pwd_base85_path_format(uint8_t pwd_len,
                                           unsigned int index,
                                           char *out,
                                           size_t out_len);
