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
