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

// SSKR helpers
#include "./seed_rom_variables.h"
#include "./sskr-constants.h"

// Largest a serialized share can be on the wire, in bytes: the CBOR long-form
// byte-string header (3-byte tag + 0x58 + one length byte), the shard itself
// (SSKR_METADATA_LENGTH_BYTES plus a value of at most SSKR_MAX_STRENGTH_BYTES)
// and the CRC32. Nothing longer can be a share, whatever its header declares.
#define SSKR_SHARE_MAX_WIRE_LENGTH (5 + SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES + 4)

// Byte 3 of a share on the wire is a CBOR initial byte: three bits of major
// type, five of additional information (RFC 8949 section 3). BCR-2020-011
// wraps a shard as a byte string, so the only major type a share can carry
// there is 2, and the initial byte of a 21-byte shard is 0x55 -- 010 10101.
//
// Everywhere that byte is read, the additional-information field is taken out
// of it with `& 0x1F`. This is the other half of the same byte, kept here in
// one place because it is read from two files and four functions, and a rule
// that held on some of them and not the others would be worse than no rule:
// the entry path would size a frame the input check then refuses, or the other
// way round.
#define SSKR_CBOR_MAJOR_TYPE_MASK     0xE0
#define SSKR_CBOR_MAJOR_TYPE_BYTE_STR 0x40
#define SSKR_CBOR_IS_BYTE_STRING(byte) \
    (((byte) & SSKR_CBOR_MAJOR_TYPE_MASK) == SSKR_CBOR_MAJOR_TYPE_BYTE_STR)

// Read what the CBOR header of a share being entered says about that share,
// one entered byte at a time.
//
// The UI layers append one byte per ByteWord to a buffer and, at four fixed
// positions in the share, take the expected total length and the share count
// out of the bytes just entered. This is that arithmetic, in one place: the
// three entry paths (nbgl/sskr_shares.c, bagl/nanox_enter_phrase.c and
// bagl/nanos_enter_phrase.c) all called it, in three copies of the same
// switch, none of which a unit test could reach except the first.
//
// `buffer` holds the bytes entered since the buffer was last reset, `index` is
// the position of the byte this ByteWord just wrote, and `word_number` is how
// many words of the *current* share had already been entered before it -- so 3
// for the fourth byte of the share, which is where the byte-string header
// sits. The two are only equal while the first share is being entered: for
// later shares `word_number` restarts at 0 and `index` keeps counting, which
// is also why the byte-string header this reads at `buffer[3]` is the one of
// the *first* share entered. Every share in a set has the same shape, so the
// two agree; this preserves what the three copies did rather than changing it.
//
// The bytes are read through a `const uint8_t *` because the callers hold them
// in a `char` buffer, and `char` is signed on the host that builds the unit
// tests and unsigned on the device.
//
// `*final_size` and `*count` are left as they are wherever the entered header
// says nothing about them: at any `word_number` other than the four below, and
// at the positions whose form of length is not the one the header declared.
// Callers rely on that -- reserved additional-info values deliberately leave
// the share count at whatever it was, which is what makes the completed share
// fail bolos_ux_sskr_hex_check().
void bolos_ux_sskr_entry_header_update(const uint8_t *buffer,
                                       size_t index,
                                       size_t word_number,
                                       size_t *final_size,
                                       uint8_t *count);

// Locate one share inside the buffer a generated set is concatenated into.
//
// bolos_ux_bip39_to_sskr_convert() writes the whole set into a single buffer,
// back to back and with no separator, and reports how many shares it holds.
// Paging through them is therefore a division, and both display paths --
// bagl/ux_sskr.c walking the set one screen at a time, nbgl/ui.c handing one
// share per generic-review page -- carried a copy of it.
//
// `share_index` is zero-based. On success `*offset` is where that share
// starts in the buffer and `*length` is how long one share is; on failure
// neither is written.
//
// Returning false rather than dividing is what holds the one thing this
// arithmetic can get wrong: `share_count` reaching it as 0. The BAGL path
// kept that out with a `1 <= index <= share_count` guard which cannot pass
// when the count is 0; the NBGL path had no guard of its own and relied on
// the review never being opened on an empty set.
bool bolos_ux_sskr_share_slice(size_t buffer_length,
                               uint8_t share_count,
                               uint8_t share_index,
                               size_t *offset,
                               size_t *length);

// Encode SSKR ByteWord as hex. Returns false, without writing to *value, if
// the ByteWord is not in the wordlist.
bool bolos_ux_sskr_byteword_to_hex(const unsigned char *byteword, uint8_t *value);

// Combine hex value SSKR shares into seed
void bolos_ux_sskr_to_seed_convert(unsigned char *sskr_shares_hex,
                                   unsigned int sskr_shares_hex_length,
                                   unsigned int sskr_shares_count,
                                   unsigned char *words_buffer,
                                   unsigned int *words_buffer_length,
                                   unsigned char *seed);

// convert seed from BIP39 to SSKR
unsigned int bolos_ux_bip39_to_sskr_convert(unsigned char *bip39_words_buffer,
                                            unsigned int bip39_words_buffer_length,
                                            unsigned int bip39_type,
                                            unsigned int *sskr_group_descriptor,
                                            uint8_t *sskr_share_count,
                                            unsigned char *sskr_words_buffer,
                                            unsigned int *sskr_words_buffer_length);

unsigned int bolos_ux_sskr_hex_check(unsigned char *sskr_shares_hex,
                                     unsigned int sskr_shares_hex_length,
                                     unsigned int sskr_share_count);

// Combine hex encoded SSKR shares into a secret
unsigned int bolos_ux_sskr_combine(unsigned char *sskr_shares_hex,
                                   unsigned int sskr_shares_hex_length,
                                   unsigned int sskr_shares_count,
                                   unsigned char *output);

// Decode a serialized SSKR share into space separated ByteWords
unsigned int bolos_ux_sskr_share_hex_decode(unsigned char *input,
                                            unsigned int input_len,
                                            unsigned char *output,
                                            unsigned int output_len);

unsigned int bolos_ux_sskr_get_word_idx_starting_with(const unsigned char *prefix,
                                                      const unsigned int prefixlength);
unsigned int bolos_ux_sskr_idx_strcpy(const unsigned int index, unsigned char *buffer);
unsigned int bolos_ux_sskr_get_word_count_starting_with(const unsigned char *prefix,
                                                        const unsigned int prefixlength);
unsigned int bolos_ux_sskr_get_word_next_letters_starting_with(const unsigned char *prefix,
                                                               const unsigned int prefixlength,
                                                               unsigned char *next_letters_buffer);

#if defined(HAVE_NBGL)
size_t bolos_ux_sskr_fill_with_candidates(const unsigned char *startingChars,
                                          const size_t startingCharsLength,
                                          char wordCandidatesBuffer[],
                                          const char *wordIndexorBuffer[]);
uint32_t bolos_ux_sskr_get_keyboard_mask(const unsigned char *prefix,
                                         const unsigned int prefixLength);
#endif
