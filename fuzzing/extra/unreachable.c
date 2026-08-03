/*
 * The one symbol the fuzzers link but must never call.
 *
 * sss.c calls interpolate() to split and to recover a secret. interpolate() is
 * written against the SDK's cx_bn_* big-number API, whose only host
 * implementation in this repository is tests/unit/lib/bolos/cx_bn.c plus
 * cx_mpi.c -- built on OpenSSL, which tests/unit/CMakeLists.txt compiles from
 * source for that reason. Linking it here would make a fuzzer build take
 * minutes instead of seconds, to fuzz finite-field arithmetic rather than a
 * parser.
 *
 * None of the targets in this directory reaches it: they stop at the CBOR
 * frame, the ByteWords table, the entry-time header arithmetic and the BIP-39
 * wordlist, all of which sit strictly before Shamir recovery. Defining it as
 * an abort rather than leaving it undefined is what makes that claim testable
 * -- a target that grows a path into sss_recover_secret() stops the run
 * immediately and visibly, instead of fuzzing a stub that returns a fixed
 * answer.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "cx_errors.h"

cx_err_t interpolate(uint8_t n,
                     const uint8_t *xi,
                     uint8_t yl,
                     const uint8_t **yij,
                     uint8_t x,
                     uint8_t *result);

cx_err_t interpolate(uint8_t n,
                     const uint8_t *xi,
                     uint8_t yl,
                     const uint8_t **yij,
                     uint8_t x,
                     uint8_t *result) {
    (void) n;
    (void) xi;
    (void) yl;
    (void) yij;
    (void) x;
    (void) result;

    fprintf(stderr,
            "interpolate() reached from a fuzz target: this build does not "
            "carry a big-number implementation, so the result would be "
            "meaningless. Link tests/unit/lib/bolos/cx_bn.c and cx_mpi.c "
            "(and OpenSSL) if this path is meant to be fuzzed.\n");
    abort();
}
