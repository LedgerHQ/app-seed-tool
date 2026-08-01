#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bip39/common_bip39.h"
#include "testutils.h"

const unsigned char bip39_mnemonic[] =
    "toe priority custom gauge jacket theme arrest bargain gloom wide ill fit "
    "eagle prepare capable fish limb cigar reform other priority speak rough "
    "imitate";

const unsigned char seed[] = {
    0x27, 0x18, 0x6B, 0x7F, 0x5A, 0xA1, 0xD6, 0xC2, 0xBC, 0x81, 0xCA,
    0x9A, 0xB8, 0xD4, 0x3A, 0x47, 0x8F, 0xDB, 0x80, 0xD5, 0x26, 0x04,
    0x9D, 0x7A, 0x28, 0x09, 0x89, 0xCA, 0x02, 0xDA, 0x86, 0xA2, 0xB3,
    0xB2, 0x7D, 0xD0, 0x08, 0x02, 0xA5, 0xC7, 0x96, 0xCA, 0x4A, 0x0E,
    0x51, 0x58, 0x45, 0x66, 0x7D, 0xEE, 0x32, 0xE7, 0x6A, 0xED, 0x18,
    0x49, 0x8D, 0xEA, 0x8A, 0x20, 0x61, 0xFA, 0x0D, 0x9A};

static void test_bip39(void** state) {
    uint8_t buffer[64] = {0};

    bolos_ux_bip39_mnemonic_to_seed(bip39_mnemonic, sizeof(bip39_mnemonic) - 1,
                                    buffer);

    assert_memory_equal(buffer, seed, sizeof(seed));
}

// bolos_ux_bip39_mnemonic_to_seed() only pre-hashes the mnemonic (SHA-512,
// truncated to 64 bytes) before PBKDF2 when its length exceeds 128 bytes;
// below that threshold it feeds PBKDF2 the raw mnemonic bytes directly. The
// test above uses a 152-character mnemonic and exercises only the
// pre-hashed branch -- the far more common case in practice (a
// standard-length phrase stays well under 128 characters) was untested.
// This drives that branch directly against an independently computed
// PBKDF2-HMAC-SHA512 vector (salt "mnemonic", 2048 iterations); the
// mnemonic is the same 12-word vector already used by the roundtrip tests
// below.
static void test_bip39_seed_short_mnemonic(void** state) {
    (void)state;
    static const unsigned char mnemonic[] =
        "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
        "nose";
    static const uint8_t expected_seed[64] = {
        0x24, 0x2a, 0xbe, 0x4d, 0xed, 0x79, 0x03, 0xcd, 0xb6, 0xcf, 0xcb,
        0x3e, 0x0a, 0xed, 0x5e, 0x62, 0x7d, 0x95, 0x00, 0x52, 0x09, 0x4e,
        0x51, 0xa7, 0x40, 0x47, 0x89, 0x2d, 0xc1, 0xed, 0x1d, 0xb5, 0x37,
        0xf0, 0x0c, 0xc0, 0x42, 0xd5, 0xd8, 0x3c, 0x1e, 0x40, 0x7c, 0xc7,
        0x3f, 0xfc, 0xe5, 0x5f, 0xf5, 0xfb, 0xaa, 0x4f, 0x24, 0x04, 0xd8,
        0x19, 0xef, 0xdf, 0xd2, 0x61, 0x7b, 0x49, 0xeb, 0xe3};
    uint8_t out_seed[64];

    bolos_ux_bip39_mnemonic_to_seed((unsigned char*)mnemonic,
                                    sizeof(mnemonic) - 1, out_seed);

    assert_memory_equal(out_seed, expected_seed, sizeof(expected_seed));
}

