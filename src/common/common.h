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

#define ALPHABET_LENGTH 27
#define KBD_LETTERS     "qwertyuiopasdfghjklzxcvbnm"

#define memzero(...) explicit_bzero(__VA_ARGS__)

#ifndef SPRINTF
// avoid typing the size each time
#define SPRINTF(strbuf, ...) snprintf((char *) (strbuf), sizeof(strbuf), __VA_ARGS__)
#endif

/*
 * The two values the seed comparison speaks in, and the reason it does not
 * speak in a bool.
 *
 * This is the one decision in the application worth injecting a fault into:
 * a false "no match" sends the user back to the entry screen, but a false
 * "match" sends them away with a backup that will not open their device, and
 * they find out when they need it. Compiled from a bool, that decision was two
 * ARM instructions wide and the whole of it lived in bit 0 of one register --
 * a single bit flip, in the direction that costs the funds.
 *
 * These two differ in all 32 bits, so no single bit flip on the path from
 * compare_recovery_phrase_finish() to the screen turns one into the other,
 * and neither is a value a register arrives at by accident: not zero, not one,
 * not a small count, not an address in this application's map.
 *
 * Callers must test for VERDICT_MATCH and treat *everything* else as a
 * mismatch, VERDICT_NO_MATCH included but not only -- a verdict that is
 * neither is a corrupted one, and a corrupted verdict is not a match.
 */
#define VERDICT_MATCH    0xA5C3F00Du
#define VERDICT_NO_MATCH 0x5A3C0FF2u

/*
 * Compares the entered secret with the device's seed. Returns VERDICT_MATCH
 * and nothing else on agreement; see above.
 *
 * `reconstructed` is set to false when the input could not be turned into a
 * mnemonic at all (SSKR shards that pass their CRC but cannot be combined).
 * That is a distinct outcome from a seed mismatch and must not be reported as
 * one. It is always true for the BIP39 tool, which has nothing to reconstruct.
 */
unsigned int compare_recovery_phrase(bool *reconstructed);

/*
 * The tail of compare_recovery_phrase(): everything that happens once
 * os_derive_bip32_no_throw() has returned. Split out because that syscall is
 * not available on host while the comparison and the two erasures around it are
 * pure logic. Not for callers outside common_seed.c -- it is here so that the
 * file that defines it, and the unit suite that drives it, are checked against
 * one declaration.
 */
unsigned int compare_recovery_phrase_finish(cx_err_t derivation_status,
                                            uint8_t buffer[64],
                                            uint8_t buffer_device[64]);
