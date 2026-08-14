/*
 * Regression test for the unchecked sign of bolos_ux_sskr_size_get()'s
 * return value in bolos_ux_bip39_to_sskr_convert() (seed_sskr.c).
 *
 * bolos_ux_sskr_size_get() propagates sskr_count_shards()'s return value
 * as-is, so an invalid group descriptor comes back as a negative error
 * code. The caller multiplied that count by the share length into a
 * uint16_t and passed the product to bolos_ux_sskr_generate() as
 * share_buffer_len -- the same value its failure path hands to
 * memzero(share_buffer, share_buffer_len). Measured on the three invalid
 * descriptors below, against a share_hex_buffer that is 592 bytes
 * (SSKR_MAX_GROUP_COUNT * SSS_MAX_SHARE_COUNT * (SSKR_MAX_STRENGTH_BYTES +
 * SSKR_METADATA_LENGTH_BYTES)):
 *
 *     {2,3} (valid)  count=  3  share_len=21  product=   63
 *     {3,2}          count=-12  share_len=21  product=65284
 *     {1,0}          count=-18  share_len=21  product=65158
 *     {1,5}          count= -4  share_len=21  product=65452
 *
 * i.e. up to 65452 bytes of zeros written over a 592-byte stack array,
 * through the caller's frame.
 *
 * Not reachable from the application today: both callers of
 * bolos_ux_bip39_to_sskr_convert() constrain the descriptor first --
 * src/nbgl/ui.c rejects a share count outside 1..16, a threshold of 0, a
 * threshold above the share count and 1-of-m with m > 1; src/bagl/ux_sskr.c
 * derives the share count from a bounded menu index, bounds the threshold
 * selector by the share count already chosen, and diverts 1-of-m with
 * m > 1 to a warning screen. bolos_ux_bip39_to_sskr_convert() is declared
 * in common_sskr.h all the same, and neither UI guard is compiled into any
 * test target, so nothing here checks that they stay.
 *
 * Verification technique: memzero() is explicit_bzero(), which this
 * toolchain's AddressSanitizer does not intercept -- unlike the plain
 * indexed writes in sskr_generate_bound_groups.c, the overrun is not
 * reported at the point of the write. What ASan sees is the process dying
 * later on a pointer the smashed frame no longer holds:
 *
 *     AddressSanitizer: SEGV on unknown address 0x000000000000
 *     The signal is caused by a WRITE memory access.
 *         #0 bolos_ux_bip39_to_sskr_convert .../seed_sskr.c
 *
 * so the first assertion here is survival: with neither half of the fix in
 * place these calls crash, and cmocka reports the failure. On top of that,
 * the return value pins each half separately -- the descriptor rejection
 * makes bolos_ux_bip39_to_sskr_convert() return 0 like its other failure
 * exit, which the bound inside bolos_ux_sskr_generate() alone would not do
 * (it would leave the caller returning 1 with a zero share count), and
 * test_generate_rejects_share_buffer_len_beyond_capacity exercises that
 * bound on its own. ASan is still enabled on the target to catch any
 * neighbouring overrun these cases might provoke, and seed_sskr.c is
 * compiled directly into it (rather than linked) so its stack frames are
 * instrumented, matching test_sskr_generate_bound_groups.
 */

#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "constants.h"
#include "sskr/common_sskr.h"
#include "sskr/seed_sskr_internal.h"
#include "sskr/sskr-constants.h"
#include "testutils.h"

/* Same 24-word vector as roundtrip.c, itself taken from the Blockchain
 * Commons SSKR test vectors. A valid mnemonic is required: an invalid one
 * would fail bolos_ux_bip39_mnemonic_decode() and return before reaching
 * the descriptor handling under test. */
static const unsigned char bip39_mnemonic[] =
    "toe priority custom gauge jacket theme arrest bargain gloom wide ill "
    "fit eagle prepare capable fish limb cigar reform other priority speak "
    "rough imitate";

/* Real capacity of bolos_ux_bip39_to_sskr_convert()'s share_hex_buffer. */
#define SHARE_HEX_BUFFER_LENGTH                   \
    (SSKR_MAX_GROUP_COUNT * SSS_MAX_SHARE_COUNT * \
     (SSKR_MAX_STRENGTH_BYTES + SSKR_METADATA_LENGTH_BYTES))

