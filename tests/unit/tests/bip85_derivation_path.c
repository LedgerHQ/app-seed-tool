#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern unsigned int bip85_path_drng(unsigned int* path, unsigned int index);
extern unsigned int bip85_path_bip39(unsigned int* path, uint8_t language,
                                     uint8_t words, unsigned int index);
extern unsigned int bip85_path_hex(unsigned int* path, uint8_t num_bytes,
                                   unsigned int index);
extern unsigned int bip85_path_pwd_base64(unsigned int* path, uint8_t pwd_len,
                                          unsigned int index);
extern unsigned int bip85_path_pwd_base85(unsigned int* path, uint8_t pwd_len,
                                          unsigned int index);
extern unsigned int bip85_path_dice(unsigned int* path, uint32_t sides,
                                    uint32_t rolls, unsigned int index);

extern bool bip85_bip39_words_valid(uint8_t words);
extern bool bip85_hex_num_bytes_valid(uint8_t num_bytes);
extern bool bip85_pwd_base64_len_valid(uint8_t pwd_len);
extern bool bip85_pwd_base85_len_valid(uint8_t pwd_len);
extern bool bip85_dice_sides_valid(uint32_t sides);
extern bool bip85_dice_rolls_valid(uint32_t rolls);

// Everything the expected paths are built from is spelled out here as the
// decimal numbers BIP-85 itself uses, plus the BIP-32 hardening offset.
// Copying the hexadecimal literals out of seed_bip85.c instead would make
// these tests agree with the implementation by construction and catch
// nothing -- and a wrong constant is exactly the defect that has no other
// symptom: the device would still produce a well-formed mnemonic, password
// or dice sequence, just derived from a path no other BIP-85 implementation
// would ever visit.
//
// Numbers per BIP-85 ("Deterministic Entropy From BIP32 Keychains"):
//   purpose  83696968  ("BIPS" in ASCII, high bit of each byte cleared)
//   BIP39    39
//   HEX      128169
//   PWD BASE64 707764
//   PWD BASE85 707785
//   DICE     89101
//   DRNG     0
#define HARDENED_OFFSET 0x80000000u
#define HARDENED(n) (HARDENED_OFFSET | (uint32_t)(n))

#define BIP85_PURPOSE 83696968u
#define BIP85_APP_DRNG 0u
#define BIP85_APP_BIP39 39u
#define BIP85_APP_HEX 128169u
#define BIP85_APP_PWD_B64 707764u
#define BIP85_APP_PWD_B85 707785u
#define BIP85_APP_DICE 89101u

// Component counts, counted off the path layouts documented by BIP-85:
//   DRNG        m / purpose' / app' / index'                        -> 3
//   BIP39       m / purpose' / app' / language' / words' / index'   -> 5
//   HEX         m / purpose' / app' / num_bytes' / index'           -> 4
//   PWD BASE64  m / purpose' / app' / pwd_len' / index'             -> 4
//   PWD BASE85  m / purpose' / app' / pwd_len' / index'             -> 4
//   DICE        m / purpose' / app' / sides' / rolls' / index'      -> 5
#define EXPECTED_LEN_DRNG 3u
#define EXPECTED_LEN_BIP39 5u
#define EXPECTED_LEN_HEX 4u
#define EXPECTED_LEN_PWD_B64 4u
#define EXPECTED_LEN_PWD_B85 4u
#define EXPECTED_LEN_DICE 5u

// Builders are handed an over-sized buffer prefilled with a sentinel, so a
// builder that wrote one component too many (or reported a length shorter
// than what it wrote) is caught rather than silently tolerated.
#define PATH_CAPACITY 8
#define SENTINEL 0xdeadbeefu

static void fill_sentinel(unsigned int* path) {
    for (size_t i = 0; i < PATH_CAPACITY; i++) {
        path[i] = SENTINEL;
    }
}

// Checks, in one place, everything a derivation path has to satisfy: the
// number of components, each component's exact value, that every component
// is hardened, and that nothing past the reported length was written.
static void check_path(const unsigned int* path, unsigned int path_len,
                       const unsigned int* expected,
                       unsigned int expected_len) {
    assert_int_equal(path_len, expected_len);

    for (unsigned int i = 0; i < expected_len; i++) {
        assert_int_equal(path[i], expected[i]);
        // BIP-85 hardens every component of every path it defines.
        assert_true((path[i] & HARDENED_OFFSET) == HARDENED_OFFSET);
    }

    for (unsigned int i = expected_len; i < PATH_CAPACITY; i++) {
        assert_int_equal(path[i], SENTINEL);
    }
}

