/*
 * ui_strings.h is the first piece of interface the application can test in
 * this suite: src/nbgl/ui.c and the files under src/bagl/ are not compiled
 * into any unit target, so this is the only net any of these strings gets
 * other than the functional tests under Speculos.
 *
 * Three things are checked:
 *
 *   - every string is non-empty. A macro whose value became "" by accident
 *     (a bad merge, a copy-paste that dropped the literal) is otherwise
 *     invisible until someone runs the app;
 *
 *   - every BAGL fragment destined for a fixed (non-wrapping) Nano layout
 *     fits the real pixel budget of that layout. NN/NNN/PBB/PNN/PB/BN steps
 *     do not wrap or crop. A string that does not fit is silently clipped at
 *     runtime; nothing else catches it. The BNNN_PAGING *body* is excluded --
 *     it wraps, so it cannot clip;
 *
 *   - the two labels that head a BNNN_PAGING page fit its *title* line, which
 *     does clip. They are checked on their own, at the end of this file,
 *     because what reaches the screen is the macro plus a page counter the
 *     widget appends and the string does not contain -- which is how the
 *     phrase title came to ship clipped while passing the table above.
 *
 * Each entry below is `ENTRY(UI_STR_X)`, which expands to `{"UI_STR_X",
 * UI_STR_X}` -- the name and the header's own macro, not a retyped copy of
 * its value. That is what "declares its own table naming every macro" in
 * ui_strings.h's own comment means: nothing here can drift from the header,
 * because nothing here repeats it by hand. A renamed or removed macro is a
 * compile error in this file, not a silent gap in the table.
 */

#include "ui_strings.h"

#include <cmocka.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "constants.h"
#include "sss-constants.h"

#define ENTRY(name) {#name, name}
#define BOUNDED(name, budget_px, is_bold) {#name, name, budget_px, is_bold}

/*
 * Every macro in src/common/ui_strings.h. Kept in the order the header
 * declares them, so a diff of the header and a diff of this list line up.
 */
