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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "./seed_rom_variables.h"
#include "constants.h"

#ifdef HAVE_SHA3
#include <lcx_sha3.h>
#endif

/**
 * @brief Computes the number of bits needed to represent a DICE roll drawn
 * uniformly from `[0, sides)`.
 *
 * @details Kept outside the `HAVE_NBGL` guard below, and with external
 * linkage, purely so it can be linked into a unit test: the rest of this
 * file pulls in NBGL headers the unit test harness does not have.
 *
 * @param[in] sides Number of sides on the die. Must be in
 * `[2, UINT32_MAX >> 1]`; the caller enforces this.
 *
 * @return The number of bits per roll.
 */
uint8_t bip85_dice_bits_per_roll(uint32_t sides) {
    return (sizeof(sides) << 3) - __builtin_clz(sides - 1);
}

#ifdef HAVE_SHA3

// How many times the DRNG digest is allowed to double in size (from
// BIP85_DRNG_MAX_DIGEST_SIZE) before bip85_dice_roll() gives up and reports
// an error instead of producing a truncated result.
#define BIP85_DICE_MAX_DRNG_DOUBLINGS 3

/**
 * @brief Draws `rolls` values uniformly distributed in `[0, sides)` from a
 * BIP85-derived seed, re-extending the SHAKE256 output stream if the
 * initial digest runs out before enough values have been drawn.
 *
 * @details Kept outside the `HAVE_NBGL` guard below, and with external
 * linkage, purely so it can be linked into a unit test against the real
 * `cx_shake256_hash()` -- pure software, no BOLOS syscall -- without the
 * NBGL headers the rest of this file needs.
 *
 * Rejection sampling with `bits_per_roll = ceil(log2(sides))`
 * (`bip85_dice_bits_per_roll()` above) never accepts fewer than half of the
 * candidates it draws: the worst case is `sides` one more than a power of
 * two (e.g. `sides = 129`, `bits_per_roll = 8`, acceptance 129/256 =~
 * 50.4%). A `BIP85_DRNG_MAX_DIGEST_SIZE`-byte digest therefore runs out
 * only when `rolls` is in the hundreds or more for single-byte rolls, and
 * higher still for wider ones. When it does, this function re-derives the
 * digest from scratch at double the previous length rather than resuming
 * mid-stream: SHAKE256 is a genuine XOF, so a longer digest from the same
 * seed reproduces the same leading bytes -- already-accepted rolls are
 * identical across a retry -- and the computation is cheap enough that
 * redoing it from scratch costs nothing worth optimizing away.
 * `BIP85_DICE_MAX_DRNG_DOUBLINGS` bounds the number of retries: given the
 * better-than-50% acceptance rate above, exhausting them without producing
 * `rolls` values is not expected for any realistic request, but it is
 * still reported as an error rather than looped on indefinitely.
 *
 * @param[out] out           Buffer to receive the `rolls` dice results.
 * @param[in]  out_capacity  Capacity of `out`, in elements.
 * @param[in]  sides         Number of sides on the die.
 * @param[in]  rolls         Number of rolls requested.
 * @param[in]  seed          BIP85 entropy seed, `BIP85_ENTROPY_LENGTH`
 * bytes.
 *
 * @return `rolls` on success (`out[0..rolls)` is fully populated). A
 * negative value on error:
 * - `-1` if `out_capacity < rolls`;
 * - `-2` if the SHAKE256 call failed;
 * - `-3` if the DRNG stream was exhausted after
 *   `BIP85_DICE_MAX_DRNG_DOUBLINGS` doublings without producing `rolls`
 *   valid results.
 */
int32_t bip85_dice_roll(uint32_t* out, size_t out_capacity, uint32_t sides,
                        uint32_t rolls,
                        const uint8_t seed[BIP85_ENTROPY_LENGTH]) {
    if (out_capacity < rolls) {
        return -1;
    }

    uint8_t bits_per_roll = bip85_dice_bits_per_roll(sides);
    uint8_t bytes_per_roll = (bits_per_roll + 7) >> 3;
    uint8_t shift_amount = (bytes_per_roll << 3) - bits_per_roll;

    // Sized for the largest digest a retry can request; only the leading
    // `digest_length` bytes are meaningful on any given attempt.
    uint8_t digest[BIP85_DRNG_MAX_DIGEST_SIZE << BIP85_DICE_MAX_DRNG_DOUBLINGS];
    int32_t result = -3;

    for (uint8_t attempt = 0; attempt <= BIP85_DICE_MAX_DRNG_DOUBLINGS;
         attempt++) {
        size_t digest_length = (size_t)BIP85_DRNG_MAX_DIGEST_SIZE << attempt;

        if (cx_shake256_hash(seed, BIP85_ENTROPY_LENGTH, digest,
                             digest_length) != CX_OK) {
            result = -2;
            break;
        }

        uint8_t* digest_ptr = digest;
        uint32_t roll_index = 0;

        while (roll_index < rolls &&
               (size_t)(digest_ptr - digest) + bytes_per_roll <=
                   digest_length) {
            // Construct roll result from bytes_per_roll bytes
            uint32_t roll_result = 0;
            uint8_t* end_ptr = digest_ptr + bytes_per_roll;
            while (digest_ptr < end_ptr) {
                roll_result = roll_result << 8 | (uint32_t)*digest_ptr++;
            }
            // Adjust roll result to keep only bits_per_roll
            roll_result >>= shift_amount;

            // Check if roll result is within valid range
            if (roll_result < sides) {
                out[roll_index++] = roll_result;
            }
        }

        if (roll_index == rolls) {
            result = (int32_t)rolls;
            break;
        }
    }

    explicit_bzero(digest, sizeof(digest));
    return result;
}

