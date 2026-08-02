#pragma once

/*
 * Stand-in for the SDK's lib_nbgl/include/nbgl_layout.h.
 *
 * lib/glyphs.h and lib/bolos_target.h are here for the same reason and are
 * empty: the harness only has to make an `#include` resolve. This one cannot
 * be empty, because the block it unblocks reads a value from it.
 *
 * `#if defined(HAVE_NBGL)` at the end of src/common/sskr/seed_sskr.c and
 * src/common/bip39/seed_bip39.c opens with `#include <nbgl_layout.h>`, and the
 * only symbol the four functions behind that guard take from it is
 * NB_MAX_SUGGESTION_BUTTONS. Nothing else in the real header is referenced
 * (MIN comes from the SDK's os.h, PRINTF from its debug header, and
 * ALPHABET_LENGTH/KBD_LETTERS from src/common/common.h), so nothing else is
 * reproduced here.
 *
 * The value is not a free choice: it is the cap that
 * bolos_ux_*_fill_with_candidates() applies with MIN() before writing into
 * buffers its caller sized from the same constant, so it decides what the
 * tests are actually testing. It is copied from the SDK, per target, with the
 * SDK's own conditional structure:
 *
 *   ledger-secure-sdk, lib_nbgl/include/nbgl_layout.h (tag
 *   stax_1.10.0-tr1-293-g4cd6b839, the revision shipped in
 *   ghcr.io/ledgerhq/ledger-app-builder/ledger-app-dev-tools:latest):
 *
 *     #ifdef HAVE_SE_TOUCH
 *     #if defined(TARGET_STAX)
 *     #define NB_MAX_SUGGESTION_BUTTONS 12    (line 38)
 *     #elif defined(TARGET_FLEX)
 *     #define NB_MAX_SUGGESTION_BUTTONS 8     (line 68)
 *     #elif defined(TARGET_APEX)
 *     #define NB_MAX_SUGGESTION_BUTTONS 8     (line 98)
 *     ...
 *     #else  // HAVE_SE_TOUCH
 *     #define NB_MAX_SUGGESTION_BUTTONS 8     (line 141)
 *
 * Of those, only the first three are live for this application. HAVE_NBGL is
 * defined for stax, flex and apex_p; the other three devices in
 * ledger_app.toml (nanos, nanox, nanos+) build with HAVE_BAGL, since the SDK
 * only turns NBGL on for TARGET_NANOX/TARGET_NANOS2 when USE_NBGL=1 and this
 * application's Makefile does not set it. So the constant is 12 on one device
 * and 8 on the other two, and both values are exercised: each test file below
 * is compiled into two executables, one with TARGET_STAX and one with
 * TARGET_FLEX.
 *
 * The `#else` is an error rather than a default for the same reason the SDK
 * writes `#error Undefined target` there: a silent fallback would let a test
 * target compile against a number no device uses.
 */

#if defined(TARGET_STAX)
#define NB_MAX_SUGGESTION_BUTTONS 12
#elif defined(TARGET_FLEX) || defined(TARGET_APEX)
#define NB_MAX_SUGGESTION_BUTTONS 8
#else
#error \
    "No target defined: NB_MAX_SUGGESTION_BUTTONS is target-dependent in the SDK and must not be guessed"
#endif