// Exercises the exact 128/129-byte boundary of the pre-hash guard above.
// bolos_ux_bip39_mnemonic_to_seed() does not validate its input as a real
// BIP-39 phrase -- it hashes whatever bytes it is given -- so an arbitrary,
// deterministic buffer (repeating A-Z) is enough. Both expected seeds are
// computed independently (128: direct PBKDF2 on the 128 raw bytes; 129:
// PBKDF2 on SHA-512(129 bytes) truncated to 64), confirming the
// implementation lands on the correct side of the guard at each length.
static void test_bip39_seed_length_boundary(void** state) {
    (void)state;
    unsigned char buf[129];
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = 'A' + (i % 26);
    }

    static const uint8_t expected_seed_128[64] = {
        0x9b, 0xc3, 0x17, 0x45, 0x6c, 0xfa, 0x41, 0x0c, 0xb7, 0x07, 0xb7,
        0x91, 0xc2, 0x2e, 0x59, 0xad, 0xc9, 0x57, 0xf1, 0x4f, 0xcd, 0x91,
        0xb9, 0x42, 0xf7, 0xec, 0x95, 0x15, 0x74, 0xb2, 0xfa, 0xc2, 0x5b,
        0x63, 0x3e, 0x02, 0x52, 0xbb, 0x76, 0x13, 0xa1, 0xaa, 0xcf, 0xa8,
        0x6c, 0x2f, 0x07, 0x27, 0xd0, 0xed, 0x04, 0xed, 0xd3, 0x83, 0xb2,
        0x86, 0x7d, 0x05, 0xb6, 0xb3, 0xce, 0x83, 0xd5, 0x12};
    static const uint8_t expected_seed_129[64] = {
        0xd2, 0x96, 0xf1, 0xf4, 0xaa, 0x20, 0xb3, 0xfb, 0x72, 0x59, 0xfc,
        0x7b, 0xbf, 0xcc, 0xd1, 0x43, 0x26, 0x39, 0x49, 0x22, 0x13, 0x51,
        0xb1, 0xff, 0x47, 0x82, 0xb9, 0x12, 0xd2, 0x97, 0x3b, 0x79, 0x95,
        0xa2, 0x40, 0x6d, 0x2f, 0x1f, 0x5f, 0x8d, 0xb8, 0xc4, 0xc6, 0xe1,
        0x52, 0xfa, 0xb7, 0x26, 0xf1, 0xc2, 0xb1, 0x48, 0x4b, 0x9e, 0x57,
        0x0b, 0x45, 0xff, 0x6f, 0xb5, 0x2e, 0x6a, 0x01, 0xfc};
    uint8_t seed_128[64];
    uint8_t seed_129[64];

    bolos_ux_bip39_mnemonic_to_seed(buf, 128, seed_128);
    assert_memory_equal(seed_128, expected_seed_128, sizeof(expected_seed_128));

    bolos_ux_bip39_mnemonic_to_seed(buf, 129, seed_129);
    assert_memory_equal(seed_129, expected_seed_129, sizeof(expected_seed_129));
}

// All twelve words exist in the wordlist; only the last word differs from
// the valid vector (whose last word is "nose") -- the checksum byte no
// longer matches, so decode() must reject it despite every word being
// individually valid.
static const unsigned char bip39_bad_checksum_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "abandon";

static void test_bip39_decode_bad_checksum(void** state) {
    (void)state;
    unsigned char bits[32 + 1];

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        bip39_bad_checksum_mnemonic, sizeof(bip39_bad_checksum_mnemonic) - 1,
        bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// The vector above swaps in "abandon", which is index 0 and so changes the
// entropy bits as well as the checksum bits. It therefore says nothing about
// how *many* checksum bits decode() actually compares. A 12-word phrase
// carries a 4-bit checksum, which is why decode() masks with 0xF0 for n == 12;
// narrowing that mask to 0xE0 would compare only three of the four and admit
// any phrase whose fourth checksum bit is wrong. Nothing held that width: with
// mask = 0xE0 the whole suite stayed green. This vector pins it.
//
// Derivation, starting from the 16-byte entropy already used below by
// test_bip39_roundtrip_12_words:
//
//   entropy    62 50 b6 8d af 74 6d 12 a2 4d 58 b4 78 7a 71 4b
//   SHA-256    3313908a3667d9ad2f7f6bd844ab737430b84651f0bd70ebd6646a7d0564c379
//   SHA-256[0] 0x33 = 0011 0011, so the four checksum bits are 0011
//
// The checksum occupies the last 4 of the 132 bits, i.e. the low nibble of
// the twelfth word's 11-bit index. "nose" is index 1203 = 100 1011 0011,
// low nibble 0011 -- the correct checksum. Clearing its last bit gives index
// 1202 = 100 1011 0010 = "north": the same 128 entropy bits, and a checksum
// of 0010, wrong in the fourth bit and only there.
//
// What decode() then compares, with buffer[0] = SHA-256[0] = 0x33 and
// bits[16] = 0x20 (the "north" nibble, left-aligned):
//
//   0x33 & 0xF0 = 0x30 != 0x20 = bits[16] & 0xF0  -> rejected, as it must be
//   0x33 & 0xE0 = 0x20 == 0x20 = bits[16] & 0xE0  -> accepted, the weakening
//
// Checked independently against SHA-256 and the BIP-39 English wordlist: the
// two phrases decode to byte-identical entropy and differ in that one
// checksum bit alone. The valid phrase is asserted here too, so a failure
// cannot be blamed on something unrelated to the mask.
static const unsigned char bip39_valid_12_word_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "nose";

static const unsigned char bip39_fourth_checksum_bit_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "north";

static void test_bip39_decode_checksum_mask_covers_fourth_bit(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x62, 0x50, 0xb6, 0x8d, 0xaf, 0x74,
                                        0x6d, 0x12, 0xa2, 0x4d, 0x58, 0xb4,
                                        0x78, 0x7a, 0x71, 0x4b};
    unsigned char bits[32 + 1];

    assert_int_equal(
        bolos_ux_bip39_mnemonic_decode(bip39_valid_12_word_mnemonic,
                                       sizeof(bip39_valid_12_word_mnemonic) - 1,
                                       bits, sizeof(bits)),
        1);
    assert_memory_equal(bits, entropy, sizeof(entropy));

    assert_int_equal(
        bolos_ux_bip39_mnemonic_decode(
            bip39_fourth_checksum_bit_mnemonic,
            sizeof(bip39_fourth_checksum_bit_mnemonic) - 1, bits, sizeof(bits)),
        0);
}