#endif  // HAVE_SHA3

#ifdef HAVE_HMAC
#include <lcx_hmac.h>

/**
 * @brief Computes BIP85 entropy from an already BIP32-derived 32-byte root
 * key, via HMAC-SHA512("bip-entropy-from-k", key) as specified by BIP-85.
 *
 * @details Kept outside the `HAVE_NBGL` guard below, and with external
 * linkage, purely so it can be linked into a unit test against the real
 * `cx_hmac_no_throw()` -- pure software, no BOLOS syscall, unlike the BIP32
 * derivation in `bolos_ux_bip85_entropy()` below, which depends on
 * `os_derive_bip32_no_throw()` and cannot be tested on host. Same pattern as
 * `bip85_dice_roll()` above.
 *
 * `bolos_ux_bip85_entropy()` calls this with `key` and `out` pointing at the
 * same buffer -- safe only because a single `CX_LAST` call consumes the
 * whole 32-byte input before any output byte is written; preserve that
 * property if this function is ever changed.
 *
 * @param[in]  key      32-byte root key.
 * @param[out] out      Buffer to receive the HMAC-SHA512 output; may alias
 * `key`.
 * @param[in]  out_len  Number of bytes of `out` to write.
 *
 * @return `true` on success, `false` if the HMAC computation failed.
 */
bool bip85_entropy_from_key(const uint8_t key[32], uint8_t* out,
                            size_t out_len) {
    cx_hmac_sha512_t ctx;
    const char hmac_key[] = "bip-entropy-from-k";

    if (cx_hmac_sha512_init_no_throw(&ctx, (const uint8_t*)hmac_key,
                                     strlen(hmac_key)) != CX_OK) {
        return false;
    }
    if (cx_hmac_no_throw((cx_hmac_t*)&ctx, CX_LAST, key, 32, out, out_len) !=
        CX_OK) {
        return false;
    }
    return true;
}
#endif  // HAVE_HMAC

/**
 * @brief Copies the first `pwd_len` bytes of an encoded password buffer into
 * the caller's output buffer and NUL-terminates it.
 *
 * @details Kept outside the `HAVE_NBGL` guard below, and with external
 * linkage, purely so it can be linked into a unit test: `buffer_pwd` comes
 * from `base64_encode_64bytes()`/`base85_encode_64bytes()`, already tested
 * directly, but this final truncation-and-terminate step -- the one that
 * previously had a missing `pwd[pwd_len] = '\0';` on the Base64 side -- had
 * no test of its own. Same pattern as `bip85_dice_roll()`/
 * `bip85_entropy_from_key()` above.
 *
 * @param[in]  buffer_pwd  Fully encoded password, at least `pwd_len` bytes.
 * @param[out] pwd         Buffer to receive the truncated, NUL-terminated
 * password; must have room for `pwd_len + 1` bytes.
 * @param[in]  pwd_len     Number of bytes to copy from `buffer_pwd`.
 *
 * @return `pwd_len`.
 */
uint8_t bip85_finalize_pwd(const char* buffer_pwd, char* pwd, uint8_t pwd_len) {
    memcpy(pwd, buffer_pwd, pwd_len);
    pwd[pwd_len] = '\0';  // Add string termination character
    return pwd_len;
}

#if defined(HAVE_NBGL)
#include <lcx_hmac.h>
#include <lcx_sha3.h>

/* clang-format off */
#include "ui.h"
/* clang-format on */
#include "./seed_rom_variables.h"
#include "common.h"
#include "constants.h"

extern uint8_t base64_encode_64bytes(const uint8_t* src, char* dst);
extern uint8_t base85_encode_64bytes(const uint8_t* src, char* dst);