static const struct {
    const char* name;
    const char* value;
} k_all_strings[] = {
    ENTRY(UI_STR_QUIT),
    ENTRY(UI_STR_VERSION_LABEL),
    ENTRY(UI_STR_WORDS_12),
    ENTRY(UI_STR_WORDS_18),
    ENTRY(UI_STR_WORDS_24),
    ENTRY(UI_STR_NBGL_BIP39_PHRASE_TITLE),
    ENTRY(UI_STR_BAGL_BIP39_PHRASE_TITLE),
    ENTRY(UI_STR_BAGL_PROCESSING),
    ENTRY(UI_STR_NBGL_MENU_CHECK),
    ENTRY(UI_STR_NBGL_MENU_BACKUP),
    ENTRY(UI_STR_NBGL_MENU_RECOVER),
    ENTRY(UI_STR_NBGL_MENU_DERIVE),
    ENTRY(UI_STR_NBGL_MENU_TITLE),
    ENTRY(UI_STR_BAGL_IDLE_CHECK_L1),
    ENTRY(UI_STR_BAGL_IDLE_CHECK_L2),
    ENTRY(UI_STR_BAGL_IDLE_BACKUP_L1),
    ENTRY(UI_STR_BAGL_IDLE_BACKUP_L2),
    ENTRY(UI_STR_BAGL_IDLE_RECOVER_L1),
    ENTRY(UI_STR_BAGL_IDLE_RECOVER_L2),
    ENTRY(UI_STR_BAGL_BACKUP_EXPLAIN_L1),
    ENTRY(UI_STR_BAGL_BACKUP_EXPLAIN_L2),
    ENTRY(UI_STR_BAGL_BACKUP_EXPLAIN_L3),
    ENTRY(UI_STR_BAGL_RECOVER_EXPLAIN_L1),
    ENTRY(UI_STR_BAGL_RECOVER_EXPLAIN_L2),
    ENTRY(UI_STR_BAGL_RECOVER_EXPLAIN_L3),
    ENTRY(UI_STR_NBGL_HOME_DESCRIPTION),
    ENTRY(UI_STR_NBGL_HOME_ACTION),
    ENTRY(UI_STR_NBGL_HOME_COPYRIGHT),
    ENTRY(UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK),
    ENTRY(UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_TITLE_L1),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_TITLE_L2),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_TITLE_L3),
    ENTRY(UI_STR_BAGL_BIP39_LENGTH_BACK),
    ENTRY(UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS),
    ENTRY(UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS),
    ENTRY(UI_STR_BAGL_SSKR_START_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_START_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_START_TITLE_L3),
    ENTRY(UI_STR_NBGL_ENTER_BIP39_WORD),
    ENTRY(UI_STR_NBGL_ENTER_SSKR_WORD),
    ENTRY(UI_STR_BAGL_NANOS_WORD_HEADER),
    ENTRY(UI_STR_BAGL_NANOS_WORD_HEADER_LOWER),
    ENTRY(UI_STR_BAGL_NANOS_ENTER_SSKR_WORD),
    ENTRY(UI_STR_BAGL_NANOS_RESTART_FROM),
    ENTRY(UI_STR_BAGL_NANOS_WORD_INDEX_PREFIX),
    ENTRY(UI_STR_BAGL_NANOX_INTRO_L1),
    ENTRY(UI_STR_BAGL_NANOX_INTRO_L2),
    ENTRY(UI_STR_BAGL_NANOX_INTRO_L3),
    ENTRY(UI_STR_BAGL_NANOX_CLEAR_WORD),
    ENTRY(UI_STR_BAGL_NANOX_ENTER_BIP39_WORD),
    ENTRY(UI_STR_BAGL_NANOX_ENTER_SSKR_WORD),
    ENTRY(UI_STR_BAGL_NANOX_OF_WORD),
    ENTRY(UI_STR_BAGL_NANOX_SELECT_WORD),
    ENTRY(UI_STR_BAGL_ENTER_LABEL),
    ENTRY(UI_STR_BAGL_RETURN_TO_MENU),
    ENTRY(UI_STR_NBGL_RESULT_PHRASE_INVALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_PHRASE_NOMATCH_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_PHRASE_VALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_SHARES_INVALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_SHARES_NOMATCH_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_SHARES_VALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_INVALID),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_INVALID),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_NOMATCH),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_MATCH),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_NOMATCH),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_MATCH),
    ENTRY(UI_STR_NBGL_RESULT_TAP_TO_DISMISS),
    ENTRY(UI_STR_NBGL_RESULT_BACKUP_NOMATCH),
    ENTRY(UI_STR_NBGL_RESULT_BACKUP_MATCH),
    ENTRY(UI_STR_NBGL_RESULT_TAP_TO_CONTINUE),
    ENTRY(UI_STR_NBGL_RESULT_INVALID_ADVICE),
    ENTRY(UI_STR_BAGL_INVALID_ADVICE_L1),
    ENTRY(UI_STR_BAGL_INVALID_ADVICE_L2),
    ENTRY(UI_STR_BAGL_BIP39_INVALID_TITLE_L2),
    ENTRY(UI_STR_BAGL_BIP39_REENTER_PHRASE),
    ENTRY(UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_BIP39_MATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_BACKUP_NOMATCH_L1),
    ENTRY(UI_STR_BAGL_BACKUP_NOMATCH_L2),
    ENTRY(UI_STR_BAGL_SSKR_SHARES_TITLE),
    ENTRY(UI_STR_BAGL_SSKR_INVALID_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_REENTER_SHARES),
    ENTRY(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_MATCH_TITLE_L2),
    ENTRY(UI_STR_NBGL_RECOVER_WARN_TITLE),
    ENTRY(UI_STR_NBGL_RECOVER_WARN_DESC),
    ENTRY(UI_STR_NBGL_CANCEL),
    ENTRY(UI_STR_NBGL_EXPLANATION_CONTINUE),
    ENTRY(UI_STR_NBGL_SSKR_NUMBERS_TITLE),
    ENTRY(UI_STR_NBGL_SSKR_NUMBERS_ROW_CREATE),
    ENTRY(UI_STR_NBGL_SSKR_NUMBERS_ROW_WORDS),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_TITLE),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_REBUILD),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_LOST),
    ENTRY(UI_STR_NBGL_PHRASE_NOT_READABLE),
    ENTRY(UI_STR_NBGL_BIP85_INDEX_CONCEPT_TITLE),
    ENTRY(UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_TELLS),
    ENTRY(UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_COUNT),
    ENTRY(UI_STR_NBGL_BIP85_TITLE),
    ENTRY(UI_STR_NBGL_BIP85_ROW_DERIVED),
    ENTRY(UI_STR_NBGL_BIP85_ROW_PATH),
    ENTRY(UI_STR_NBGL_BIP85_ROW_LOST),
    ENTRY(UI_STR_NBGL_RECOVER_TITLE),
    ENTRY(UI_STR_NBGL_RECOVER_ROW_SUBSET),
    ENTRY(UI_STR_NBGL_RECOVER_ROW_ORDER),
    ENTRY(UI_STR_NBGL_BACKUP_SSKR_TITLE),
    ENTRY(UI_STR_NBGL_BACKUP_ROW_SPLIT),
    ENTRY(UI_STR_NBGL_BACKUP_ROW_APART),
    ENTRY(UI_STR_NBGL_BACKUP_PHRASE_TITLE),
    ENTRY(UI_STR_NBGL_BACKUP_ROW_CHECKED),
    ENTRY(UI_STR_NBGL_BACKUP_EXPLAIN_CONFIRM),
    ENTRY(UI_STR_NBGL_CLOSE),
    ENTRY(UI_STR_NBGL_CONTINUE_ANYWAY),
    ENTRY(UI_STR_NBGL_BACK_TO_SAFETY),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_FINISH),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_ITEM_FORMAT),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_ITEM_SHARES),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_ITEM_THRESHOLD),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_ITEM_WORDS),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_FORMAT_VALUE),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_COUNT_VALUE),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE),
    ENTRY(UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE_SINGLE),
    ENTRY(UI_STR_NBGL_SSKR_REVEAL_WARN),
    ENTRY(UI_STR_NBGL_SSKR_CLOSE_CONFIRM_TITLE),
    ENTRY(UI_STR_NBGL_SSKR_CLOSE_CONFIRM_DESC),
    ENTRY(UI_STR_NBGL_SSKR_CLOSE_CONFIRM_YES),
    ENTRY(UI_STR_NBGL_SSKR_CLOSE_CONFIRM_NO),
    ENTRY(UI_STR_BAGL_GENERATE_SSKR_L1),
    ENTRY(UI_STR_BAGL_GENERATE_SSKR_L2),
    ENTRY(UI_STR_BAGL_RECOVER_BIP39_L1),
    ENTRY(UI_STR_BAGL_RECOVER_BIP39_L2),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_SHARES),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_WORDS),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L1),
    ENTRY(UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L2),
    ENTRY(UI_STR_BAGL_SSKR_REVEAL_WARN_L1),
    ENTRY(UI_STR_BAGL_SSKR_REVEAL_WARN_L2),
    ENTRY(UI_STR_BAGL_SCREEN_PRIVACY_L1),
    ENTRY(UI_STR_BAGL_SCREEN_PRIVACY_L2),
    ENTRY(UI_STR_BAGL_SSKR_REVEAL_WARN_L5),
    ENTRY(UI_STR_BAGL_SSKR_REVEAL_WARN_L6),
    ENTRY(UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L1),
    ENTRY(UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L2),
    ENTRY(UI_STR_BAGL_RECOVER_WARN_L1),
    ENTRY(UI_STR_BAGL_RECOVER_WARN_L2),
    ENTRY(UI_STR_BAGL_RECOVER_WARN_L5),
    ENTRY(UI_STR_BAGL_RECOVER_WARN_L6),
    ENTRY(UI_STR_NBGL_SSKR_SHARE_HEADER),
    ENTRY(UI_STR_BAGL_SSKR_SHARE_HEADER),
    ENTRY(UI_STR_NBGL_SSKR_NUMSHARES_TITLE),
    ENTRY(UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR),
    ENTRY(UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_TITLE),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_ZERO_ERROR),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_RANGE_ERROR),
    ENTRY(UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR),
    ENTRY(UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1),
    ENTRY(UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2),
    ENTRY(UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1),
    ENTRY(UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2),
    ENTRY(UI_STR_NBGL_BIP85_APP_BIP39),
    ENTRY(UI_STR_NBGL_BIP85_APP_PWD_BASE64),
    ENTRY(UI_STR_NBGL_BIP85_APP_PWD_BASE85),
    ENTRY(UI_STR_NBGL_BIP85_SELECT_APP_TITLE),
    ENTRY(UI_STR_NBGL_BIP85_BIP39_HEADER),
    ENTRY(UI_STR_NBGL_BIP85_BASE64_HEADER),
    ENTRY(UI_STR_NBGL_BIP85_BASE85_HEADER),
    ENTRY(UI_STR_NBGL_BIP85_RESULT_LABEL),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_FINISH),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_ITEM_APP),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_ITEM_LENGTH),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_ITEM_INDEX),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_ITEM_PATH),
    ENTRY(UI_STR_NBGL_BIP85_LANGUAGE_ENGLISH),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_APP_WITH_LANGUAGE),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_INDEX_VALUE),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_LENGTH_WORDS),
    ENTRY(UI_STR_NBGL_BIP85_REVIEW_LENGTH_CHARACTERS),
    ENTRY(UI_STR_NBGL_BIP85_REVEAL_WARN_BIP39),
    ENTRY(UI_STR_NBGL_BIP85_REVEAL_WARN_PWD),
    ENTRY(UI_STR_NBGL_BIP85_INDEX_TITLE),
    ENTRY(UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR),
    ENTRY(UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE),
    ENTRY(UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR),
};

