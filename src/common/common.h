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

#define ALPHABET_LENGTH 27
#define KBD_LETTERS     "qwertyuiopasdfghjklzxcvbnm"

#define memzero(...) explicit_bzero(__VA_ARGS__)

#ifndef SPRINTF
// avoid typing the size each time
#define SPRINTF(strbuf, ...) snprintf((char *) (strbuf), sizeof(strbuf), __VA_ARGS__)
#endif

/*
 * Compares the entered secret with the device's seed.
 *
 * `reconstructed` is set to false when the input could not be turned into a
 * mnemonic at all (SSKR shards that pass their CRC but cannot be combined).
 * That is a distinct outcome from a seed mismatch and must not be reported as
 * one. It is always true for the BIP39 tool, which has nothing to reconstruct.
 */
bool compare_recovery_phrase(bool *reconstructed);

/*
 * The tail of compare_recovery_phrase(): everything that happens once
 * os_derive_bip32_no_throw() has returned. Split out because that syscall is
 * not available on host while the comparison and the two erasures around it are
 * pure logic. Not for callers outside common_seed.c -- it is here so that the
 * file that defines it, and the unit suite that drives it, are checked against
 * one declaration.
 */
bool compare_recovery_phrase_finish(cx_err_t derivation_status,
                                    uint8_t buffer[64],
                                    uint8_t buffer_device[64]);
