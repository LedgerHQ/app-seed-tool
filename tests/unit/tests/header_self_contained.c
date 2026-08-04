/*
 * One public header of src/common/, included first and with nothing before it.
 *
 * A header that uses uint8_t, size_t or bool without including <stdint.h>,
 * <stddef.h> or <stdbool.h> compiles anywhere its callers happened to pull
 * those in first, and nowhere else. Four of them were in that state, and it
 * only showed when a new translation unit put one of them at the top: the
 * `bool` added to sskr.h and sss/sss.h for the randomness callback would not
 * compile, and neither would a file whose first include was common_bip39.h.
 *
 * Nothing already in this suite can hold that. Every file that includes these
 * headers also includes os.h, cx.h or the standard headers ahead of them, so
 * the whole suite stays green with the dependency unmet.
 *
 * Neither can one file that includes them all in turn -- the first honest
 * attempt at this check was exactly that, and it did not work: the second
 * header in the list is satisfied by whatever the first one included, so
 * deleting the includes from common_sskr.h left it compiling. Each header
 * needs a translation unit to itself, which is what CMakeLists.txt builds by
 * compiling this same file once per header with a different
 * HEADER_UNDER_TEST.
 *
 * `WIDE` is a separate matter and deliberately outside the claim. It is the
 * SDK's storage qualifier, supplied by arch.h through os.h on the device and
 * by lib/testutils.h here; two of the three seed_rom_variables.h define a
 * fallback for it, sskr's does not. What these headers must not require of a
 * caller is the *standard* types.
 *
 * There is nothing to run. If every one of these objects compiles, the
 * property holds; the executable that depends on them is in
 * header_self_contained_main.c.
 */

#ifndef HEADER_UNDER_TEST
#error "HEADER_UNDER_TEST must name the header this translation unit checks"
#endif

#define WIDE

/* cx_err_t, which common.h needs for compare_recovery_phrase_finish(). A
 * device type rather than a standard one, so it is outside the claim -- but it
 * is pulled in only for the header that needs it, because cx_errors.h includes
 * <stdint.h> and would otherwise satisfy every header under test. That is
 * exactly what the second attempt at this check got wrong: with cx_errors.h in
 * front of all of them, stripping the includes from any header still
 * compiled. */
#ifdef HEADER_NEEDS_CX_ERRORS
/* common.h is the one header this check cannot falsify. It needs cx_err_t, so
 * cx_errors.h has to precede it, and cx_errors.h brings <stdint.h> and (through
 * ledger_assert.h) <stdbool.h> with it -- deleting common.h's own includes
 * still compiles. They are kept there because they state what the header uses,
 * but no test holds them. Every other header in the list is genuinely
 * falsifiable, and was checked that way. */
#include <cx_errors.h>
#endif

#include HEADER_UNDER_TEST