/*
 * Nano character metrics, reproduced from nanos_characters_width[] in the
 * SDK's lib_ux/src/ux_layout_paging_compute.c: BAGL_FONT_OPEN_SANS_REGULAR_11px
 * in the high nibble, BAGL_FONT_OPEN_SANS_EXTRABOLD_11px in the low, covering
 * printable ASCII 0x20-0x7F -- the only two fonts this application's fixed
 * (non-wrapping) UX_STEP layouts use. Measuring a regular-font string with
 * the bold table would over-estimate its width and could fail it against a
 * budget it actually clears; each string below is measured in the font its
 * own layout actually draws it in.
 */
static const unsigned char k_nanos_char_width_regular[96] = {
    3,  3, 4, 7, 6, 9, 8, 2,  3, 3, 6, 6, 3, 4,  3, 4, /* 0x20-0x2F */
    6,  6, 6, 6, 8, 6, 6, 6,  6, 6, 3, 3, 6, 6,  6, 5, /* 0x30-0x3F */
    10, 7, 7, 7, 8, 6, 6, 8,  8, 3, 4, 7, 6, 10, 8, 9, /* 0x40-0x4F */
    7,  9, 7, 6, 7, 8, 7, 10, 6, 6, 6, 4, 4, 4,  6, 5, /* 0x50-0x5F */
    6,  6, 7, 5, 7, 6, 5, 6,  7, 3, 4, 6, 3, 10, 7, 7, /* 0x60-0x6F */
    7,  7, 4, 5, 4, 7, 6, 9,  6, 6, 5, 4, 6, 4,  6, 7, /* 0x70-0x7F */
};
static const unsigned char k_nanos_char_width_bold[96] = {
    3,  3, 6, 7, 6, 10, 9, 3,  4, 4, 6, 6, 3, 4,  3, 5, /* 0x20-0x2F */
    8,  6, 7, 7, 8, 6,  8, 7,  8, 8, 3, 3, 5, 6,  5, 6, /* 0x30-0x3F */
    10, 8, 7, 7, 8, 6,  6, 8,  8, 4, 5, 8, 6, 11, 9, 9, /* 0x40-0x4F */
    7,  9, 8, 6, 6, 8,  6, 11, 8, 7, 7, 5, 5, 5,  7, 6, /* 0x50-0x5F */
    7,  7, 7, 6, 7, 7,  6, 7,  7, 4, 5, 7, 4, 10, 7, 7, /* 0x60-0x6F */
    7,  7, 5, 6, 5, 7,  7, 10, 7, 7, 6, 5, 6, 5,  6, 6, /* 0x70-0x7F */
};

