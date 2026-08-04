/*******************************************************************************
 *   Ledger Seed Tool application
 *   (c) 2016-2026 Ledger SAS
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#pragma once

/*
 * Every user-visible string in the application, for both interface stacks --
 * BAGL (the three Nano targets) and NBGL (the touch targets). Nothing here
 * changes what a screen says; this header only gives the two stacks one place
 * to read their wording from, so a future change to one is not made without
 * seeing whether the other needs it too.
 *
 * The two stacks do not cut their text the same way. NBGL carries a full
 * sentence per screen; BAGL splits the same idea across the 2-4 lines of a
 * UX_STEP, and often uses different words entirely -- the two stacks already
 * diverged before this header existed (see the PR this header was added in).
 * Where the concept is shared but the wording is not, both forms are declared
 * here, next to each other, under one comment. Nothing is deduplicated by
 * inventing a shared string and a splitting algorithm: the divergence stays
 * visible, in this file, rather than being hidden by code that reconciles it.
 *
 * Where the wording happens to already be byte-identical between the two
 * stacks -- "Quit" on every BAGL screen that has it, "SSKR Share #%d" on
 * both -- a single macro serves every call site.
 *
 * `#define` rather than a table of `extern const char[]`: a string literal
 * behind a macro costs nothing beyond what the literal itself costs, and the
 * linker merges identical literals; an array of pointers would cost 4 bytes
 * of flash and a relocation per entry, everywhere it is linked in, including
 * `nanos`, which has the least flash of the six targets. The one thing a
 * table buys -- enumerating every string in a test -- is recovered without
 * that cost by tests/unit/test_ui_strings.c, which declares its own table
 * naming every macro below; nothing in this header needs to be enumerable
 * from device code.
 *
 * Two BAGL-only literals are deliberately not here:
 *   - the "1".."16" share-count menu labels in src/bagl/ux_sskr_menu.c are a
 *     generated table, not authored wording, and are sized by the
 *     TARGET_NANOS-conditional _Static_assert against SSS_MAX_SHARE_COUNT
 *     immediately above them in that file; moving the labels out would
 *     separate them from the invariant that bounds them;
 *   - PRINTF trace strings, on both stacks, which the user never sees.
 */

/*
 * Common to both stacks, byte-identical -- one macro, several call sites.
 */
#define UI_STR_QUIT               "Quit"
#define UI_STR_VERSION_LABEL      "Version"
#define UI_STR_BIP39_PHRASE_TITLE "BIP39 Phrase"
#define UI_STR_SSKR_SHARE_HEADER  "SSKR Share #%d"
#define UI_STR_WORDS_12           "12 words"
#define UI_STR_WORDS_18           "18 words"
#define UI_STR_WORDS_24           "24 words"

/*
 * "Processing", shown on nanos while a blocking comparison or SSKR
 * generation runs (src/bagl/nanos_enter_phrase.c), and on nanox/nanos+ for
 * the same wait (src/bagl/nanox_enter_phrase.c). Not used on NBGL, which has
 * no equivalent blocking step.
 */
#define UI_STR_BAGL_PROCESSING "Processing"

/*
 * Home / tool selection
 */

/* NBGL: three buttons on the "select a tool" screen (src/nbgl/ui.c). */
#define UI_STR_NBGL_TOOL_BIP39        "BIP39 Check"
#define UI_STR_NBGL_TOOL_SSKR         "SSKR Check"
#define UI_STR_NBGL_TOOL_BIP85        "BIP85 Generate"
#define UI_STR_NBGL_SELECT_TOOL_TITLE "Select the tool\nyou wish to use"

/*
 * BAGL: the equivalent two entries on the Nano idle menu
 * (src/bagl/ui.c). BAGL's idle menu has no third entry for BIP85 -- that
 * flow has no BAGL screens at all, on any Nano.
 */
#define UI_STR_BAGL_IDLE_BIP39_L1 "Check BIP39"
#define UI_STR_BAGL_IDLE_BIP39_L2 "recovery phrase"
#define UI_STR_BAGL_IDLE_SSKR_L1  "Check SSKR"
#define UI_STR_BAGL_IDLE_SSKR_L2  "recovery phrase"

/* NBGL home screen description and action (src/nbgl/ui.c). */
#define UI_STR_NBGL_HOME_DESCRIPTION \
    "This Ledger application\nprovides some useful seed\nmanagement utilities."
#define UI_STR_NBGL_HOME_ACTION    "Select Tool"
#define UI_STR_NBGL_HOME_COPYRIGHT "(c) 2018-2026 Ledger"

/*
 * BIP39 phrase length selection
 *
 * display_bip39_select_phrase_length_page() (src/nbgl/ui.c) serves two
 * different callers, distinguished by tool_type: checking an existing phrase
 * and generating a new one. BAGL has one screen for the same choice
 * (src/bagl/ui.c), worded for the Check flow only, and split 2 lines on
 * nanos, 3 lines on nanox/nanos+.
 */
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK    "How long is your\nBIP39 Recovery\nPhrase?"
#define UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE   "Length of BIP39\nphrase to\ngenerate?"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS "Enter number"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS "of BIP39 words"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L1       "Select the number of"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L2       "words written on"
#define UI_STR_BAGL_BIP39_LENGTH_TITLE_L3       "your Recovery Sheet"
#define UI_STR_BAGL_BIP39_LENGTH_BACK           "Back"