// ---------------------------------------------------------------------------
// DRNG -- m / 83696968' / 0' / index'
// ---------------------------------------------------------------------------

static void check_drng(unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_drng(path, index);

    const unsigned int expected[EXPECTED_LEN_DRNG] = {
        HARDENED(BIP85_PURPOSE),
        HARDENED(BIP85_APP_DRNG),
        HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_DRNG);
}

static void test_path_drng(void** state) {
    (void)state;
    check_drng(0);
    check_drng(1);
    check_drng(0x7fffffffu);
}

// ---------------------------------------------------------------------------
// BIP39 -- m / 83696968' / 39' / language' / words' / index'
// ---------------------------------------------------------------------------

static void check_bip39(uint8_t language, uint8_t words, unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_bip39(path, language, words, index);

    const unsigned int expected[EXPECTED_LEN_BIP39] = {
        HARDENED(BIP85_PURPOSE), HARDENED(BIP85_APP_BIP39), HARDENED(language),
        HARDENED(words),         HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_BIP39);
}

static void test_path_bip39(void** state) {
    (void)state;
    // language 0 is English, the only one this application offers; the other
    // values guard the position of the parameter in the path, not a feature.
    check_bip39(0, 12, 0);
    check_bip39(0, 24, 0);
    // The full set of mnemonic lengths the application accepts.
    check_bip39(0, 15, 1);
    check_bip39(0, 18, 2);
    check_bip39(0, 21, 3);
    check_bip39(9, 12, 0x7fffffffu);
    check_bip39(255, 255, 0xffffffffu);
}

// ---------------------------------------------------------------------------
// HEX -- m / 83696968' / 128169' / num_bytes' / index'
// ---------------------------------------------------------------------------

static void check_hex(uint8_t num_bytes, unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_hex(path, num_bytes, index);

    const unsigned int expected[EXPECTED_LEN_HEX] = {
        HARDENED(BIP85_PURPOSE),
        HARDENED(BIP85_APP_HEX),
        HARDENED(num_bytes),
        HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_HEX);
}

static void test_path_hex(void** state) {
    (void)state;
    // 16 and 64 are the length bounds bolos_ux_bip85_hex() accepts.
    check_hex(16, 0);
    check_hex(64, 0);
    check_hex(32, 1);
    check_hex(64, 0x7fffffffu);
    check_hex(255, 0xffffffffu);
}

// ---------------------------------------------------------------------------
// PWD BASE64 -- m / 83696968' / 707764' / pwd_len' / index'
// ---------------------------------------------------------------------------

static void check_pwd_base64(uint8_t pwd_len, unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_pwd_base64(path, pwd_len, index);

    const unsigned int expected[EXPECTED_LEN_PWD_B64] = {
        HARDENED(BIP85_PURPOSE),
        HARDENED(BIP85_APP_PWD_B64),
        HARDENED(pwd_len),
        HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_PWD_B64);
}

static void test_path_pwd_base64(void** state) {
    (void)state;
    // 20 and 86 are the length bounds bolos_ux_bip85_pwd_base64() accepts.
    check_pwd_base64(20, 0);
    check_pwd_base64(86, 0);
    check_pwd_base64(64, 1);
    check_pwd_base64(20, 0x7fffffffu);
    check_pwd_base64(255, 0xffffffffu);
}

// ---------------------------------------------------------------------------
// PWD BASE85 -- m / 83696968' / 707785' / pwd_len' / index'
// ---------------------------------------------------------------------------

static void check_pwd_base85(uint8_t pwd_len, unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_pwd_base85(path, pwd_len, index);

    const unsigned int expected[EXPECTED_LEN_PWD_B85] = {
        HARDENED(BIP85_PURPOSE),
        HARDENED(BIP85_APP_PWD_B85),
        HARDENED(pwd_len),
        HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_PWD_B85);
}

static void test_path_pwd_base85(void** state) {
    (void)state;
    // 10 and 80 are the length bounds bolos_ux_bip85_pwd_base85() accepts.
    check_pwd_base85(10, 0);
    check_pwd_base85(80, 0);
    check_pwd_base85(64, 1);
    check_pwd_base85(10, 0x7fffffffu);
    check_pwd_base85(255, 0xffffffffu);
}