#define NANOS_FIRST_CHAR 0x20
#define NANOS_LAST_CHAR 0x7F

/*
 * Every string below appears on at least one BAGL target through a fixed
 * (non-wrapping, non-cropping) UX_STEP layout -- never through BNNN_PAGING,
 * which the dynamic word/share buffers use and which auto-wraps. Two real,
 * measured pixel budgets are in play, both read from the SDK
 * (lib_ux/src/ux_layout_{bb,pb,pbb,pnn,nnn}.c on the 128x32 and 128x64
 * variants):
 *
 *   - 116px: the NN / NNN / BN label box (x=6, width=116 on every BAGL
 *     screen size the SDK ships). PB (128px, centered) is bounded here too,
 *     tighter than its real budget, since nothing here needs the extra room;
 *   - 87px: the PBB / PNN icon-flanked label box on the 128x32 (nanos)
 *     variant -- x=41 to the 128px screen edge. This is the tighter of the
 *     two, and the one that would actually clip: nanos's PBB/PNN box is
 *     narrower than nanox/nanos+'s (x=6, width=116, same as NN/NNN there).
 *
 * NANOX_ENTER_BIP39_WORD and NANOX_ENTER_SSKR_WORD render through a third
 * box, not through NN/PBB: the keyboard title label in the nanox/nanos+
 * branch of screen_common_keyboard_elements (src/bagl/ux_keyboard.c,
 * userid 0x04) -- x=0, width=128, regular, centered. Bounded at that real
 * 128px, not the 116px margin used elsewhere, because
 * "Enter Share#99 word#99" needs the room the real box actually has.
 *
 * NANOS_WORD_INDEX_PREFIX, a keyboard render fragment in
 * nanos_enter_phrase.c, was not independently re-derived; it is bounded at
 * the tighter 87px in the bold font -- the more conservative of the two
 * combinations -- rather than guessed at a wider or lighter one.
 *
 * Nothing below is exempt any more. Two strings used to be, and both were
 * recorded here rather than only in a PR body, which is what made them visible
 * enough to close when their screens were next touched: "recovery phrase" at
 * 93px, which the two idle entries ended on, and "BIP39 Recovery" at 90px,
 * which titled the invalid verdict. The first was rewritten when the idle menu
 * was; the second stopped existing when the three BIP-39 verdict screens were
 * given one title between them. Every macro reaching a fixed layout is now
 * asserted.
 */
#define BOUND_WIDE 116
#define BOUND_TIGHT 87
#define BOUND_KEYBOARD_TITLE 128

