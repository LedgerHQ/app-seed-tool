/*
 * Regression test for the erase-nothing calls in seed_sskr.c.
 *
 * Several error paths clear a caller-supplied buffer with
 *
 *     memzero(buffer, sizeof(buffer));
 *
 * where `buffer` is a pointer parameter. sizeof() then yields the size of the
 * pointer, not of the buffer, so only the first few bytes are erased and the
 * secret material the call was meant to destroy stays in RAM.
 *
 * The same file gets this right elsewhere (see the memzero() at the end of
 * bolos_ux_bip39_to_sskr_convert(), which passes the real length), which is
 * what marks these as mistakes rather than a deliberate budget.
 *
 * bolos_ux_sskr_share_hex_decode() is used here because it is the one such
 * path reachable with a pure function call: asking it to write more bytewords
 * than the output can hold makes it bail out and clear the output.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "testutils.h"
#include "sskr/common_sskr.h"

static void test_hex_decode_clears_whole_output_on_overflow(void **state)
{
    (void) state;

    /* Large enough that a partial erase is obvious, too small to hold the
     * decoded bytewords, so the function takes its failure path. */
    unsigned char output[64];
    unsigned char input[16];

    memset(output, 0xFF, sizeof(output));
    memset(input, 0x00, sizeof(input));

    unsigned int written =
        bolos_ux_sskr_share_hex_decode(input, sizeof(input), output, sizeof(output));

    assert_int_equal(written, 0);

    /* Every byte must be gone, not just the first sizeof(pointer) of them. */
    for (size_t i = 0; i < sizeof(output); i++) {
        if (output[i] != 0x00) {
            fail_msg("output[%zu] = 0x%02X, expected 0x00 (buffer not fully erased)",
                     i,
                     output[i]);
        }
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hex_decode_clears_whole_output_on_overflow),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