/**
 * @brief Generates BIP85 entropy from a device's seed using a specified BIP32
 * path.
 *
 * @details This function derives a root key from the device's seed using the
 * provided BIP32 path. It then calculates BIP85 entropy from this root key
 * using a HMAC-SHA512 hash with a specific key.
 *
 * @param[out] entropy  Pointer to a buffer where the generated BIP85 entropy
 * will be stored.
 * @param[in]  path     Pointer to an array of BIP32 path components.
 * @param[in]  path_len Length of the BIP32 path in components.
 *
 * @return 1 on success, 0 on failure.
 */
bool bolos_ux_bip85_entropy(uint8_t* entropy, const unsigned int* path,
                            unsigned int path_len) {
    // get rootkey from device's seed
    if (os_derive_bip32_no_throw(CX_CURVE_256K1, path, path_len, entropy,
                                 entropy + 32) != CX_OK) {
        PRINTF("An error occurred while generating BIP85 entropy\n");
        return 0;
    }
    PRINTF("Root key from device: 32 bytes\n");

    // Generate BIP85 entropy from root key
    if (!bip85_entropy_from_key(entropy, entropy, BIP85_ENTROPY_LENGTH)) {
        memzero(entropy, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "HMAC failed");
    }
    PRINTF("BIP85 entropy from root key: %u bytes\n", BIP85_ENTROPY_LENGTH);

    return 1;
}

/**
 * @brief Generates a random digest using SHAKE-256.
 *
 * @details This function generates a random digest of the specified length
 * using the SHAKE-256 hash function, seeded with the provided seed data.
 *
 * @param[out] digest         Pointer to the buffer to store the generated
 * digest.
 * @param[in]  digest_length  Length of the digest in bytes.
 * @param[in]  seed           Pointer to the seed data.
 * @param[in]  seed_length    Length of the seed data in bytes.
 *
 * @return 1 on success, 0 on failure.
 */
bool bolos_ux_bip85_drng_with_seed(uint8_t* seed, size_t seed_length,
                                   uint8_t* digest, size_t digest_length) {
    LEDGER_ASSERT(digest_length <= BIP85_DRNG_MAX_DIGEST_SIZE,
                  "BIP85 DRNG digest length exceeds maximum");
    if (cx_shake256_hash(seed, seed_length, digest, digest_length) != CX_OK) {
        PRINTF("SHAKE256 hash error\n");
        return 0;
    }
    PRINTF("BIP85 DRNG output: %u bytes\n", digest_length);

    return 1;
}

/**
 * @brief Generates a random digest using BIP85 DRNG.
 *
 * @details This function generates a random digest of the specified length
 * using the BIP85 Deterministic Random Number Generator (DRNG). The DRNG is
 * seeded with entropy derived from a specific BIP85 derivation path.
 *
 * @param[out] digest         Pointer to the buffer to store the generated
 * digest.
 * @param[in]  digest_length  Length of the digest in bytes.
 * @param[in]  index          Index used to differentiate different random
 * number generations.
 *
 * @return None
 */