static const struct {
    const char* name;
    const char* value;
    unsigned int budget_px;
    bool bold;
} k_nano_bounded_strings[] = {
    BOUNDED(UI_STR_QUIT, BOUND_WIDE, true),
    BOUNDED(UI_STR_VERSION_LABEL, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_BIP39_PHRASE_TITLE, BOUND_TIGHT, true),
    BOUNDED(UI_STR_WORDS_12, BOUND_WIDE, true),
    BOUNDED(UI_STR_WORDS_18, BOUND_WIDE, true),
    BOUNDED(UI_STR_WORDS_24, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_PROCESSING, BOUND_WIDE, true),
    /*
     * All six idle-menu fragments, both lines of each. The two entries these
     * replaced ended on "recovery phrase", 93px against an 87px box, and were
     * recorded above as unassertable rather than asserted; the three that
     * took their place were written to the budget and are held to it.
     */
    BOUNDED(UI_STR_BAGL_IDLE_CHECK_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_IDLE_CHECK_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_IDLE_BACKUP_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_IDLE_BACKUP_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_IDLE_RECOVER_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_IDLE_RECOVER_L2, BOUND_TIGHT, true),
    /* nn steps, so the wide box and the regular font. */
    BOUNDED(UI_STR_BAGL_BACKUP_EXPLAIN_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BACKUP_EXPLAIN_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BACKUP_EXPLAIN_L3, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_EXPLAIN_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_EXPLAIN_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_EXPLAIN_L3, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BACKUP_NOMATCH_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BACKUP_NOMATCH_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_TITLE_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_TITLE_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_TITLE_L3, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_LENGTH_BACK, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_START_TITLE_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_START_TITLE_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_START_TITLE_L3, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOS_WORD_HEADER, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOS_WORD_HEADER_LOWER, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_NANOS_ENTER_SSKR_WORD, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOS_RESTART_FROM, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_NANOS_WORD_INDEX_PREFIX, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_NANOX_INTRO_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOX_INTRO_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOX_INTRO_L3, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOX_CLEAR_WORD, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_NANOX_ENTER_BIP39_WORD, BOUND_KEYBOARD_TITLE, false),
    BOUNDED(UI_STR_BAGL_NANOX_ENTER_SSKR_WORD, BOUND_KEYBOARD_TITLE, false),
    BOUNDED(UI_STR_BAGL_NANOX_OF_WORD, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_NANOX_SELECT_WORD, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_ENTER_LABEL, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RETURN_TO_MENU, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_INVALID_ADVICE_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_INVALID_ADVICE_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_BIP39_INVALID_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_BIP39_REENTER_PHRASE, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_BIP39_MATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_SHARES_TITLE, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_INVALID_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REENTER_SHARES, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_MATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_GENERATE_SSKR_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_GENERATE_SSKR_L2, BOUND_TIGHT, true),
    /*
     * The review and the two warnings added in front of the two places a Nano
     * reveals something. The pbb titles take the tight icon-flanked box, the
     * nn bodies the wide one, as everything above does.
     *
     * UI_STR_BAGL_SSKR_REVIEW_WORDS and _SHARES are not here: both compose a
     * value that reaches three digits, which is one more than
     * expand_worst_case() substitutes, so they are measured against their real
     * bounds by test_sskr_review_lines_fit_the_nano_box() below.
     */
    BOUNDED(UI_STR_BAGL_SSKR_REVIEW_TITLE_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REVIEW_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REVIEW_CONFIRM_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REVEAL_WARN_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REVEAL_WARN_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SCREEN_PRIVACY_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SCREEN_PRIVACY_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_REVEAL_WARN_L5, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_REVEAL_WARN_L6, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_WARN_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_RECOVER_WARN_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_RECOVER_WARN_L5, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_WARN_L6, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_RECOVER_BIP39_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_RECOVER_BIP39_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2, BOUND_WIDE, false),
    BOUNDED(UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1, BOUND_TIGHT, false),
    BOUNDED(UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2, BOUND_TIGHT, false),
    BOUNDED(UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2, BOUND_TIGHT, true),
};

/*
 * The BOUNDED table above holds unformatted templates: several contain "%d",
 * substituted at runtime with an onboarding step, share, or word index. Every
 * one of those is bounded well under 100 on every BAGL target (word counts up
 * to 46, share indices up to 16), so replacing each "%d" with "99" measures
 * the worst case those templates can actually reach.
 */
static void expand_worst_case(const char* template, char* out,
                              size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; template[i] != '\0' && o + 1 < out_size; i++) {
        if (template[i] == '%' && template[i + 1] == 'd') {
            if (o + 2 < out_size) {
                out[o++] = '9';
                out[o++] = '9';
            }
            i++;
        } else {
            out[o++] = template[i];
        }
    }
    out[o] = '\0';
}

static unsigned int text_width_px(const char* text, bool bold) {
    const unsigned char* table =
        bold ? k_nanos_char_width_bold : k_nanos_char_width_regular;
    unsigned int width = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        assert_true(c >= NANOS_FIRST_CHAR && c <= NANOS_LAST_CHAR);
        width += table[c - NANOS_FIRST_CHAR];
    }
    return width;
}

static void test_every_string_is_non_empty(void** state) {
    (void)state;

    for (size_t i = 0; i < sizeof(k_all_strings) / sizeof(k_all_strings[0]);
         i++) {
        if (strlen(k_all_strings[i].value) == 0) {
            fail_msg("%s is empty", k_all_strings[i].name);
        }
    }
}

static void test_nano_fixed_layout_strings_fit_their_budget(void** state) {
    (void)state;

    for (size_t i = 0;
         i < sizeof(k_nano_bounded_strings) / sizeof(k_nano_bounded_strings[0]);
         i++) {
        char expanded[128];
        expand_worst_case(k_nano_bounded_strings[i].value, expanded,
                          sizeof(expanded));

        unsigned int width =
            text_width_px(expanded, k_nano_bounded_strings[i].bold);
        if (width > k_nano_bounded_strings[i].budget_px) {
            fail_msg("%s (\"%s\") is %upx wide, over its %upx budget",
                     k_nano_bounded_strings[i].name, expanded, width,
                     k_nano_bounded_strings[i].budget_px);
        }
    }
}

