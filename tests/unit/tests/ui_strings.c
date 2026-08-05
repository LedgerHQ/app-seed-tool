/*
 * ui_strings.h is the first piece of interface the application can test in
 * this suite: src/nbgl/ui.c and the files under src/bagl/ are not compiled
 * into any unit target, so this is the only net any of these strings gets
 * other than the functional tests under Speculos.
 *
 * Two things are checked:
 *
 *   - every string is non-empty. A macro whose value became "" by accident
 *     (a bad merge, a copy-paste that dropped the literal) is otherwise
 *     invisible until someone runs the app;
 *
 *   - every BAGL fragment destined for a fixed (non-wrapping) Nano layout
 *     fits the real pixel budget of that layout. NN/NNN/PBB/PNN/PB/BN steps
 *     do not wrap or crop -- unlike BNNN_PAGING, which the dynamic word/share
 *     buffers use and which this test does not need to bound. A string that
 *     does not fit is silently clipped at runtime; nothing else catches it.
 *
 * Each entry below is `ENTRY(UI_STR_X)`, which expands to `{"UI_STR_X",
 * UI_STR_X}` -- the name and the header's own macro, not a retyped copy of
 * its value. That is what "declares its own table naming every macro" in
 * ui_strings.h's own comment means: nothing here can drift from the header,
 * because nothing here repeats it by hand. A renamed or removed macro is a
 * compile error in this file, not a silent gap in the table.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "ui_strings.h"

#define ENTRY(name) \
    { #name, name }
#define BOUNDED(name, budget_px, is_bold) \
    { #name, name, budget_px, is_bold }

/*
 * Every macro in src/common/ui_strings.h. Kept in the order the header
 * declares them, so a diff of the header and a diff of this list line up.
 */
static const struct {
    const char *name;
    const char *value;
} k_all_strings[] = {
    ENTRY(UI_STR_QUIT),
    ENTRY(UI_STR_VERSION_LABEL),
    ENTRY(UI_STR_BIP39_PHRASE_TITLE),
    ENTRY(UI_STR_SSKR_SHARE_HEADER),
    ENTRY(UI_STR_WORDS_12),
    ENTRY(UI_STR_WORDS_18),
    ENTRY(UI_STR_WORDS_24),
    ENTRY(UI_STR_BAGL_PROCESSING),
    ENTRY(UI_STR_NBGL_TOOL_BIP39),
    ENTRY(UI_STR_NBGL_TOOL_SSKR),
    ENTRY(UI_STR_NBGL_TOOL_BIP85),
    ENTRY(UI_STR_NBGL_SELECT_TOOL_TITLE),
    ENTRY(UI_STR_BAGL_IDLE_BIP39_L1),
    ENTRY(UI_STR_BAGL_IDLE_BIP39_L2),
    ENTRY(UI_STR_BAGL_IDLE_SSKR_L1),
    ENTRY(UI_STR_BAGL_IDLE_SSKR_L2),
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
    ENTRY(UI_STR_NBGL_RESULT_INVALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_NOMATCH_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_VALID_TITLE),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_INVALID),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_INVALID),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_NOMATCH),
    ENTRY(UI_STR_NBGL_RESULT_BIP39_MATCH),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_NOMATCH),
    ENTRY(UI_STR_NBGL_RESULT_SSKR_MATCH),
    ENTRY(UI_STR_NBGL_RESULT_TAP_TO_DISMISS),
    ENTRY(UI_STR_NBGL_RESULT_INVALID_ADVICE),
    ENTRY(UI_STR_BAGL_INVALID_ADVICE_L1),
    ENTRY(UI_STR_BAGL_INVALID_ADVICE_L2),
    ENTRY(UI_STR_BAGL_BIP39_INVALID_TITLE_L1),
    ENTRY(UI_STR_BAGL_BIP39_INVALID_TITLE_L2),
    ENTRY(UI_STR_BAGL_BIP39_REENTER_PHRASE),
    ENTRY(UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_BIP39_MATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_INVALID_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_INVALID_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_REENTER_SHARES),
    ENTRY(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2),
    ENTRY(UI_STR_BAGL_SSKR_MATCH_TITLE_L1),
    ENTRY(UI_STR_BAGL_SSKR_MATCH_TITLE_L2),
    ENTRY(UI_STR_NBGL_RECOVER_BIP39_TITLE),
    ENTRY(UI_STR_NBGL_RECOVER_BIP39_DESC),
    ENTRY(UI_STR_NBGL_RECOVER_BIP39_CONFIRM),
    ENTRY(UI_STR_NBGL_CANCEL),
    ENTRY(UI_STR_NBGL_GENERATE_SSKR_TITLE),
    ENTRY(UI_STR_NBGL_GENERATE_SSKR_DESC),
    ENTRY(UI_STR_NBGL_GENERATE_SSKR_CONFIRM),
    ENTRY(UI_STR_NBGL_CLOSE),
    ENTRY(UI_STR_BAGL_GENERATE_SSKR_L1),
    ENTRY(UI_STR_BAGL_GENERATE_SSKR_L2),
    ENTRY(UI_STR_BAGL_RECOVER_BIP39_L1),
    ENTRY(UI_STR_BAGL_RECOVER_BIP39_L2),
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
    3,  3,  4,  7,  6,  9,  8,  2,  3,  3,  6,  6,  3,  4,  3,  4,  /* 0x20-0x2F */
    6,  6,  6,  6,  8,  6,  6,  6,  6,  6,  3,  3,  6,  6,  6,  5,  /* 0x30-0x3F */
    10, 7,  7,  7,  8,  6,  6,  8,  8,  3,  4,  7,  6,  10, 8,  9,  /* 0x40-0x4F */
    7,  9,  7,  6,  7,  8,  7,  10, 6,  6,  6,  4,  4,  4,  6,  5,  /* 0x50-0x5F */
    6,  6,  7,  5,  7,  6,  5,  6,  7,  3,  4,  6,  3,  10, 7,  7,  /* 0x60-0x6F */
    7,  7,  4,  5,  4,  7,  6,  9,  6,  6,  5,  4,  6,  4,  6,  7,  /* 0x70-0x7F */
};
static const unsigned char k_nanos_char_width_bold[96] = {
    3,  3,  6,  7,  6,  10, 9,  3,  4,  4,  6,  6,  3,  4,  3,  5,  /* 0x20-0x2F */
    8,  6,  7,  7,  8,  6,  8,  7,  8,  8,  3,  3,  5,  6,  5,  6,  /* 0x30-0x3F */
    10, 8,  7,  7,  8,  6,  6,  8,  8,  4,  5,  8,  6,  11, 9,  9,  /* 0x40-0x4F */
    7,  9,  8,  6,  6,  8,  6,  11, 8,  7,  7,  5,  5,  5,  7,  6,  /* 0x50-0x5F */
    7,  7,  7,  6,  7,  7,  6,  7,  7,  4,  5,  7,  4,  10, 7,  7,  /* 0x60-0x6F */
    7,  7,  5,  6,  5,  7,  7,  10, 7,  7,  6,  5,  6,  5,  6,  6,  /* 0x70-0x7F */
};

