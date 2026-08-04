/*
 * The device syscalls the parsers call, standing in for BOLOS on a host.
 *
 * These are the same three stand-ins tests/unit/lib/testutils.c already
 * defines for the cmocka suite, repeated here rather than reused: that file
 * includes bolos/cxlib.h, which includes <openssl/bn.h> for the big-number
 * stand-in the fuzzers deliberately do not build (see extra/SeedParsers.cmake).
 * Any behaviour difference between the two would show up as a fuzzer finding
 * the unit tests cannot reproduce, so they are kept byte-for-byte identical;
 * if one changes, change both.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "cx_errors.h"

/* Declared by the SDK's ledger_assert_internals.h; defined here so a
 * LEDGER_ASSERT() firing inside any fuzzed function stops the run with a
 * non-zero status the fuzzing engine reports, rather than exiting quietly. */
void assert_exit(bool confirm);

void assert_exit(bool confirm) {
    (void) confirm;
    abort();
}

/* Fake random generator. Only bolos_ux_sskr_generate() uses it, and no fuzz
 * target reaches that; a deterministic filler is what keeps a run
 * reproducible if one ever does. */
void cx_rng_no_throw(uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buffer[i] = (uint8_t) i;
    }
}

/* The same filler behind the syscall seed_sskr.c actually calls. Unlike
 * cx_rng_no_throw() above this one has a status to return, which is why it is
 * the one share generation goes through; a host stand-in has nothing to fail
 * at, so it always succeeds. */
cx_err_t cx_get_random_bytes(void *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ((uint8_t *) buffer)[i] = (uint8_t) i;
    }
    return CX_OK;
}

/* Constant-time comparison. The wordlist searches and the CBOR/CRC checks all
 * go through it, so its bounds are part of what is being fuzzed: it reads
 * `length` bytes from both operands whatever they hold. */
char os_secure_memcmp(const void *src1, const void *src2, size_t length) {
#define SRC1 ((unsigned char const *) src1)
#define SRC2 ((unsigned char const *) src2)
    unsigned int l = length;
    unsigned char xoracc = 0;
    // don't || to ensure all condition are evaluated
    while (!(!length && !l)) {
        length--;
        xoracc |= SRC1[length] ^ SRC2[length];
        l--;
    }
    return xoracc;
}

/*
 * MemorySanitizer only: teach it that memzero() initialises its buffer.
 *
 * Ledger's ClusterFuzzLite workflow runs a matrix of sanitizers and `memory`
 * is one of them. MSan's shadow is only updated by the writes it can see, and
 * the OSS-Fuzz toolchain does not intercept explicit_bzero() -- which is what
 * `memzero()` expands to throughout src/. So a buffer this application clears
 * stays "uninitialized" as far as MSan is concerned, and the first read of it
 * is reported as a defect that is not there.
 *
 * It is not a hypothetical: `bits` in bolos_ux_bip39_mnemonic_check() is a
 * stack array that bolos_ux_bip39_mnemonic_decode() clears with memzero()
 * before OR-ing bits into it, so MSan reports seed_bip39.c:114 on any valid
 * recovery phrase -- including the seed corpus. Reduced to eight lines
 * (explicit_bzero() on a stack buffer, then read it) the same toolchain gives
 * the same report, and the memset() version of those eight lines is clean.
 *
 * Defining the symbol here puts it in the fuzz target's own object, which
 * resolves ahead of libc's, so every memzero() in src/ lands on this one. The
 * unpoison call is what the interceptor would have done. Guarded so that the
 * address, undefined and coverage builds keep calling the real thing: this is
 * about what MSan can see, not about what the application should do, and
 * explicit_bzero()'s reason for existing -- not being optimised away -- has no
 * bearing on a fuzzer.
 */
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)

#include <sanitizer/msan_interface.h>
#include <string.h>

void explicit_bzero(void *s, size_t len);

void explicit_bzero(void *s, size_t len) {
    memset(s, 0, len);
    __msan_unpoison(s, len);
}

#endif
#endif