/*
 * SSKR entry start (src/bagl/ui.c). nanos gets 2 lines, nanox/nanos+ get 3
 * and start the flow directly from the step's own callback rather than a
 * shared init call -- an existing difference, not introduced here.
 */
#define UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS "Enter SSKR"
#define UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS "recovery phrase"
#define UI_STR_BAGL_SSKR_START_TITLE_L1       "Enter first word of"
#define UI_STR_BAGL_SSKR_START_TITLE_L2       "first share of SSKR"
#define UI_STR_BAGL_SSKR_START_TITLE_L3       "recovery phrase"

/*
 * Word / share entry keyboard
 *
 * NBGL carries one header sentence per keystroke (src/nbgl/ui.c). BAGL
 * rebuilds a short label into G_ux.string_buffer on every keystroke too, but
 * the exact wording differs between nanos (src/bagl/nanos_enter_phrase.c)
 * and nanox/nanos+ (src/bagl/nanox_enter_phrase.c) -- including the
 * capitalization of "word" and whether "Share" and "#" have a space between
 * them. None of that is touched here.
 */
#define UI_STR_NBGL_ENTER_BIP39_WORD "Enter word n. %d/%d of your\nBIP39 Recovery Phrase"
#define UI_STR_NBGL_ENTER_SSKR_WORD  "Enter Share %d Word %d\nof your Recovery Phrase"

/*
 * BAGL nanos (src/bagl/nanos_enter_phrase.c). UI_STR_BAGL_NANOS_WORD_HEADER_LOWER
 * has two call sites -- the "restart from" label and the entry-screen title
 * -- with byte-identical text; one macro serves both.
 */
#define UI_STR_BAGL_NANOS_WORD_HEADER       "Word #%d"
#define UI_STR_BAGL_NANOS_WORD_HEADER_LOWER "word #%d"
#define UI_STR_BAGL_NANOS_ENTER_SSKR_WORD   "Share#%d Word#%d"
#define UI_STR_BAGL_NANOS_RESTART_FROM      "Restart from"
#define UI_STR_BAGL_NANOS_WORD_INDEX_PREFIX "#%d "

/* BAGL nanox/nanos+ (src/bagl/nanox_enter_phrase.c). */
#define UI_STR_BAGL_NANOX_INTRO_L1         "Enter first letters"
#define UI_STR_BAGL_NANOX_INTRO_L2         "Next, enter letters"
#define UI_STR_BAGL_NANOX_INTRO_L3         "Finally, enter letters"
#define UI_STR_BAGL_NANOX_CLEAR_WORD       "Clear word"
#define UI_STR_BAGL_NANOX_ENTER_BIP39_WORD "Enter word #%d"
#define UI_STR_BAGL_NANOX_ENTER_SSKR_WORD  "Enter Share#%d word#%d"
#define UI_STR_BAGL_NANOX_OF_WORD          "of word #%d"
#define UI_STR_BAGL_NANOX_SELECT_WORD      "Select word #%d"

/*
 * Restore/entry navigation (src/bagl/ux_nano.c), shared by both the BIP39
 * and SSKR entry flows on every BAGL target.
 */
#define UI_STR_BAGL_ENTER_LABEL    "Enter"
#define UI_STR_BAGL_RETURN_TO_MENU "Return to menu"

/*
 * Check result / verdict
 *
 * NBGL (src/nbgl/ui.c) gives the "valid but doesn't match" and "valid and
 * matches" sub-cases the *same* title, "Valid Secret\nRecovery Phrase" --
 * only the body text differs. BAGL (src/bagl/ux_nano.c) gives them two
 * different titles, "doesn't match" vs. "is correct", on two distinct flows.
 * This is one of the two divergences that motivated this header; it is not
 * corrected here.
 */
#define UI_STR_NBGL_RESULT_INVALID_TITLE "Invalid Secret\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_VALID_TITLE   "Valid Secret\nRecovery Phrase"
#define UI_STR_NBGL_RESULT_BIP39_INVALID "The BIP39 Recovery Phrase\nyou have entered is not valid"
#define UI_STR_NBGL_RESULT_SSKR_INVALID  "The SSKR Recovery Phrase\nyou have entered is not valid"
#define UI_STR_NBGL_RESULT_BIP39_NOMATCH                                                          \
    "The BIP39 Recovery Phrase\nyou have entered\ndoesn't match the one present\non this Ledger " \
    "device."
#define UI_STR_NBGL_RESULT_BIP39_MATCH \
    "The BIP39 Recovery Phrase\nyou have entered\nmatches the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_SSKR_NOMATCH                                                          \
    "The SSKR Recovery Phrase\nyou have entered\ndoesn't match the one present\non this Ledger " \
    "device."
#define UI_STR_NBGL_RESULT_SSKR_MATCH \
    "The SSKR Recovery Phrase\nyou have entered\nmatches the one present\non this Ledger device."