/*
 * The SSKR share label fits the Nano title line once the paging widget has
 * added its own page counter.
 *
 * This one is not in the table above, and cannot be: that table measures a
 * macro on its own, and what reaches the screen here is the macro *plus* a
 * "(page/total)" counter that bnnn_paging appends at display time and that
 * the string itself knows nothing about. Measuring the macro alone would
 * report 68px against a 114px line and call it comfortable, while the drawn
 * title is 108px.
 *
 * It is worth measuring rather than trusting, because a bnnn_paging title
 * does not wrap -- it clips, silently. The longer label first tried for this
 * screen did exactly that, and only a screenshot revealed it
 * ("SSKR Share... of 3 (4/5)" under Speculos).
 *
 * The worst case is built from the real bounds, not from placeholder digits:
 *
 *   - shares: SSS_MAX_SHARE_COUNT, the count the generation menu is capped
 *     to (16, or 10 under TARGET_NANOS);
 *   - pages: a share is at most SSKR_SHARE_MAX_WIRE_LENGTH (46) ByteWords;
 *     the widest four-letter word in this font leaves room for two per
 *     114px line, and the 128x32 Nano S shows one such line per page
 *     (UX_LAYOUT_PAGING_LINE_COUNT), so 23 pages bounds every device.
 *
 * Using "99" for both, as expand_worst_case() would, gives 116px and fails a
 * layout that is actually fine: 99 shares cannot be generated.
 */
#define NANO_PIXEL_PER_LINE 114
#define SSKR_MAX_PAGES_PER_SHARE 23

static void test_sskr_share_label_fits_the_nano_title_line(void** state) {
    (void)state;

    char title[64];
    snprintf(title, sizeof(title), UI_STR_BAGL_SSKR_SHARE_HEADER " (%d/%d)",
             SSS_MAX_SHARE_COUNT, SSS_MAX_SHARE_COUNT, SSKR_MAX_PAGES_PER_SHARE,
             SSKR_MAX_PAGES_PER_SHARE);

    /* a bnnn_paging title is drawn in the bold font */
    unsigned int width = text_width_px(title, true);
    if (width > NANO_PIXEL_PER_LINE) {
        fail_msg(
            "the widest SSKR share title (\"%s\") is %upx, over the %upx a "
            "Nano line holds -- bnnn_paging would clip it with no warning",
            title, width, NANO_PIXEL_PER_LINE);
    }
}

/*
 * The same measurement for the other label a bnnn_paging title carries, and
 * the reason this test exists at all: this one was clipped in production.
 *
 * src/bagl/ux_bip39.c heads the recovered phrase with
 * UI_STR_BAGL_BIP39_PHRASE_TITLE, and the widget appends the counter here too.
 * The macro was in k_nano_bounded_strings[] all along and passed -- "BIP39
 * Phrase" is 75px against the 87px PBB box -- because measuring a macro on its
 * own cannot see a counter the macro does not contain. What reached the screen
 * was "BIP39 Phrase (24/24)" at 121px on a 114px line, drawn as "BIP39 Phrase
 * (12/1".
 *
 * The worst case is the widest counter, and the counter's two halves are both
 * page numbers of one phrase, so both are bounded by the same thing: a page
 * break falls between words, never inside one, so a phrase of
 * BIP39_MNEMONIC_SIZE_24 words cannot paginate past 24 pages -- reached on the
 * 128x32 Nano S, where UX_LAYOUT_PAGING_LINE_COUNT is 1 and a line of two
 * eight-letter words does not fit.
 */
static void test_phrase_title_fits_the_nano_title_line(void** state) {
    (void)state;

    char title[64];
    snprintf(title, sizeof(title), UI_STR_BAGL_BIP39_PHRASE_TITLE " (%d/%d)",
             BIP39_MNEMONIC_SIZE_24, BIP39_MNEMONIC_SIZE_24);

    unsigned int width = text_width_px(title, true);
    if (width > NANO_PIXEL_PER_LINE) {
        fail_msg(
            "the widest phrase title (\"%s\") is %upx, over the %upx a "
            "Nano line holds -- bnnn_paging would clip it with no warning",
            title, width, NANO_PIXEL_PER_LINE);
    }
}

/*
 * The three composed lines of the Nano backup review and its close question,
 * measured against the numbers they can actually carry.
 *
 * They are not in k_nano_bounded_strings[] because expand_worst_case()
 * substitutes "99" for every "%d", and the word total is the one string in this
 * header that goes past two digits: SSS_MAX_SHARE_COUNT shares of
 * SSKR_LONGEST_SHARE_WORDCOUNT ByteWords is 460 under TARGET_NANOS and 736
 * elsewhere. "99" would measure a narrower line than the one that gets drawn,
 * which is the failure mode this file exists to prevent.
 *
 * All three are nn lines: the wide 116px box, the regular font, and no
 * wrapping -- they clip.
 */
/* the same 46 src/nbgl/ui.c static-asserts against SSS_MAX_SHARE_COUNT */
#define SSKR_LONGEST_SHARE_WORDCOUNT 46

