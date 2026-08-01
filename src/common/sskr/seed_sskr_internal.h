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

// Internals of seed_sskr.c: helpers that no other file in src/ calls, but that
// keep external linkage so the unit suite can reach them. Same pattern, and for
// the same reason, as bip85/bip85_internal.h: the single place they are
// declared, included by seed_sskr.c so the compiler checks the definitions, and
// by the tests so it checks their use.
//
// Before this header, the two entry points below were declared by hand, with
// `extern`, in each of the three test files that needed them, and nothing
// checked those declarations against the definitions: no translation unit saw
// both. They happened to agree, which is not the same as being kept in
// agreement.
//
// Nothing here is part of the application's SSKR interface. That is
// common_sskr.h, and it is what callers outside this directory should use.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "./group.h"

// Expand the flat group descriptor the UI layers hold into the array of
// sskr_group_descriptor_t that sskr_count_shards() and sskr_generate_shards()
// take.
//
// `group_descriptor` points at the first element of an `unsigned int[N][2]` --
// nbgl/sskr_shares.c and bagl/ux_nano.h both declare it that way and pass
// `[0]` -- so group `i` holds its threshold at `[i * 2]` and its member count
// at `[i * 2 + 1]`.
//
// Returns false, having written nothing, when `groups_len` exceeds
// `groups_capacity`: `groups` is a fixed-size array in both callers, while the
// length reaching them comes from outside.
bool bolos_ux_sskr_groups_from_descriptor(const unsigned int *group_descriptor,
                                          uint8_t groups_len,
                                          sskr_group_descriptor_t *groups,
                                          size_t groups_capacity);

// The two SSKR generation entry points. Both are reached only from
// bolos_ux_bip39_to_sskr_convert() in the same file, which is why neither is in
// common_sskr.h; the unit suite calls them directly.
//
// bolos_ux_sskr_size_get() returns the number of shares the descriptor calls
// for, or one of sskr_count_shards()'s negative error codes, and writes the
// serialized length of one share to *share_len. bolos_ux_sskr_generate()
// returns the number of shares written to share_buffer, or 0 on any failure,
// having first erased the whole of share_buffer -- so share_buffer_len is a
// write length and not merely a capacity. The share_len_expected and
// share_count_expected it is given are the figures bolos_ux_sskr_size_get()
// produced for the same descriptor, and it refuses to report success unless
// what it generated matches them.
int16_t bolos_ux_sskr_size_get(uint8_t bip39_type,
                               uint8_t groups_threshold,
                               unsigned int *group_descriptor,
                               uint8_t groups_len,
                               uint8_t *share_len);

unsigned int bolos_ux_sskr_generate(uint8_t groups_threshold,
                                    unsigned int *group_descriptor,
                                    uint8_t groups_len,
                                    unsigned char *seed,
                                    unsigned int seed_len,
                                    uint8_t *share_len,
                                    unsigned char *share_buffer,
                                    unsigned int share_buffer_len,
                                    uint8_t share_len_expected,
                                    int16_t share_count_expected);