// Same phrase as above, but the last word is replaced with a 17-character
// string. bolos_ux_bip39_mnemonic_decode() accumulates each word into a
// local `unsigned char current_word[10]` and bails out as soon as a word
// does not fit, before ever consulting the wordlist -- so this vector
// exercises the buffer guard, not the wordlist lookup. Under the sanitizer
// this target is built with, dropping that guard is a stack overrun rather
// than a silently wrong answer, which is why the case is worth naming.
static const unsigned char bip39_overlong_word_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "notarealbip39word";

static void test_bip39_decode_word_too_long_for_buffer(void** state) {
    (void)state;
    unsigned char bits[32 + 1];

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        bip39_overlong_word_mnemonic, sizeof(bip39_overlong_word_mnemonic) - 1,
        bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// Same phrase again, with a last word that is absent from the BIP-39 English
// wordlist *and* short enough to reach the lookup: the longest word in the
// list is 8 characters and `current_word` holds 10, so "abandonx" clears the
// buffer guard above and reaches the "no match in the wordlist" guard, which
// is the one this test is named for. No 8-character entry of the list starts
// with "aband", and the lookup requires an exact length match as well as an
// exact prefix, so "abandon" cannot match it either.
//
// What this test does and does not pin down, measured rather than assumed:
// it is the first test in the suite to reach that guard at all (its two
// lines had a zero execution count before), but the guard turns out to be
// redundant for the return value. An unmatched word contributes no bits, so
// `bi` ends short of `n * 11` and the check below the loop rejects the
// phrase anyway; deleting the wordlist guard leaves this test green, with
// that second check firing exactly once in its place. So this asserts that
// an out-of-list word is rejected -- not that any one guard does it.
static const unsigned char bip39_unknown_word_mnemonic[] =
    "girl mad pet galaxy egg matter matrix prison refuse sense ordinary "
    "abandonx";

static void test_bip39_decode_unknown_word(void** state) {
    (void)state;
    unsigned char bits[32 + 1];

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        bip39_unknown_word_mnemonic, sizeof(bip39_unknown_word_mnemonic) - 1,
        bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// Decodes the 12-word phrase above with its last word replaced, and asserts
// the phrase is rejected. The word count is what it has to be for the lookup
// to be reached at all, so the replacement has to keep it at 12.
static void assert_last_word_rejected(const char* last_word) {
    static const char prefix[] =
        "girl mad pet galaxy egg matter matrix prison refuse sense ordinary ";
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t word_len = strlen(last_word);
    unsigned char mnemonic[sizeof(prefix) + 16];
    unsigned char bits[32 + 1];

    assert_true(prefix_len + word_len <= sizeof(mnemonic));
    memcpy(mnemonic, prefix, prefix_len);
    memcpy(mnemonic + prefix_len, last_word, word_len);

    unsigned int result = bolos_ux_bip39_mnemonic_decode(
        mnemonic, (unsigned int)(prefix_len + word_len), bits, sizeof(bits));

    assert_int_equal(result, 0);
}

// BIP39_WORDLIST stores the 2048 entries back to back with no separator and
// BIP39_WORDLIST_OFFSETS says where each one starts: 11068 bytes in all, the
// last entry ("zoo") at offset 11065 for 3 bytes. The lookup in
// bolos_ux_bip39_mnemonic_decode() compares current_word_size bytes -- the
// length of the *typed* word, not of the entry -- so a word longer than the
// entry it is held against reads past that entry, and past the array itself
// on the last ones. os_secure_memcmp() is constant time and never short
// circuits, so it reads every one of those bytes rather than stopping at the
// first difference.
//
// It takes two things at once to get there: a word absent from the list, so
// that the scan is not stopped early by the break on a match, and a word
// longer than 3 characters, so that the read runs off the end once the scan
// reaches "zoo". A word that is in the list can do neither -- it stops the
// scan at its own index, and no entry extends past the end of the array.
//
// The lengths below cover the whole range that can reach the lookup: 4 is
// the shortest that reads past "zoo" (by 1 byte), 10 is the longest the
// current_word[10] buffer accepts, and reads 7 bytes past the end of the
// array -- the worst case. Under the sanitizer this target is built with,
// each of those reads is a failure; without one they land in whatever the
// linker put after the array, and the phrase is rejected all the same, so
// the assertion holds in both builds and only the sanitizer tells them
// apart.
static void test_bip39_decode_word_longer_than_last_wordlist_entry(
    void** state) {
    (void)state;
    static const char* const absent_words[] = {
        "zzzz",     "zzzzz",     "zzzzzz",    "zzzzzzz",
        "zzzzzzzz", "zzzzzzzzz", "zzzzzzzzzz"};

    for (size_t i = 0; i < sizeof(absent_words) / sizeof(absent_words[0]);
         i++) {
        assert_last_word_rejected(absent_words[i]);
    }
}

// The counterpart of the case above: words that are exactly as long as the
// last entry of the wordlist and still absent from it. They reach the last
// entry like the ones above but read nothing past it, so the length check
// lets the comparison through and the comparison alone has to reject them.
// Without this, a length check that rejected everything -- or a comparison
// that no longer ran -- would look just as green as a correct one.
static void test_bip39_decode_three_letter_word_absent_from_wordlist(
    void** state) {
    (void)state;

    assert_last_word_rejected("zzz");
    assert_last_word_rejected("aaa");
    assert_last_word_rejected("qqq");
}

// bolos_ux_bip39_mnemonic_decode() rejects a phrase purely on word count
// before looking at word content, so the content here does not matter --
// only the number of space-separated words.
static void test_bip39_decode_wrong_length(void** state) {
    (void)state;
    static const unsigned int word_counts[] = {11, 13, 17, 19, 20, 23, 25};
    unsigned char bits[32 + 1];
    unsigned char mnemonic[9 * 25];

    for (size_t i = 0; i < sizeof(word_counts) / sizeof(word_counts[0]); i++) {
        unsigned int words = word_counts[i];
        size_t offset = 0;

        for (unsigned int w = 0; w < words; w++) {
            memcpy(mnemonic + offset, "abandon", 7);
            offset += 7;
            if (w < words - 1) {
                mnemonic[offset++] = ' ';
            }
        }

        unsigned int result = bolos_ux_bip39_mnemonic_decode(
            mnemonic, offset, bits, sizeof(bits));

        assert_int_equal(result, 0);
    }
}

// 16 bytes of entropy encode to a 74-character mnemonic; an output buffer of
// 10 bytes is far too small for even the first two words, so encode() must
// fail cleanly (return 0) instead of writing past the buffer it was given.
//
// The returned 0 is only half the assertion, and on its own it is not the
// interesting half: encode() checks `offset` again after each memcpy(), so
// it still returns 0 when the length guard that precedes the copy is gone --
// only after the copy has already run off the end. This target is built
// with AddressSanitizer (see the CMake comment on it), so `out` is a
// redzoned array and the write itself is observed rather than inferred.
static void test_bip39_encode_insufficient_buffer(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                        0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                        0x0c, 0x0d, 0x0e, 0x0f};
    unsigned char out[10];

    unsigned int result = bolos_ux_bip39_mnemonic_encode(
        entropy, sizeof(entropy), out, sizeof(out));

    assert_int_equal(result, 0);
}

// The same guard as the test above, seen one byte further along. It bounds
// the word about to be copied, but not the separator that follows it:
//
//     if ((offset + word_len) > out_len) {  // equality accepted
//         ...
//     }
//     memcpy(out + offset, ..., word_len);
//     offset += word_len;                   // offset == out_len now possible
//     if (offset > out_len) {               // so this can never fire
//         ...
//     }
//     if (i < (seed_len * 3 / 4) - 1) {
//         out[offset++] = ' ';              // writes out[out_len]
//     }
//
// When a capacity lands exactly on a word boundary and words still remain,
// the first guard accepts the word, `offset` becomes exactly out_len, the
// second guard cannot fire, and the separator goes one byte past the end.
//
// The return value does not tell the two behaviours apart. A separator is
// only written when a word follows, and after the overrun `offset` is
// out_len + 1, so that next word never fits and encode() returns 0 either
// way -- as it already did. The observable is the write itself, so each
// capacity is tried twice: once into a buffer followed by sentinel bytes,
// which holds with or without a sanitizer, and once into a heap block sized
// exactly to the capacity, which AddressSanitizer redzones (this target is
// built with it, see the CMake comment).
//
// Not reachable from the application. Both callers pass a fixed capacity --
// BIP39_MNEMONIC_MAX_LENGTH = 24 * (8 + 1) = 216 in src/nbgl/bip39_mnemonic.c,
// and the caller-owned words_buffer handed to src/common/sskr/seed_sskr.c,
// 257 on the BAGL side -- while the longest possible mnemonic is
// 24 * 8 + 23 = 215 characters, so `offset` never reaches out_len there.
// bolos_ux_bip39_mnemonic_encode() is declared in bip39/common_bip39.h and
// takes out_len from whoever calls it, which is what the bound is for.

// Entropy 00 01 .. 0f -- the vector used by the test above -- encodes to the
// 74-character
//
//     abandon amount liar amount expire adjust cage candy arch gather
//     drum buyer
//
// whose word lengths are 7 6 4 6 6 6 4 5 4 6 4 5. Counting the separators,
// `offset` sits at each of the values below right after copying a word that
// still has successors: these are exactly the capacities that trip the
// guard. 74, the offset after the last word, is not among them -- no
// separator follows it.
#define ENCODE_ENTROPY_MNEMONIC_LENGTH (74)
#define ENCODE_SENTINEL_BYTE (0xa5)
#define ENCODE_SENTINEL_LENGTH (16)

static const uint8_t encode_entropy[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                           0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                           0x0c, 0x0d, 0x0e, 0x0f};

static const char encode_entropy_mnemonic[] =
    "abandon amount liar amount expire adjust cage candy arch gather drum "
    "buyer";

static const size_t encode_word_boundaries[] = {7,  14, 19, 26, 33, 40,
                                                45, 51, 56, 63, 68};

static void test_bip39_encode_separator_stays_in_bounds(void** state) {
    (void)state;

    for (size_t k = 0;
         k < sizeof(encode_word_boundaries) / sizeof(encode_word_boundaries[0]);
         k++) {
        const size_t capacity = encode_word_boundaries[k];
        unsigned char
            block[ENCODE_ENTROPY_MNEMONIC_LENGTH + ENCODE_SENTINEL_LENGTH];

        memset(block, ENCODE_SENTINEL_BYTE, sizeof(block));

        // none of these capacities can hold the whole mnemonic, so a clean
        // failure is the only correct answer -- a non-zero return here would
        // be a truncated mnemonic reported as a success, which is worse than
        // the overrun being fixed
        assert_int_equal(
            bolos_ux_bip39_mnemonic_encode(
                encode_entropy, sizeof(encode_entropy), block, capacity),
            0);

        // nothing at or past out[out_len] may have been touched
        for (size_t i = capacity; i < sizeof(block); i++) {
            assert_int_equal(block[i], ENCODE_SENTINEL_BYTE);
        }
    }
}

// Same capacities, destination alone in a heap block of exactly that size, so
// the byte past the end belongs to AddressSanitizer's redzone rather than to
// the sentinel above.
static void test_bip39_encode_separator_stays_in_bounds_heap(void** state) {
    (void)state;

    for (size_t k = 0;
         k < sizeof(encode_word_boundaries) / sizeof(encode_word_boundaries[0]);
         k++) {
        const size_t capacity = encode_word_boundaries[k];
        unsigned char* out = malloc(capacity);

        assert_non_null(out);
        assert_int_equal(
            bolos_ux_bip39_mnemonic_encode(
                encode_entropy, sizeof(encode_entropy), out, capacity),
            0);
        free(out);
    }
}

// The bound must not be tightened past what the mnemonic actually needs: a
// capacity of exactly its length still has to encode, and return that length.
// One byte less is the last failing capacity, and it must fail cleanly rather
// than return a truncated phrase.
static void test_bip39_encode_exact_capacity_succeeds(void** state) {
    (void)state;
    unsigned char
        block[ENCODE_ENTROPY_MNEMONIC_LENGTH + ENCODE_SENTINEL_LENGTH];

    memset(block, ENCODE_SENTINEL_BYTE, sizeof(block));

    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(encode_entropy, sizeof(encode_entropy),
                                       block, ENCODE_ENTROPY_MNEMONIC_LENGTH),
        ENCODE_ENTROPY_MNEMONIC_LENGTH);
    assert_memory_equal(block, encode_entropy_mnemonic,
                        ENCODE_ENTROPY_MNEMONIC_LENGTH);
    for (size_t i = ENCODE_ENTROPY_MNEMONIC_LENGTH; i < sizeof(block); i++) {
        assert_int_equal(block[i], ENCODE_SENTINEL_BYTE);
    }

    memset(block, ENCODE_SENTINEL_BYTE, sizeof(block));

    assert_int_equal(bolos_ux_bip39_mnemonic_encode(
                         encode_entropy, sizeof(encode_entropy), block,
                         ENCODE_ENTROPY_MNEMONIC_LENGTH - 1),
                     0);
    for (size_t i = ENCODE_ENTROPY_MNEMONIC_LENGTH - 1; i < sizeof(block);
         i++) {
        assert_int_equal(block[i], ENCODE_SENTINEL_BYTE);
    }
}

