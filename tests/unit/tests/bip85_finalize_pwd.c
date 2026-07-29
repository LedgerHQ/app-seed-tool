#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BASE64_ENCODE_LENGTH 88
#define BASE85_ENCODE_LENGTH 80

extern uint8_t bip85_finalize_pwd(const char* buffer_pwd, char* pwd,
                                  uint8_t pwd_len);

// Same fully-encoded vectors as tests/base64.c/base85.c -- already verified
// there against base64_encode_64bytes()/base85_encode_64bytes() directly.
// bip85_finalize_pwd() only ever sees this kind of fully-encoded buffer as
// input, so reusing it here (rather than an arbitrary string) keeps this
// test representative of the real call site.
static const char base64_encoded[] =
    "dKLoepugzdVJvdL56ogNVUxsNVsI7SUIjPqI8/"
    "HE90YytlL9So9f2kMHTG9pZKN1Owi7UhDI9edcB6TCogv26Q==";
static const char base85_encoded[] =
    "_s`{TW89)i4`uwnp5{Hxh0%|78*5%18;3KmWLe1sr(S}zqAvgWx#peX6iX>OsBuFXpj5WYRf1}"
    "cQ9@D)";

#define SENTINEL 0xaa

// pwd_len 20 and 86 (== BASE64_ENCODE_LENGTH - 2) are the user-facing length
// bounds of bolos_ux_bip85_pwd_base64() (seed_bip85.c). The destination is
// prefilled with a sentinel byte across its whole capacity so that both "did
// it copy the right prefix" and "did it touch anything past pwd_len + 1" are
// checked in the same test.
static void test_finalize_pwd_base64_length(uint8_t pwd_len) {
    char pwd[BASE64_ENCODE_LENGTH];
    memset(pwd, SENTINEL, sizeof(pwd));

    uint8_t returned = bip85_finalize_pwd(base64_encoded, pwd, pwd_len);

    assert_int_equal(returned, pwd_len);
    assert_memory_equal(pwd, base64_encoded, pwd_len);
    assert_int_equal((uint8_t)pwd[pwd_len], 0);
    for (size_t i = (size_t)pwd_len + 1; i < sizeof(pwd); i++) {
        assert_int_equal((uint8_t)pwd[i], SENTINEL);
    }
}

static void test_finalize_pwd_base64_min_length(void** state) {
    (void)state;
    test_finalize_pwd_base64_length(20);
}

static void test_finalize_pwd_base64_max_length(void** state) {
    (void)state;
    test_finalize_pwd_base64_length(BASE64_ENCODE_LENGTH - 2);
}

// pwd_len 10 and 80 (== BASE85_ENCODE_LENGTH) are the user-facing length
// bounds of bolos_ux_bip85_pwd_base85() (seed_bip85.c). At pwd_len ==
// BASE85_ENCODE_LENGTH there is no room left in `pwd` for a sentinel past
// `pwd_len + 1`, so this case only checks the copy and the terminator.
static void test_finalize_pwd_base85_length(uint8_t pwd_len) {
    char pwd[BASE85_ENCODE_LENGTH + 1];
    memset(pwd, SENTINEL, sizeof(pwd));

    uint8_t returned = bip85_finalize_pwd(base85_encoded, pwd, pwd_len);

    assert_int_equal(returned, pwd_len);
    assert_memory_equal(pwd, base85_encoded, pwd_len);
    assert_int_equal((uint8_t)pwd[pwd_len], 0);
    for (size_t i = (size_t)pwd_len + 1; i < sizeof(pwd); i++) {
        assert_int_equal((uint8_t)pwd[i], SENTINEL);
    }
}

static void test_finalize_pwd_base85_min_length(void** state) {
    (void)state;
    test_finalize_pwd_base85_length(10);
}

static void test_finalize_pwd_base85_max_length(void** state) {
    (void)state;
    test_finalize_pwd_base85_length(BASE85_ENCODE_LENGTH);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_finalize_pwd_base64_min_length),
        cmocka_unit_test(test_finalize_pwd_base64_max_length),
        cmocka_unit_test(test_finalize_pwd_base85_min_length),
        cmocka_unit_test(test_finalize_pwd_base85_max_length),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
