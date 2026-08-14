/*
 * Randomised properties of the BIP-39 mnemonic encoder and decoder.
 *
 * tests/bip39.c holds this pair against the published BIP-39 vectors and
 * against a set of named edge cases. This file states the invariant those
 * vectors are instances of, and checks it on entropy drawn at random rather
 * than on the handful of values someone thought to write down:
 *
 *   decode(encode(entropy)) == entropy
 *
 * Self-checking, like the SSKR properties: the encoder is compared to the
 * decoder rather than to a second implementation of BIP-39 written for the
 * occasion. Holding this code against the specification is the frozen vectors'
 * job and they already do it; what a wordlist-based oracle here would add is
 * mostly its own bugs.
 *
 * The five entropy lengths the encoder accepts are 16, 20, 24, 28 and 32 bytes
 * (bolos_ux_bip39_mnemonic_encode() takes any multiple of 4 in that range), but
 * only three of them round trip, and that asymmetry is deliberate rather than a
 * defect:
 *
 *   - 16, 24 and 32 bytes produce 12, 18 and 24 words. These are the three
 *     sizes the application offers (BIP39_MNEMONIC_SIZE_12/18/24 in
 *     src/constants.h) and the three the decoder accepts.
 *   - 20 and 28 bytes produce 15 and 21 words, which are valid BIP-39 but are
 *     not offered anywhere in this application, and which
 *     bolos_ux_bip39_mnemonic_decode() refuses outright (`n != 12 && n != 18 &&
 *     n != 24`).
 *
 * Both halves are asserted below. The second is the more useful of the two: it
 * pins the boundary, so that a future change either keeps refusing 15 and 21
 * words or turns this test red and has to say why.
 *
 * Reproducibility works the same way as in tests/sskr_properties.c: each
 * iteration seeds tests/unit/lib/testprng.h from a fixed root plus its index,
 * and every failure message carries that number and the case. The suite's
 * cx_rng_no_throw() is not used and not touched -- it writes 0, 1, 2, ... and
 * restarts on every call, so the same entropy would be tested a few hundred
 * times over.
 */

#include <cmocka.h>
#include <inttypes.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* testutils.h has to come first: it defines WIDE, which
 * bip39/seed_rom_variables.h uses without defining. Not sorted, hence the
 * clang-format exclusion. */
// clang-format off
#include "testutils.h"
#include "testprng.h"
#include "bip39/common_bip39.h"
// clang-format on

#define SEED_ROUND_TRIP UINT64_C(0x53B1A9C304D71E80)
#define SEED_REFUSED UINT64_C(0x53B1A9C304D71E90)

#define RANDOM_ITERATIONS 400

/* 24 words of at most 8 characters, separated by spaces. */
#define MNEMONIC_CAPACITY (24 * 9)
/* Entropy plus the checksum byte the decoder writes past it. */
#define BITS_CAPACITY (32 + 1)

/* The three the decoder accepts, and the three the application offers. */
static const uint8_t round_trip_lengths[] = {16, 24, 32};

/* Encoder-valid, decoder-refused: 15 and 21 words. */
static const uint8_t refused_lengths[] = {20, 28};

/* One round trip, reported under a label that identifies the case: the seed for
 * a drawn one, a name for a fixed one. Both go through the same path so that no
 * failure can come out as a bare "0 != 1". */
static void round_trip_check(const char* case_label, const uint8_t* entropy,
                             uint8_t entropy_len) {
    unsigned char mnemonic[MNEMONIC_CAPACITY];
    unsigned char decoded[BITS_CAPACITY];

    const unsigned int mnemonic_len = bolos_ux_bip39_mnemonic_encode(
        entropy, entropy_len, mnemonic, sizeof(mnemonic));
    if (mnemonic_len == 0) {
        fail_msg("BIP-39 round trip: %s, %u bytes of entropy: encoding failed",
                 case_label, entropy_len);
    }

    if (bolos_ux_bip39_mnemonic_check(mnemonic, mnemonic_len) != 1) {
        fail_msg(
            "BIP-39 round trip: %s, %u bytes of entropy: the mnemonic just "
            "encoded was reported invalid",
            case_label, entropy_len);
    }

    memset(decoded, 0xA5, sizeof(decoded));
    if (bolos_ux_bip39_mnemonic_decode(mnemonic, mnemonic_len, decoded,
                                       sizeof(decoded)) != 1) {
        fail_msg("BIP-39 round trip: %s, %u bytes of entropy: decoding failed",
                 case_label, entropy_len);
    }

    if (memcmp(decoded, entropy, entropy_len) != 0) {
        fail_msg(
            "BIP-39 round trip: %s, %u bytes of entropy: the decoded entropy "
            "differs from the original",
            case_label, entropy_len);
    }
}