// bolos_ux_bip39_mnemonic_encode() rejects seed_len before touching `out` if
// it fails any of three conditions: not a multiple of 4, below 16, or above
// 32. Only the valid lengths (16/24/32) are exercised elsewhere (the
// roundtrip tests below); each case here isolates exactly one condition,
// satisfying the other two, and a generously sized `out` confirms the
// rejection is a clean early return (0) rather than a controlled partial
// write.
static void test_bip39_encode_rejects_invalid_seed_len(void** state) {
    (void)state;
    static const uint8_t seed_bytes[36] = {0};
    unsigned char out[256];

    // Not a multiple of 4, but within [16, 32].
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 17, out, sizeof(out)), 0);

    // Multiple of 4, but below 16.
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 12, out, sizeof(out)), 0);

    // Multiple of 4, but above 32.
    assert_int_equal(
        bolos_ux_bip39_mnemonic_encode(seed_bytes, 36, out, sizeof(out)), 0);
}

// The three entropy/mnemonic pairs below are the same independently-verified
// vectors already used by tests/bip85_bip39_entropy.c (12/18/24 words).
static void run_roundtrip(const uint8_t* entropy, uint8_t entropy_len,
                          const char* mnemonic) {
    unsigned char encoded[256];
    size_t mnemonic_len = strlen(mnemonic);

    unsigned int encoded_len = bolos_ux_bip39_mnemonic_encode(
        entropy, entropy_len, encoded, sizeof(encoded));

    assert_int_equal(encoded_len, mnemonic_len);
    assert_memory_equal(encoded, mnemonic, mnemonic_len);

    unsigned char bits[32 + 1];
    unsigned int decode_result = bolos_ux_bip39_mnemonic_decode(
        (const unsigned char*)mnemonic, mnemonic_len, bits, sizeof(bits));

    assert_int_equal(decode_result, 1);
    assert_memory_equal(bits, entropy, entropy_len);
}