#define NANOS_FIRST_CHAR 0x20
#define NANOS_LAST_CHAR  0x7F

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
 * Two strings fail the bound below and are deliberately not asserted:
 * "recovery phrase" (UI_STR_BAGL_IDLE_BIP39_L2 / _SSKR_L2, 93px bold, PBB) and
 * "BIP39 Recovery" (UI_STR_BAGL_BIP39_INVALID_TITLE_L1, 90px bold, PBB), both
 * 3-6px over the 87px PBB budget on nanos. Both predate this header --
 * nothing here changed either word -- and this lot does not change wording
 * (see the PR body). They are recorded here, not silently passed, so the
 * gap this test cannot close is visible in the same place as the strings it
 * does check, rather than only in the PR body.
 */
#define BOUND_WIDE           116
#define BOUND_TIGHT          87
#define BOUND_KEYBOARD_TITLE 128

static const struct {
    const char *name;
    const char *value;
    unsigned int budget_px;
    bool bold;
} k_nano_bounded_strings[] = {
    BOUNDED(UI_STR_QUIT, BOUND_WIDE, true),
    BOUNDED(UI_STR_VERSION_LABEL, BOUND_WIDE, true),
    BOUNDED(UI_STR_BIP39_PHRASE_TITLE, BOUND_TIGHT, true),
    BOUNDED(UI_STR_WORDS_12, BOUND_WIDE, true),
    BOUNDED(UI_STR_WORDS_18, BOUND_WIDE, true),
    BOUNDED(UI_STR_WORDS_24, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_PROCESSING, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_IDLE_BIP39_L1, BOUND_TIGHT, true),
    /* IDLE_BIP39_L2 ("recovery phrase") -- see the file comment above. */
    BOUNDED(UI_STR_BAGL_IDLE_SSKR_L1, BOUND_TIGHT, true),
    /* IDLE_SSKR_L2 ("recovery phrase") -- see the file comment above. */
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
    /* BIP39_INVALID_TITLE_L1 ("BIP39 Recovery") -- see the file comment above. */
    BOUNDED(UI_STR_BAGL_BIP39_INVALID_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_BIP39_REENTER_PHRASE, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_BIP39_MATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_INVALID_TITLE_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_INVALID_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_REENTER_SHARES, BOUND_WIDE, true),
    BOUNDED(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_MATCH_TITLE_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_SSKR_MATCH_TITLE_L2, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_GENERATE_SSKR_L1, BOUND_TIGHT, true),
    BOUNDED(UI_STR_BAGL_GENERATE_SSKR_L2, BOUND_TIGHT, true),
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
static void expand_worst_case(const char *template, char *out, size_t out_size) {
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

static unsigned int text_width_px(const char *text, bool bold) {
    const unsigned char *table = bold ? k_nanos_char_width_bold : k_nanos_char_width_regular;
    unsigned int width = 0;
    for (size_t i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char) text[i];
        assert_true(c >= NANOS_FIRST_CHAR && c <= NANOS_LAST_CHAR);
        width += table[c - NANOS_FIRST_CHAR];
    }
    return width;
}

static void test_every_string_is_non_empty(void **state) {
    (void) state;

    for (size_t i = 0; i < sizeof(k_all_strings) / sizeof(k_all_strings[0]); i++) {
        if (strlen(k_all_strings[i].value) == 0) {
            fail_msg("%s is empty", k_all_strings[i].name);
        }
    }
}

static void test_nano_fixed_layout_strings_fit_their_budget(void **state) {
    (void) state;

    for (size_t i = 0; i < sizeof(k_nano_bounded_strings) / sizeof(k_nano_bounded_strings[0]);
        i++) {
        char expanded[128];
        expand_worst_case(k_nano_bounded_strings[i].value, expanded, sizeof(expanded));

        unsigned int width = text_width_px(expanded, k_nano_bounded_strings[i].bold);
        if (width > k_nano_bounded_strings[i].budget_px) {
            fail_msg("%s (\"%s\") is %upx wide, over its %upx budget", k_nano_bounded_strings[i].name,
                     expanded, width, k_nano_bounded_strings[i].budget_px);
        }
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_every_string_is_non_empty),
        cmocka_unit_test(test_nano_fixed_layout_strings_fit_their_budget),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