static void test_sskr_review_lines_fit_the_nano_box(void** state) {
    (void)state;

    char line[64];

    snprintf(line, sizeof(line), UI_STR_BAGL_SSKR_REVIEW_WORDS,
             SSS_MAX_SHARE_COUNT * SSKR_LONGEST_SHARE_WORDCOUNT);
    unsigned int width = text_width_px(line, false);
    if (width > BOUND_WIDE) {
        fail_msg(
            "the widest backup review word total (\"%s\") is %upx, over "
            "the %upx a nn line holds -- it would be clipped with no warning",
            line, width, BOUND_WIDE);
    }

    /*
     * Both counts at their maximum: the threshold cannot exceed the share
     * count, so SSS_MAX_SHARE_COUNT twice is the widest this line reaches, and
     * it is the value the generation menu is actually capped to rather than a
     * placeholder.
     */
    snprintf(line, sizeof(line), UI_STR_BAGL_SSKR_REVIEW_SHARES,
             SSS_MAX_SHARE_COUNT, SSS_MAX_SHARE_COUNT);
    width = text_width_px(line, false);
    if (width > BOUND_WIDE) {
        fail_msg(
            "the widest backup review scheme line (\"%s\") is %upx, over "
            "the %upx a nn line holds -- it would be clipped with no warning",
            line, width, BOUND_WIDE);
    }

    snprintf(line, sizeof(line), UI_STR_BAGL_SSKR_CLOSE_CONFIRM_L2,
             SSS_MAX_SHARE_COUNT);
    width = text_width_px(line, false);
    if (width > BOUND_WIDE) {
        fail_msg(
            "the widest close question (\"%s\") is %upx, over the %upx a "
            "nn line holds",
            line, width, BOUND_WIDE);
    }
}

/*
 * Four of the entry screens name, in their own title, the range of values
 * they accept. A title that announces a bound the code does not apply is
 * worse than a title with no bound at all: it is read as a promise.
 *
 * Two of the four carry the bound as a literal, because nothing about it
 * varies -- and a literal is exactly what drifts. Both are checked here
 * against the constant that actually decides the bound:
 *
 *   - the SSKR share count against SSS_MAX_SHARE_COUNT, which is what
 *     sskr_sharenum_validate() compares against and what the Shamir layer
 *     refuses to exceed;
 *   - the BIP85 index against the keypad's own digit cap,
 *     BIP85_INDEX_MAX_NUMBER_LENGTH. Raising that cap to eight digits without
 *     touching the strings would leave two screens announcing a ceiling a
 *     million short of the one the keypad accepts.
 *
 * The number is looked for as digits, with any thousands separators dropped
 * first, so that "9,999,999" and "9999999" are the same claim.
 *
 * It is matched as a whole run of digits, not as a substring. A substring
 * search passes on exactly the half of the problem that matters: lower the
 * keypad cap to six digits and "999999" is still found inside the title's
 * "9999999", so a title promising ten times what the keypad accepts would go
 * through. Comparing whole runs fails in both directions.
 */
static void strip_commas(const char* in, char* out, size_t out_size) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < out_size; i++) {
        if (in[i] != ',') {
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

static bool has_digit_run(const char* text, const char* bound) {
    size_t bound_len = strlen(bound);
    for (size_t i = 0; text[i] != '\0';) {
        if (text[i] < '0' || text[i] > '9') {
            i++;
            continue;
        }
        size_t start = i;
        while (text[i] >= '0' && text[i] <= '9') {
            i++;
        }
        size_t run_len = i - start;
        if (run_len == bound_len &&
            strncmp(text + start, bound, bound_len) == 0) {
            return true;
        }
    }
    return false;
}

static void assert_announces(const char* name, const char* value,
                             const char* bound) {
    char plain[256];
    strip_commas(value, plain, sizeof(plain));
    if (!has_digit_run(plain, bound)) {
        fail_msg("%s (\"%s\") does not name %s, the bound the code applies",
                 name, value, bound);
    }
}

static void test_announced_bounds_are_the_applied_ones(void** state) {
    (void)state;

    char bound[32];

    snprintf(bound, sizeof(bound), "%d", SSS_MAX_SHARE_COUNT);
    assert_announces("UI_STR_NBGL_SSKR_NUMSHARES_TITLE",
                     UI_STR_NBGL_SSKR_NUMSHARES_TITLE, bound);
    assert_announces("UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR",
                     UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR, bound);

    /* the largest value BIP85_INDEX_MAX_NUMBER_LENGTH digits can spell */
    unsigned long index_max = 1;
    for (int i = 0; i < BIP85_INDEX_MAX_NUMBER_LENGTH; i++) {
        index_max *= 10;
    }
    snprintf(bound, sizeof(bound), "%lu", index_max - 1);
    assert_announces("UI_STR_NBGL_BIP85_INDEX_TITLE",
                     UI_STR_NBGL_BIP85_INDEX_TITLE, bound);
    assert_announces("UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR",
                     UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR, bound);
}

/*
 * The other two carry their bounds as "%d", filled in at display time -- the
 * threshold's maximum is the share count just entered, the password length's
 * pair depends on the chosen application. Nothing here can check the values,
 * which only exist at runtime; what it can check is that each format still
 * takes exactly the arguments its call site passes.
 *
 * Both failure modes are real and neither is loud. A title reworded without
 * its "%d" drops the bound the screen was changed to show, and nothing warns.
 * A title that gains one reads an argument that was never pushed.
 */
static size_t count_conversions(const char* format) {
    size_t n = 0;
    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] == 'd') {
            n++;
            i++;
        }
    }
    return n;
}