#define UI_STR_NBGL_RESULT_TAP_TO_DISMISS "Tap to dismiss"

/*
 * BAGL invalid-phrase advice (src/bagl/ux_nano.c), shared by the BIP39 and
 * SSKR invalid flows. NBGL's invalid-result body text above carries no such
 * advice -- the other divergence that motivated this header.
 */
#define UI_STR_BAGL_INVALID_ADVICE_L1 "Check length,"
#define UI_STR_BAGL_INVALID_ADVICE_L2 "order and spelling"

#define UI_STR_BAGL_BIP39_INVALID_TITLE_L1 "BIP39 Recovery"
#define UI_STR_BAGL_BIP39_INVALID_TITLE_L2 "phrase invalid"
#define UI_STR_BAGL_BIP39_REENTER_PHRASE   "Re-enter phrase"
#define UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2 "doesn't match"
#define UI_STR_BAGL_BIP39_MATCH_TITLE_L2   "is correct"

#define UI_STR_BAGL_SSKR_INVALID_TITLE_L1 "SSKR Recovery"
#define UI_STR_BAGL_SSKR_INVALID_TITLE_L2 "phrase invalid"
#define UI_STR_BAGL_SSKR_REENTER_SHARES   "Re-enter shares"
#define UI_STR_BAGL_SSKR_NOMATCH_TITLE_L1 "SSKR Phrase"
#define UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2 "doesn't match"
#define UI_STR_BAGL_SSKR_MATCH_TITLE_L1   "SSKR Phrase"
#define UI_STR_BAGL_SSKR_MATCH_TITLE_L2   "is correct"

/*
 * Recover BIP39 from SSKR / Generate SSKR from BIP39 -- the two choice
 * screens offered right after a successful verdict.
 *
 * NBGL asks with a title + description + two buttons (src/nbgl/ui.c); BAGL
 * folds the same choice into the verdict flow's own steps
 * (src/bagl/ux_nano.c). Wording differs; declared together.
 */
#define UI_STR_NBGL_RECOVER_BIP39_TITLE "Recover BIP39 Phrase?"
#define UI_STR_NBGL_RECOVER_BIP39_DESC \
    "Choose if you wish to\nrecover the BIP39 phrase\nfrom your valid\nSSKR shares."
#define UI_STR_NBGL_RECOVER_BIP39_CONFIRM "Recover BIP39"
#define UI_STR_NBGL_DONE                  "Done"

#define UI_STR_NBGL_GENERATE_SSKR_TITLE "Generate SSKR Phrase?"
#define UI_STR_NBGL_GENERATE_SSKR_DESC \
    "Choose if you wish to\ngenerate SSKR shares from\nyour valid BIP39 phrase."
#define UI_STR_NBGL_GENERATE_SSKR_CONFIRM "Generate SSKR"

#define UI_STR_BAGL_GENERATE_SSKR_L1 "Generate"
#define UI_STR_BAGL_GENERATE_SSKR_L2 "SSKR phrases"
#define UI_STR_BAGL_RECOVER_BIP39_L1 "Recover"
#define UI_STR_BAGL_RECOVER_BIP39_L2 "BIP39 phrase"

/*
 * SSKR share count / threshold selection
 */
#define UI_STR_NBGL_SSKR_NUMSHARES_TITLE       "Enter number of SSKR shares\nto generate (1 - 16)"
#define UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR "Number of SSKR shares must be between 1 and 16"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1    "Select number"
#define UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2    "of shares"

#define UI_STR_NBGL_SSKR_THRESHOLD_TITLE      "Enter threshold value"
#define UI_STR_NBGL_SSKR_THRESHOLD_ZERO_ERROR "Threshold value cannot be 0"
#define UI_STR_NBGL_SSKR_THRESHOLD_RANGE_ERROR \
    "Threshold value cannot be greater than number of shares"
#define UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR "1-of-m shares where\nm > 1 is not supported"
#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1       "Select"
#define UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2       "threshold"
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1         "1-of-m shares"
#define UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2         "where m > 1"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1          "Not"
#define UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2          "Supported"

/*
 * BIP85 -- NBGL only. No BAGL target has a BIP85 screen at all.
 */
#define UI_STR_NBGL_BIP85_APP_BIP39        "BIP39"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE64   "Password (Base64)"
#define UI_STR_NBGL_BIP85_APP_PWD_BASE85   "Password (Base85)"
#define UI_STR_NBGL_BIP85_SELECT_APP_TITLE "Which BIP85\napplication do you\nwish to use?"

#define UI_STR_NBGL_BIP85_BIP39_HEADER  "BIP39 Phrase (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE64_HEADER "Base64 Password (Index #%d)"
#define UI_STR_NBGL_BIP85_BASE85_HEADER "Base85 Password (Index #%d)"

#define UI_STR_NBGL_BIP85_INDEX_TITLE       "Enter index"
#define UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR "BIP85 index must be between 0 and 9,999,999"

#define UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE       "Enter password length"
#define UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR "BIP85 password length must be between %d and %d"