void bolos_ux_bip85_drng_test(uint8_t* digest, size_t digest_length,
                              unsigned int index) {
    // m / purpose'   / app_no' / index'
    // m / 83696968'  / 0'      / index'
    const unsigned int path[] = {0x84FD1D48, 0x80000000, 0x80000000 | index};

    uint8_t buffer[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer, path, ARRAYLEN(path)) != 1) {
        memzero(buffer, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    if (bolos_ux_bip85_drng_with_seed(buffer, BIP85_ENTROPY_LENGTH, digest,
                                      digest_length) != 1) {
        memzero(buffer, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 SHAKE256 hash failed");
    }

    memzero(buffer, BIP85_ENTROPY_LENGTH);
}

uint8_t bolos_ux_bip85_bip39(uint8_t* hex_out, uint8_t language, uint8_t words,
                             unsigned int index) {
    LEDGER_ASSERT((words >= BIP39_MNEMONIC_SIZE_12) && (words % 3 == 0) &&
                      (words <= BIP39_MNEMONIC_SIZE_24),
                  "Invalid value for BIP85 BIP89 words");

    // m / purpose'   / app_no' / language' / words' / index'
    // m / 83696968'  / 39'     / language' / words' / index'
    const unsigned int path[] = {0x84FD1D48, 0x80000027, 0x80000000 | language,
                                 0x80000000 | words, 0x80000000 | index};

    uint8_t buffer[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer, path, ARRAYLEN(path)) != 1) {
        memzero(buffer, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    memcpy(hex_out, buffer, words * 4 / 3);
    memzero(buffer, BIP85_ENTROPY_LENGTH);

    PRINTF("BIP85 BIP39 hex output: %u bytes\n", words * 4 / 3);
    return words * 4 / 3;
}

void bolos_ux_bip85_hex(uint8_t* hex_out, uint8_t num_bytes,
                        unsigned int index) {
    LEDGER_ASSERT((num_bytes >= 16) && (num_bytes <= BIP85_ENTROPY_LENGTH),
                  "Invalid value for BIP85 HEX length");

    // m / purpose'   / app_no' / num_bytes' / index'
    // m / 83696968'  / 128169' / num_bytes' / index'
    const unsigned int path[] = {0x84FD1D48, 0x8001F4A9, 0x80000000 | num_bytes,
                                 0x80000000 | index};

    uint8_t buffer[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer, path, ARRAYLEN(path)) != 1) {
        memzero(buffer, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    memcpy(hex_out, buffer, num_bytes);
    memzero(buffer, BIP85_ENTROPY_LENGTH);

    PRINTF("BIP85 HEX output: %u bytes\n", num_bytes);
}

uint8_t bolos_ux_bip85_pwd_base64(char* pwd, uint8_t pwd_len,
                                  unsigned int index) {
    LEDGER_ASSERT((pwd_len >= 20) && (pwd_len <= BASE64_ENCODE_LENGTH - 2),
                  "Invalid value for BIP85 PWD BASE64 length");

    // m / purpose'   / app_no' / pwd_len' / index'
    // m / 83696968'  / 707764' / pwd_len' / index'
    const unsigned int path[] = {0x84FD1D48, 0x800ACCB4, 0x80000000 | pwd_len,
                                 0x80000000 | index};
    uint8_t buffer_ent[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer_ent, path, ARRAYLEN(path)) != 1) {
        memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    char buffer_pwd[BASE64_ENCODE_LENGTH];

    if (base64_encode_64bytes(buffer_ent, buffer_pwd) != BASE64_ENCODE_LENGTH) {
        memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "Base64 encoding failed");
    }

    bip85_finalize_pwd(buffer_pwd, pwd, pwd_len);

    memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
    memzero(buffer_pwd, BASE64_ENCODE_LENGTH);

    PRINTF("BIP85 PWD BASE64 output: %u characters\n", pwd_len);

    return pwd_len;
}

uint8_t bolos_ux_bip85_pwd_base85(char* pwd, uint8_t pwd_len,
                                  unsigned int index) {
    LEDGER_ASSERT((pwd_len >= 10) && (pwd_len <= BASE85_ENCODE_LENGTH),
                  "Invalid value for BIP85 PWD BASE85 length");

    // m / purpose'   / app_no' / pwd_len' / index'
    // m / 83696968'  / 707785' / pwd_len' / index'
    const unsigned int path[] = {0x84FD1D48, 0x800ACCC9, 0x80000000 | pwd_len,
                                 0x80000000 | index};
    uint8_t buffer_ent[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer_ent, path, ARRAYLEN(path)) != 1) {
        memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    char buffer_pwd[BASE85_ENCODE_LENGTH];

    if (base85_encode_64bytes(buffer_ent, buffer_pwd) != BASE85_ENCODE_LENGTH) {
        memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "Base85 encoding failed");
    }

    bip85_finalize_pwd(buffer_pwd, pwd, pwd_len);

    memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
    memzero(buffer_pwd, BASE85_ENCODE_LENGTH);

    PRINTF("BIP85 PWD BASE85 output: %u characters\n", pwd_len);

    return pwd_len;
}

int32_t bolos_ux_bip85_dice(uint32_t* out, size_t out_capacity, uint32_t sides,
                            uint32_t rolls, uint32_t index) {
    LEDGER_ASSERT((sides >= 2) && (sides <= (UINT32_MAX >> 1)),
                  "Invalid value for BIP85 DICE sides");
    LEDGER_ASSERT((rolls >= 1) && (rolls <= (UINT32_MAX >> 1)),
                  "Invalid value for BIP85 DICE rolls");

    // m / purpose'   / app_no' / sides' / rolls' / index'
    // m / 83696968'  / 89101' /  sides' / rolls' / index'
    const unsigned int path[] = {0x84FD1D48, 0x80015C0D, 0x80000000 | sides,
                                 0x80000000 | rolls, 0x80000000 | index};

    uint8_t buffer_ent[BIP85_ENTROPY_LENGTH];

    if (bolos_ux_bip85_entropy(buffer_ent, path, ARRAYLEN(path)) != 1) {
        memzero(buffer_ent, BIP85_ENTROPY_LENGTH);
        LEDGER_ASSERT(false, "BIP85 entropy failed");
    }

    int32_t produced =
        bip85_dice_roll(out, out_capacity, sides, rolls, buffer_ent);

    memzero(buffer_ent, BIP85_ENTROPY_LENGTH);

    return produced;
}
#endif