static void test_composed_titles_take_the_arguments_passed_to_them(
    void** state) {
    (void)state;

    static const struct {
        const char* name;
        const char* value;
        size_t expected;
    } k_composed[] = {
        /* display_sskr_select_threshold_page(): the floor, then the share count
         */
        {"UI_STR_NBGL_SSKR_THRESHOLD_TITLE", UI_STR_NBGL_SSKR_THRESHOLD_TITLE,
         2},
        /* display_bip85_select_password_length_page(): minimum, maximum */
        {"UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE",
         UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE, 2},
        /* bip85_password_length_validate(): the same pair */
        {"UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR",
         UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR, 2},
    };

    for (size_t i = 0; i < sizeof(k_composed) / sizeof(k_composed[0]); i++) {
        size_t found = count_conversions(k_composed[i].value);
        if (found != k_composed[i].expected) {
            fail_msg(
                "%s (\"%s\") takes %zu \"%%d\", but its call site passes %zu",
                k_composed[i].name, k_composed[i].value, found,
                k_composed[i].expected);
        }
    }
}

/*
 * The table at the top of this file names every macro of ui_strings.h -- and
 * until now, nothing said so.
 *
 * A *renamed* or *removed* macro has always been caught, and loudly: ENTRY()
 * expands to the macro itself, so the compiler refuses the file. An *added*
 * one was the silent half. Nothing linked the header to the table, so a macro
 * declared in the header and never listed here was checked by none of the
 * four tests above -- not for being empty, not for fitting a Nano line -- and
 * the suite stayed green. Six macros were added by the change that added this
 * test, which is why it is being added now rather than being noted as a gap.
 *
 * Read from the header itself, at the path CMake hands over, rather than
 * compared against a number kept up to date by hand: a hand-kept count is the
 * same class of thing as a hand-kept list, and would drift the same way.
 *
 * Names, not a count. Comparing cardinalities passes on a header that gains
 * two macros while the table lists one of them twice -- the totals agree and
 * one macro is still checked by nothing. Each declared name is looked for in
 * the table by name, and the table is checked for repeats, so both halves
 * fail loudly. One "#define" line per macro, including the multi-line ones: a
 * value continued with a backslash still has exactly one.
 */
#ifndef UI_STRINGS_HEADER_PATH
#error "UI_STRINGS_HEADER_PATH must name src/common/ui_strings.h"
#endif

#define MACRO_PREFIX "#define UI_STR_"

static bool table_names(const char* name) {
    for (size_t i = 0; i < sizeof(k_all_strings) / sizeof(k_all_strings[0]);
         i++) {
        if (strcmp(k_all_strings[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

static void test_the_table_names_every_macro_of_the_header(void** state) {
    (void)state;

    /* no entry names the same macro twice */
    for (size_t i = 0; i < sizeof(k_all_strings) / sizeof(k_all_strings[0]);
         i++) {
        for (size_t j = i + 1;
             j < sizeof(k_all_strings) / sizeof(k_all_strings[0]); j++) {
            if (strcmp(k_all_strings[i].name, k_all_strings[j].name) == 0) {
                fail_msg("k_all_strings[] lists %s twice",
                         k_all_strings[i].name);
            }
        }
    }

    FILE* header = fopen(UI_STRINGS_HEADER_PATH, "r");
    if (header == NULL) {
        fail_msg("cannot open %s", UI_STRINGS_HEADER_PATH);
    }

    size_t declared = 0;
    char line[512];
    while (fgets(line, sizeof(line), header) != NULL) {
        if (strncmp(line, MACRO_PREFIX, strlen(MACRO_PREFIX)) != 0) {
            continue;
        }
        declared++;

        /* the macro name is the token after "#define " */
        const char* start = line + strlen("#define ");
        size_t len = 0;
        while (start[len] != '\0' && !isspace((unsigned char)start[len]) &&
               start[len] != '(') {
            len++;
        }
        char name[128];
        if (len >= sizeof(name)) {
            fail_msg("%s declares a macro name longer than this test can hold",
                     MACRO_PREFIX);
        }
        memcpy(name, start, len);
        name[len] = '\0';

        if (!table_names(name)) {
            fclose(header);
            fail_msg(
                "%s declares %s, k_all_strings[] does not list it; a macro "
                "missing from that table is checked by nothing in this file",
                UI_STRINGS_HEADER_PATH, name);
        }
    }
    fclose(header);

    /* every name in the table is a macro of the header, so equal totals now
     * mean equal sets -- the reverse direction is a compile error anyway,
     * since ENTRY() expands to the macro itself */
    const size_t listed = sizeof(k_all_strings) / sizeof(k_all_strings[0]);
    if (declared != listed) {
        fail_msg("%s declares %zu UI_STR_ macros, k_all_strings[] lists %zu",
                 UI_STRINGS_HEADER_PATH, declared, listed);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_every_string_is_non_empty),
        cmocka_unit_test(test_the_table_names_every_macro_of_the_header),
        cmocka_unit_test(test_nano_fixed_layout_strings_fit_their_budget),
        cmocka_unit_test(test_sskr_share_label_fits_the_nano_title_line),
        cmocka_unit_test(test_phrase_title_fits_the_nano_title_line),
        cmocka_unit_test(test_sskr_review_lines_fit_the_nano_box),
        cmocka_unit_test(test_announced_bounds_are_the_applied_ones),
        cmocka_unit_test(
            test_composed_titles_take_the_arguments_passed_to_them),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