// The two password applications differ only by their application number.
// Getting those two swapped is the single mistake most likely to go
// unnoticed -- both paths have the same shape and both still yield a
// perfectly usable password -- so it gets an explicit test.
static void test_path_pwd_base64_and_base85_differ(void** state) {
    (void)state;
    unsigned int path64[PATH_CAPACITY];
    unsigned int path85[PATH_CAPACITY];
    fill_sentinel(path64);
    fill_sentinel(path85);

    unsigned int len64 = bip85_path_pwd_base64(path64, 20, 0);
    unsigned int len85 = bip85_path_pwd_base85(path85, 20, 0);

    assert_int_equal(len64, len85);
    assert_int_equal(path64[1], HARDENED(707764u));
    assert_int_equal(path85[1], HARDENED(707785u));
    assert_int_not_equal(path64[1], path85[1]);
    // Everything except the application number must match.
    assert_int_equal(path64[0], path85[0]);
    assert_int_equal(path64[2], path85[2]);
    assert_int_equal(path64[3], path85[3]);
}

// ---------------------------------------------------------------------------
// DICE -- m / 83696968' / 89101' / sides' / rolls' / index'
// ---------------------------------------------------------------------------

static void check_dice(uint32_t sides, uint32_t rolls, unsigned int index) {
    unsigned int path[PATH_CAPACITY];
    fill_sentinel(path);

    unsigned int path_len = bip85_path_dice(path, sides, rolls, index);

    const unsigned int expected[EXPECTED_LEN_DICE] = {
        HARDENED(BIP85_PURPOSE), HARDENED(BIP85_APP_DICE), HARDENED(sides),
        HARDENED(rolls),         HARDENED(index),
    };
    check_path(path, path_len, expected, EXPECTED_LEN_DICE);
}

static void test_path_dice(void** state) {
    (void)state;
    // 2 and UINT32_MAX >> 1 are the bounds bolos_ux_bip85_dice() accepts for
    // sides; 1 and UINT32_MAX >> 1 for rolls.
    check_dice(2, 1, 0);
    check_dice(6, 10, 0);
    check_dice(0x7fffffffu, 0x7fffffffu, 0x7fffffffu);
    check_dice(20, 100, 1);
}

// A path whose parameters are already at or above the hardening offset must
// still come out hardened -- the OR cannot be turned into an addition
// without breaking exactly these cases.
static void test_hardening_survives_large_parameters(void** state) {
    (void)state;
    unsigned int path[PATH_CAPACITY];

    fill_sentinel(path);
    unsigned int path_len =
        bip85_path_dice(path, 0x7fffffffu, 0x7fffffffu, 0xffffffffu);
    for (unsigned int i = 0; i < path_len; i++) {
        assert_true((path[i] & HARDENED_OFFSET) == HARDENED_OFFSET);
    }
    assert_int_equal(path[2], 0xffffffffu);
    assert_int_equal(path[3], 0xffffffffu);
    assert_int_equal(path[4], 0xffffffffu);
}

// The purpose component is shared by all six applications; if it ever
// diverged between them, every application but one would derive off-spec.
static void test_purpose_is_common_to_all_paths(void** state) {
    (void)state;
    unsigned int path[PATH_CAPACITY];

    fill_sentinel(path);
    bip85_path_drng(path, 0);
    assert_int_equal(path[0], HARDENED(83696968u));

    fill_sentinel(path);
    bip85_path_bip39(path, 0, 12, 0);
    assert_int_equal(path[0], HARDENED(83696968u));

    fill_sentinel(path);
    bip85_path_hex(path, 16, 0);
    assert_int_equal(path[0], HARDENED(83696968u));

    fill_sentinel(path);
    bip85_path_pwd_base64(path, 20, 0);
    assert_int_equal(path[0], HARDENED(83696968u));

    fill_sentinel(path);
    bip85_path_pwd_base85(path, 10, 0);
    assert_int_equal(path[0], HARDENED(83696968u));

    fill_sentinel(path);
    bip85_path_dice(path, 6, 1, 0);
    assert_int_equal(path[0], HARDENED(83696968u));
}

// ---------------------------------------------------------------------------
// Parameter ranges
//
// These are the conditions the LEDGER_ASSERT()s in the bolos_ux_bip85_*
// entry points evaluate. LEDGER_ASSERT terminates the process, so the
// assertion itself still cannot be exercised from a unit test; extracting
// the condition makes both sides of every bound checkable without changing
// what happens when one is violated. Expected results below are written out
// as literals rather than recomputed from the same expression.
// ---------------------------------------------------------------------------