/* What one share of a 24-word seed occupies once serialized: a 5-byte
 * long-form CBOR byte-string header, the 37-byte shard
 * (SSKR_METADATA_LENGTH_BYTES + a 32-byte value) and the 4-byte CRC. */
#define SERIALIZED_SHARE_24_LENGTH \
    (5 + SSKR_METADATA_LENGTH_BYTES + SSKR_MAX_STRENGTH_BYTES + 4)

/* ...and as space-separated ByteWords: 4 letters per byte plus a
 * separator, minus the trailing separator of the last word. */
#define BYTEWORDS_SHARE_24_LENGTH (SERIALIZED_SHARE_24_LENGTH * 5 - 1)

/*
 * Drive bolos_ux_bip39_to_sskr_convert() end to end with the given
 * descriptor. The output buffer is sized for the largest share set any
 * descriptor could ask for, so that nothing here depends on the function
 * stopping short.
 */
static unsigned int convert_with_descriptor(unsigned int threshold,
                                            unsigned int count,
                                            uint8_t* share_count_out,
                                            unsigned int* share_words_len_out) {
    unsigned int group_descriptor[2] = {threshold, count};
    unsigned char bip39_words_buffer[sizeof(bip39_mnemonic)];
    unsigned char share_words_buffer[SSS_MAX_SHARE_COUNT *
                                     (BYTEWORDS_SHARE_24_LENGTH + 1)];

    memcpy(bip39_words_buffer, bip39_mnemonic, sizeof(bip39_words_buffer));
    memset(share_words_buffer, 0, sizeof(share_words_buffer));

    *share_count_out = 0xff;
    *share_words_len_out = 0;

    return bolos_ux_bip39_to_sskr_convert(
        bip39_words_buffer, sizeof(bip39_words_buffer) - 1,
        BIP39_MNEMONIC_SIZE_24, group_descriptor, share_count_out,
        share_words_buffer, share_words_len_out);
}

/*
 * Threshold above the member count: sskr_count_shards() returns
 * SSKR_ERROR_INVALID_MEMBER_THRESHOLD.
 */
static void test_convert_rejects_threshold_above_count(void** state) {
    (void)state;

    uint8_t share_count = 0;
    unsigned int share_words_len = 0;

    unsigned int result =
        convert_with_descriptor(3, 2, &share_count, &share_words_len);

    assert_int_equal(result, 0);
    assert_int_equal(share_count, 0);
}

/*
 * Zero members: SSKR_ERROR_INVALID_GROUP_COUNT.
 */
static void test_convert_rejects_zero_count(void** state) {
    (void)state;

    uint8_t share_count = 0;
    unsigned int share_words_len = 0;

    unsigned int result =
        convert_with_descriptor(1, 0, &share_count, &share_words_len);

    assert_int_equal(result, 0);
    assert_int_equal(share_count, 0);
}

/*
 * 1-of-m with m > 1: SSKR_ERROR_INVALID_SINGLETON_MEMBER.
 */
static void test_convert_rejects_singleton_member(void** state) {
    (void)state;

    uint8_t share_count = 0;
    unsigned int share_words_len = 0;

    unsigned int result =
        convert_with_descriptor(1, 5, &share_count, &share_words_len);

    assert_int_equal(result, 0);
    assert_int_equal(share_count, 0);
}

/*
 * The one case the UI can actually produce must be untouched: a 2-of-3
 * descriptor still yields three shares of the expected length. The share
 * contents themselves are covered by roundtrip.c; what matters here is
 * that the new rejection does not turn away a valid descriptor.
 */
static void test_convert_accepts_valid_descriptor(void** state) {
    (void)state;

    uint8_t share_count = 0;
    unsigned int share_words_len = 0;

    unsigned int result =
        convert_with_descriptor(2, 3, &share_count, &share_words_len);

    assert_int_equal(result, 1);
    assert_int_equal(share_count, 3);
    assert_int_equal(share_words_len, BYTEWORDS_SHARE_24_LENGTH * 3);
}

/*
 * The contract the rejection above consumes: bolos_ux_sskr_size_get()
 * really does return a negative code, not a count, for each of these
 * descriptors. If it ever started returning 0 or a positive value instead,
 * the guard in bolos_ux_bip39_to_sskr_convert() would silently stop
 * matching.
 */
