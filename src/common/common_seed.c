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

#include <lcx_hmac.h>
#include <lcx_rng.h>

/* clang-format off */
#include "constants.h"
#include "ui.h"
/* clang-format on */
#include "./bip39/common_bip39.h"
#include "./common.h"
#if defined(HAVE_NBGL)
#include "../nbgl/bip39_mnemonic.h"
extern unsigned int tool_type;
#endif

// The second of the two comparisons the verdict rests on, and deliberately
// not a second call to os_secure_memcmp(): a fault that lands on that
// function's body, or on the register its result comes back in, would land on
// both calls the same way, which is the one thing a redundant check must not
// do. This reads the same bytes from the other end, accumulates every
// difference rather than stopping at the first, and so takes the same time
// whatever the data -- which is the property os_secure_memcmp() is chosen for
// in the first place and must not be given up here.
static uint8_t compare_reversed(const uint8_t* a, const uint8_t* b,
                                size_t length) {
    uint8_t diff = 0;

    for (size_t i = length; i > 0; i--) {
        diff |= (uint8_t)(a[i - 1] ^ b[i - 1]);
    }

    return diff;
}

// A short, unpredictable wait before the comparison. It is not a defence on
// its own -- 255 iterations is microseconds -- and it does not try to be: what
// it costs an attacker is the ability to fire a glitch at a fixed offset from
// a trigger they can see, which is how a bench is calibrated. volatile so that
// no optimisation level deletes a loop with no effect.
static void verdict_jitter(void) {
    for (volatile uint8_t spin = cx_rng_u8(); spin > 0; spin--) {
    }
}

// Only the derivation status is a hardware-dependent input here --
// os_derive_bip32_no_throw() itself (a BOLOS syscall) is not testable on
// host, but everything that happens with its result is pure logic:
// comparing the two root keys on success, and erasing both buffers either
// way. Splitting it out lets that logic -- including the derivation-failure
// path, never exercised anywhere until now -- be covered without the
// syscall.
//
// This is where the application decides whether the phrase in front of the
// user is the one their device holds, so it is written against a fault
// injected into that decision rather than against ordinary failure. Four
// things carry that:
//
//   - the verdict is one of two constants 32 bits apart (common.h), not a
//     bool, so no single bit flip between here and the screen converts one
//     into the other;
//   - two independent comparisons have to agree. os_secure_memcmp() is kept
//     as it was; compare_reversed() above reads the same 64 bytes the other
//     way. A fault that defeats one is not the same fault that defeats the
//     other, and the unit suite holds this by making the first one lie;
//   - the bytes are read a third time inside the branch that has already
//     concluded "match", and a verdict that does not survive that re-reading
//     is a fault, not a mismatch: there is nothing safe to display, so the
//     application stops;
//   - a counter is stepped at each point the function must pass through and
//     checked at the end, so an instruction skip that jumps over the
//     comparison arrives at a count that does not add up.
//
// None of the four is held by a test on its own except the second: a fault is
// not something the host suite can produce. What the suite does hold is that
// the verdict is one of the two sentinels and never an intermediate value,
// which is what the four are there to protect.
unsigned int compare_recovery_phrase_finish(cx_err_t derivation_status,
                                            uint8_t buffer[64],
                                            uint8_t buffer_device[64]) {
    unsigned int verdict = VERDICT_NO_MATCH;
    volatile unsigned int checkpoints = 0;
    // Neither starts at zero: zero is what "the two agree" looks like, so a
    // fault that skips the assignments below must not leave agreement behind.
    unsigned char forward = 0xFF;
    unsigned char reversed = 0xFF;

    verdict_jitter();
    checkpoints++;

    if (derivation_status == CX_OK) {
        forward = (unsigned char)os_secure_memcmp(buffer, buffer_device, 64);
        reversed = compare_reversed(buffer, buffer_device, 64);
    }
    checkpoints++;

    if (derivation_status == CX_OK && forward == 0 && reversed == 0) {
        verdict = VERDICT_MATCH;
    }
    checkpoints++;

    // Re-read after the branch, not before it. The point is not to compare
    // again -- that already happened -- but to catch a decision that was
    // reached on something other than what the buffers hold.
    if (verdict == VERDICT_MATCH) {
        if (derivation_status != CX_OK ||
            os_secure_memcmp(buffer, buffer_device, 64) != 0 ||
            compare_reversed(buffer, buffer_device, 64) != 0) {
            LEDGER_ASSERT(false, "Seed verdict did not survive re-reading");
        }
    }
    checkpoints++;

    memzero(buffer_device, 64);
    memzero(buffer, 64);

    // After the erasures, so that a control-flow fault does not leave two root
    // keys on the stack on its way out.
    LEDGER_ASSERT(checkpoints == 4, "Seed verdict skipped a checkpoint");

    // Nothing but VERDICT_MATCH leaves as anything but VERDICT_NO_MATCH:
    // callers test for the former, and a third value has no business
    // travelling any further than this line.
    if (verdict != VERDICT_MATCH) {
        verdict = VERDICT_NO_MATCH;
    }

    return verdict;
}