static void test_encode_then_decode_is_the_identity(void** state) {
    (void)state;

    for (size_t i = 0;
         i < sizeof(round_trip_lengths) / sizeof(round_trip_lengths[0]); i++) {
        const uint8_t entropy_len = round_trip_lengths[i];
        uint8_t entropy[BITS_CAPACITY];
        char label[64];

        /* The all-zero and all-ones entropies first: both are valid, both are
         * as far from a uniform draw as an input gets, and neither would come
         * up in a few hundred iterations. */
        memset(entropy, 0x00, sizeof(entropy));
        round_trip_check("all-zero entropy", entropy, entropy_len);

        memset(entropy, 0xFF, sizeof(entropy));
        round_trip_check("all-ones entropy", entropy, entropy_len);

        for (unsigned int iteration = 0; iteration < RANDOM_ITERATIONS;
             iteration++) {
            const uint64_t case_seed =
                SEED_ROUND_TRIP + i * RANDOM_ITERATIONS + iteration;

            test_prng_seed(case_seed);
            test_prng_fill(entropy, entropy_len);
            snprintf(label, sizeof(label), "seed 0x%016" PRIx64, case_seed);
            round_trip_check(label, entropy, entropy_len);
        }
    }
}

/* The other side of the boundary: 20 and 28 bytes encode into a well-formed
 * 15- or 21-word mnemonic, and the decoder refuses it. Not a defect -- the
 * application never produces those two sizes -- but a limit worth pinning, so
 * that widening the encoder without widening the decoder cannot pass unnoticed.
 */
static void test_word_counts_outside_the_application_are_refused(void** state) {
    (void)state;

    for (size_t i = 0; i < sizeof(refused_lengths) / sizeof(refused_lengths[0]);
         i++) {
        const uint8_t entropy_len = refused_lengths[i];

        for (unsigned int iteration = 0; iteration < RANDOM_ITERATIONS;
             iteration++) {
            const uint64_t case_seed =
                SEED_REFUSED + i * RANDOM_ITERATIONS + iteration;
            uint8_t entropy[BITS_CAPACITY];
            unsigned char mnemonic[MNEMONIC_CAPACITY];
            unsigned char decoded[BITS_CAPACITY];

            test_prng_seed(case_seed);
            test_prng_fill(entropy, entropy_len);

            const unsigned int mnemonic_len = bolos_ux_bip39_mnemonic_encode(
                entropy, entropy_len, mnemonic, sizeof(mnemonic));
            if (mnemonic_len == 0) {
                fail_msg("BIP-39 refused sizes: seed 0x%016" PRIx64
                         ", %u bytes of entropy: encoding failed, expected it "
                         "to succeed",
                         case_seed, entropy_len);
            }

            memset(decoded, 0xA5, sizeof(decoded));
            const unsigned int accepted = bolos_ux_bip39_mnemonic_decode(
                mnemonic, mnemonic_len, decoded, sizeof(decoded));

            if (accepted != 0) {
                fail_msg("BIP-39 refused sizes: seed 0x%016" PRIx64
                         ", %u bytes of entropy (%u words): decoding returned "
                         "%u, expected a refusal",
                         case_seed, entropy_len, entropy_len * 3 / 4, accepted);
            }

            /* Nothing decoded is left behind. Note that this particular
             * refusal returns before the memzero() that the later ones go
             * through -- the word count is rejected first, above it -- so the
             * assertion is that the caller's buffer holds no decoded material,
             * not that it was wiped. */
            if (memcmp(decoded, entropy, entropy_len) == 0) {
                fail_msg("BIP-39 refused sizes: seed 0x%016" PRIx64
                         ", %u bytes of entropy: decoding was refused but the "
                         "entropy was written to the output buffer anyway",
                         case_seed, entropy_len);
            }
        }
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_encode_then_decode_is_the_identity),
        cmocka_unit_test(test_word_counts_outside_the_application_are_refused),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