static void test_bip39_roundtrip_12_words(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x62, 0x50, 0xb6, 0x8d, 0xaf, 0x74,
                                        0x6d, 0x12, 0xa2, 0x4d, 0x58, 0xb4,
                                        0x78, 0x7a, 0x71, 0x4b};

    run_roundtrip(entropy, sizeof(entropy),
                  "girl mad pet galaxy egg matter matrix prison refuse "
                  "sense ordinary nose");
}

static void test_bip39_roundtrip_18_words(void** state) {
    (void)state;
    static const uint8_t entropy[24] = {
        0x93, 0x80, 0x33, 0xed, 0x8b, 0x12, 0x69, 0x84, 0x49, 0xd4, 0xbb, 0xca,
        0x3c, 0x85, 0x3c, 0x66, 0xb2, 0x93, 0xea, 0x1b, 0x1c, 0xe9, 0xd9, 0xdc};

    run_roundtrip(entropy, sizeof(entropy),
                  "near account window bike charge season chef number "
                  "sketch tomorrow excuse sniff circle vital hockey "
                  "outdoor supply token");
}

static void test_bip39_roundtrip_24_words(void** state) {
    (void)state;
    static const uint8_t entropy[32] = {
        0xae, 0x13, 0x1e, 0x23, 0x12, 0xcd, 0xc6, 0x13, 0x31, 0x54, 0x2e,
        0xfe, 0x0d, 0x10, 0x77, 0xba, 0xc5, 0xea, 0x80, 0x3a, 0xdf, 0x24,
        0xb3, 0x13, 0xa4, 0xf0, 0xe4, 0x8e, 0x9c, 0x51, 0xf3, 0x7f};

    run_roundtrip(entropy, sizeof(entropy),
                  "puppy ocean match cereal symbol another shed magic "
                  "wrap hammer bulb intact gadget divorce twin tonight "
                  "reason outdoor destroy simple truth cigar social "
                  "volcano");
}