static void test_bip39_words_valid(void** state) {
    (void)state;
    // Accepted: 12, 15, 18, 21, 24.
    assert_true(bip85_bip39_words_valid(12));
    assert_true(bip85_bip39_words_valid(15));
    assert_true(bip85_bip39_words_valid(18));
    assert_true(bip85_bip39_words_valid(21));
    assert_true(bip85_bip39_words_valid(24));

    // Below the lower bound, including multiples of 3.
    assert_false(bip85_bip39_words_valid(0));
    assert_false(bip85_bip39_words_valid(9));
    assert_false(bip85_bip39_words_valid(11));

    // In range but not a multiple of 3.
    assert_false(bip85_bip39_words_valid(13));
    assert_false(bip85_bip39_words_valid(14));
    assert_false(bip85_bip39_words_valid(23));

    // Above the upper bound, including a multiple of 3.
    assert_false(bip85_bip39_words_valid(25));
    assert_false(bip85_bip39_words_valid(27));
    assert_false(bip85_bip39_words_valid(255));
}

static void test_hex_num_bytes_valid(void** state) {
    (void)state;
    assert_false(bip85_hex_num_bytes_valid(0));
    assert_false(bip85_hex_num_bytes_valid(15));
    assert_true(bip85_hex_num_bytes_valid(16));
    assert_true(bip85_hex_num_bytes_valid(32));
    assert_true(bip85_hex_num_bytes_valid(64));
    assert_false(bip85_hex_num_bytes_valid(65));
    assert_false(bip85_hex_num_bytes_valid(255));
}

static void test_pwd_base64_len_valid(void** state) {
    (void)state;
    assert_false(bip85_pwd_base64_len_valid(0));
    assert_false(bip85_pwd_base64_len_valid(19));
    assert_true(bip85_pwd_base64_len_valid(20));
    assert_true(bip85_pwd_base64_len_valid(86));
    assert_false(bip85_pwd_base64_len_valid(87));
    assert_false(bip85_pwd_base64_len_valid(88));
    assert_false(bip85_pwd_base64_len_valid(255));
}

static void test_pwd_base85_len_valid(void** state) {
    (void)state;
    assert_false(bip85_pwd_base85_len_valid(0));
    assert_false(bip85_pwd_base85_len_valid(9));
    assert_true(bip85_pwd_base85_len_valid(10));
    assert_true(bip85_pwd_base85_len_valid(80));
    assert_false(bip85_pwd_base85_len_valid(81));
    assert_false(bip85_pwd_base85_len_valid(255));
}

static void test_dice_sides_valid(void** state) {
    (void)state;
    assert_false(bip85_dice_sides_valid(0));
    // A one-sided die carries no entropy, and bip85_dice_bits_per_roll()
    // would evaluate __builtin_clz(0), which is undefined.
    assert_false(bip85_dice_sides_valid(1));
    assert_true(bip85_dice_sides_valid(2));
    assert_true(bip85_dice_sides_valid(6));
    assert_true(bip85_dice_sides_valid(0x7fffffffu));
    assert_false(bip85_dice_sides_valid(0x80000000u));
    assert_false(bip85_dice_sides_valid(0xffffffffu));
}

static void test_dice_rolls_valid(void** state) {
    (void)state;
    assert_false(bip85_dice_rolls_valid(0));
    assert_true(bip85_dice_rolls_valid(1));
    assert_true(bip85_dice_rolls_valid(100));
    assert_true(bip85_dice_rolls_valid(0x7fffffffu));
    assert_false(bip85_dice_rolls_valid(0x80000000u));
    assert_false(bip85_dice_rolls_valid(0xffffffffu));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_path_drng),
        cmocka_unit_test(test_path_bip39),
        cmocka_unit_test(test_path_hex),
        cmocka_unit_test(test_path_pwd_base64),
        cmocka_unit_test(test_path_pwd_base85),
        cmocka_unit_test(test_path_pwd_base64_and_base85_differ),
        cmocka_unit_test(test_path_dice),
        cmocka_unit_test(test_hardening_survives_large_parameters),
        cmocka_unit_test(test_purpose_is_common_to_all_paths),
        cmocka_unit_test(test_bip39_words_valid),
        cmocka_unit_test(test_hex_num_bytes_valid),
        cmocka_unit_test(test_pwd_base64_len_valid),
        cmocka_unit_test(test_pwd_base85_len_valid),
        cmocka_unit_test(test_dice_sides_valid),
        cmocka_unit_test(test_dice_rolls_valid),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