unsigned int compare_recovery_phrase(bool* reconstructed) {
    // convert mnemonic to hex-seed. Zeroed at the declaration because the two
    // branches that fill it are both conditional: TOOL_TYPE_BIP85 is a third
    // value of that enumeration (constants.h), no path reaches this function
    // with it today, and an uninitialised 64-byte buffer going into the
    // HMAC below is not a thing to leave resting on that.
    uint8_t buffer[64] = {0};
    unsigned int result = VERDICT_NO_MATCH;

    // declared up front so goto cleanup can reach it from every early exit
    uint8_t buffer_device[64];

    // os_derive_bip32* do not accept NULL path, even with a size of 0, so we
    // provide an empty path
    const unsigned int empty_path = 0;

    *reconstructed = true;

#if defined(HAVE_BAGL)
    if (G_bolos_ux_context.tool_type == TOOL_TYPE_BIP39) {
        bolos_ux_bip39_mnemonic_to_seed(
            (unsigned char*)G_bolos_ux_context.words_buffer,
            G_bolos_ux_context.words_buffer_length, buffer);
    } else if (G_bolos_ux_context.tool_type == TOOL_TYPE_SSKR) {
        G_bolos_ux_context.words_buffer_length =
            sizeof(G_bolos_ux_context.words_buffer);
        bolos_ux_sskr_to_seed_convert(
            (unsigned char*)G_bolos_ux_context.sskr_words_buffer,
            G_bolos_ux_context.sskr_words_buffer_length,
            G_bolos_ux_context.sskr_share_count,
            (unsigned char*)&G_bolos_ux_context.words_buffer,
            &G_bolos_ux_context.words_buffer_length, buffer);
        if (G_bolos_ux_context.words_buffer_length == 0) {
            // shards accepted by the CRC check but not combinable
            *reconstructed = false;
            goto cleanup;
        }
    }
#elif defined(HAVE_NBGL)
    if (tool_type == TOOL_TYPE_BIP39) {
        bolos_ux_bip39_mnemonic_to_seed(
            (const unsigned char*)bip39_mnemonic_get(),
            bip39_mnemonic_length_get(), buffer);
    } else if (tool_type == TOOL_TYPE_SSKR) {
        if (!bip39_mnemonic_from_sskr_shares(buffer)) {
            // shards accepted by the CRC check but not combinable
            *reconstructed = false;
            goto cleanup;
        }
    }
#endif
    PRINTF("Input seed: 64 bytes\n");

    // get rootkey from hex-seed
    cx_hmac_sha512_t ctx;
    const char key[] = "Bitcoin seed";

    LEDGER_ASSERT(cx_hmac_sha512_init_no_throw(&ctx, (const uint8_t*)key,
                                               strlen(key)) == CX_OK,
                  "HMAC init failed");
    LEDGER_ASSERT(cx_hmac_no_throw((cx_hmac_t*)&ctx, CX_LAST, buffer, 64,
                                   buffer, 64) == CX_OK,
                  "HMAC failed");
    PRINTF("Root key from input: 64 bytes\n");

    // get rootkey from device's seed
    cx_err_t derivation_status = os_derive_bip32_no_throw(
        CX_CURVE_256K1, &empty_path, 0, buffer_device, buffer_device + 32);
    if (derivation_status != CX_OK) {
        PRINTF("An error occurred while comparing the recovery phrase\n");
    } else {
        PRINTF("Root key from device: 64 bytes\n");
    }

    return compare_recovery_phrase_finish(derivation_status, buffer,
                                          buffer_device);

cleanup:
    memzero(buffer_device, 64);
    memzero(buffer, 64);

    return result;
}