// Non-regression for the order the lookup evaluates its two conditions in:
// the entries of the wordlist are still found, whatever their length and
// wherever they sit. "abandon" is entry 0 and "zoo" entry 2047, the last one
// -- looking "zoo" up walks the scan all the way to the entry the overread
// was about -- and between them the phrase covers every length the list
// holds, 3 to 8 characters. The checksum is what makes it a valid phrase, so
// the roundtrip only passes if all twelve words are matched at their own
// index.
static void test_bip39_roundtrip_wordlist_bounds(void** state) {
    (void)state;
    static const uint8_t entropy[16] = {0x00, 0x1f, 0xff, 0xff, 0x00, 0x7f,
                                        0xf5, 0xfe, 0x00, 0x17, 0xfc, 0xff,
                                        0xbf, 0xec, 0x04, 0x00};

    run_roundtrip(entropy, sizeof(entropy),
                  "abandon zoo zone abstract young yellow able zebra zero "
                  "youth absurd achieve");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_bip39),
        cmocka_unit_test(test_bip39_seed_short_mnemonic),
        cmocka_unit_test(test_bip39_seed_length_boundary),
        cmocka_unit_test(test_bip39_decode_word_too_long_for_buffer),
        cmocka_unit_test(test_bip39_decode_unknown_word),
        cmocka_unit_test(
            test_bip39_decode_word_longer_than_last_wordlist_entry),
        cmocka_unit_test(
            test_bip39_decode_three_letter_word_absent_from_wordlist),
        cmocka_unit_test(test_bip39_decode_bad_checksum),
        cmocka_unit_test(test_bip39_decode_checksum_mask_covers_fourth_bit),
        cmocka_unit_test(test_bip39_decode_wrong_length),
        cmocka_unit_test(test_bip39_encode_insufficient_buffer),
        cmocka_unit_test(test_bip39_encode_separator_stays_in_bounds),
        cmocka_unit_test(test_bip39_encode_separator_stays_in_bounds_heap),
        cmocka_unit_test(test_bip39_encode_exact_capacity_succeeds),
        cmocka_unit_test(test_bip39_encode_rejects_invalid_seed_len),
        cmocka_unit_test(test_bip39_roundtrip_12_words),
        cmocka_unit_test(test_bip39_roundtrip_18_words),
        cmocka_unit_test(test_bip39_roundtrip_24_words),
        cmocka_unit_test(test_bip39_roundtrip_wordlist_bounds),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