static void test_size_get_returns_negative_error_codes(void** state) {
    (void)state;

    unsigned int threshold_above_count[2] = {3, 2};
    unsigned int zero_count[2] = {1, 0};
    unsigned int singleton_member[2] = {1, 5};
    unsigned int valid[2] = {2, 3};
    uint8_t share_len = 0;

    assert_int_equal(
        bolos_ux_sskr_size_get(BIP39_MNEMONIC_SIZE_24, 1, threshold_above_count,
                               1, &share_len),
        SSKR_ERROR_INVALID_MEMBER_THRESHOLD);
    assert_int_equal(bolos_ux_sskr_size_get(BIP39_MNEMONIC_SIZE_24, 1,
                                            zero_count, 1, &share_len),
                     SSKR_ERROR_INVALID_GROUP_COUNT);
    assert_int_equal(bolos_ux_sskr_size_get(BIP39_MNEMONIC_SIZE_24, 1,
                                            singleton_member, 1, &share_len),
                     SSKR_ERROR_INVALID_SINGLETON_MEMBER);

    assert_int_equal(
        bolos_ux_sskr_size_get(BIP39_MNEMONIC_SIZE_24, 1, valid, 1, &share_len),
        3);
    assert_int_equal(
        share_len, BIP39_MNEMONIC_SIZE_24 * 4 / 3 + SSKR_METADATA_LENGTH_BYTES);
}

/*
 * Second half of the fix, exercised directly rather than through the
 * descriptor: a share_buffer_len beyond any real buffer must never be used
 * as a write length. share_count_expected is deliberately mismatched so
 * that bolos_ux_sskr_generate() takes its memzero(share_buffer,
 * share_buffer_len) failure path -- which, unbounded, writes 65284 bytes
 * over the 592-byte buffer below.
 */
static void test_generate_rejects_share_buffer_len_beyond_capacity(
    void** state) {
    (void)state;

    unsigned int group_descriptor[2] = {1, 1};
    uint8_t seed[SSKR_MIN_STRENGTH_BYTES] = {0};
    uint8_t share_buffer[SHARE_HEX_BUFFER_LENGTH] = {0};
    uint8_t share_len = 0;

    unsigned int share_count =
        bolos_ux_sskr_generate(1, group_descriptor, 1, seed, sizeof(seed),
                               &share_len, share_buffer, 65284, 21, 0);

    assert_int_equal(share_count, 0);
}

/*
 * ...and a share_buffer_len at exactly that capacity must still work, so
 * the bound cannot be an off-by-one that turns away the only buffer the
 * application ever passes.
 */
static void test_generate_accepts_share_buffer_len_at_capacity(void** state) {
    (void)state;

    unsigned int group_descriptor[2] = {1, 1};
    uint8_t seed[SSKR_MIN_STRENGTH_BYTES] = {0};
    uint8_t share_buffer[SHARE_HEX_BUFFER_LENGTH] = {0};
    uint8_t share_len = 0;
    uint8_t share_len_expected = 0;

    int16_t share_count_expected = bolos_ux_sskr_size_get(
        BIP39_MNEMONIC_SIZE_12, 1, group_descriptor, 1, &share_len_expected);

    unsigned int share_count = bolos_ux_sskr_generate(
        1, group_descriptor, 1, seed, sizeof(seed), &share_len, share_buffer,
        sizeof(share_buffer), share_len_expected, share_count_expected);

    assert_int_equal(share_count, 1);
    assert_int_equal(share_len,
                     SSKR_MIN_STRENGTH_BYTES + SSKR_METADATA_LENGTH_BYTES);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_convert_rejects_threshold_above_count),
        cmocka_unit_test(test_convert_rejects_zero_count),
        cmocka_unit_test(test_convert_rejects_singleton_member),
        cmocka_unit_test(test_convert_accepts_valid_descriptor),
        cmocka_unit_test(test_size_get_returns_negative_error_codes),
        cmocka_unit_test(
            test_generate_rejects_share_buffer_len_beyond_capacity),
        cmocka_unit_test(test_generate_accepts_share_buffer_len_at_capacity),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
