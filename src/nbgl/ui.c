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

#include <os.h>
#include <string.h>

#include "constants.h"
#include "glyphs.h"

#if defined(HAVE_NBGL)

#include <nbgl_debug.h>
#include <nbgl_fonts.h>
#include <nbgl_front.h>
#include <nbgl_layout.h>
#include <nbgl_obj.h>
#include <nbgl_page.h>
#include <nbgl_use_case.h>

#include "../common/bip39/common_bip39.h"
#include "../common/bip85/common_bip85.h"
#include "../common/sskr/common_sskr.h"
#include "../common/ui_strings.h"
#include "../ui.h"
#include "./bip39_mnemonic.h"
#include "./bip85_app.h"
#include "./sskr_shares.h"

/*
 * What every screen in this file agrees on.
 *
 * Three signals are used across families, so none of them can be settled at a
 * single screen. Each is written once, here, with what it excludes -- a rule
 * that does not say what it rules out is a preference.
 *
 * **A black control means an act with a consequence.** Entering a Phrase,
 * generating Shares, revealing a derived secret: the black button of an
 * explanation (display_explanation_page()) and the long-press that ends a
 * review (display_review()) are the only two shapes it takes. It does not
 * mean "the recommended option" and it does not mean "the largest of these
 * amounts" -- it meant the latter on the two length screens, which is the
 * third meaning that made this paragraph necessary. Screens that only carry
 * the reader on get the grey tap-to-continue instead, and screens that offer
 * peers -- every "choose among N" -- emphasise nothing.
 *
 * **An icon says a state or an identity, never a format.** The verdicts and
 * the warnings keep theirs (CHECK_CIRCLE_ICON, DENIED_CIRCLE_ICON,
 * IMPORTANT_CIRCLE_ICON) and so does the home page, because those say what
 * has happened or whose application this is. The icons this repository
 * authored -- icon_bip39, icon_sskr, icon_bip85 -- name formats, and a format
 * over a question about a quantity ("how many digits in the PIN?") answers a
 * question nobody asked; they were on the two length screens and are not any
 * more. Where a format icon *does* still appear it is beside a row of an
 * explanation, naming the thing that row is about, which is an identity.
 *
 * **A title belongs to the component that draws it.** There are two left, and
 * both wrap on words by themselves: the header of the list use case
 * (display_choice_list()), and the title of an explanation
 * (nbgl_layoutLeftContent_t::title). Two hand-built mechanisms are gone with
 * the button stacks -- they hung a text area under an icon or under the back
 * button, left `wrapping` clear, and so broke on *characters*, which is why
 * every title they drew carried a hand-placed "\n". No title in
 * src/common/ui_strings.h needs one now, and a new one that seems to is a
 * title in the wrong component rather than a string that needs a break.
 */

/*
 * Sized on the BIP85 result label, which is the longest thing written into it:
 * the widest of the four result headers, a newline, and a full derivation
 * path. Everything else it carries -- "SSKR share 16 of 16", "BIP39 Phrase" --
 * is far shorter. The _Static_assert below is what holds the number.
 */
#define HEADER_SIZE 96

static nbgl_page_t* pageContext;

static nbgl_layout_t* layout = 0;

static char headerText[HEADER_SIZE] = {0};
static char reviewText[(SSKR_SHARES_MAX_LENGTH / 16) + 1] = {0};

/*
 * Two of the four keypads name a bound that is only known at display time --
 * the threshold's maximum is the share count just entered, the password
 * length's pair depends on which application was chosen -- so their titles are
 * composed rather than declared.
 *
 * This buffer cannot live on the stack of the function that fills it.
 * nbgl_layoutAddKeypadContent() stores the pointer it is handed
 * (`textArea->text = title`, lib_nbgl/src/nbgl_layout_keypad.c) and the title
 * area is redrawn while the keypad is up, so the text has to outlive the call.
 *
 * The size is taken from the formats themselves rather than counted by hand:
 * every `%d` below stands for a value of at most two digits, and `%d` is
 * itself two characters wide, so a composed title is never longer than its own
 * format literal. The two-digit half of that premise is not assumed: the
 * _Static_asserts further down check it against the constants that supply the
 * values, and tests/unit/tests/ui_strings.c checks that each format still
 * takes exactly the arguments its call site passes.
 *
 * One buffer, not two: the SSKR generation flow and the BIP85 password flow
 * are reached from different branches of the tool menu and neither keypad is
 * on screen while the other is.
 */
#define KEYPAD_TITLE_SIZE                               \
    (sizeof(UI_STR_NBGL_SSKR_THRESHOLD_TITLE) >         \
             sizeof(UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE) \
         ? sizeof(UI_STR_NBGL_SSKR_THRESHOLD_TITLE)     \
         : sizeof(UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE))
static char keypadTitle[KEYPAD_TITLE_SIZE] = {0};

/*
 * The values the two review screens display, and the one composed title.
 *
 * Same reason the keypad title above cannot be a local: nbgl memorises the
 * pointer rather than the text. nbgl_useCaseGenericReview() keeps the whole
 * content list it is handed, `pairs` pointer and all, and the pages are drawn
 * long after the function that filled them returned; nbgl_useCaseChoice()
 * keeps `message` and `subMessage` the same way. A composed string on the
 * stack would be redrawn from freed memory.
 *
 * Sized from the formats themselves, as the keypad title is. Every `%d` is two
 * characters wide, so a composed value is never longer than its own format
 * literal *provided* every argument stays within two digits -- and the word
 * total does not: sixteen shares of forty-six words is 736. The
 * _Static_asserts further down check each bound against the constant that
 * supplies it rather than assuming any of them.
 *
 * One set, not one per screen: the SSKR generation flow and the BIP85
 * derivation flow are reached from different menu entries, and no screen of
 * one is on display while a screen of the other is being composed.
 */
#define REVIEW_VALUE_SIZE 24
#define CONFIRM_TITLE_SIZE sizeof(UI_STR_NBGL_SSKR_CLOSE_CONFIRM_TITLE)

static char reviewValueShares[REVIEW_VALUE_SIZE] = {0};
static char reviewValueThreshold[REVIEW_VALUE_SIZE] = {0};
static char reviewValueWords[REVIEW_VALUE_SIZE] = {0};
static char reviewValueApp[REVIEW_VALUE_SIZE] = {0};
static char reviewValueIndex[REVIEW_VALUE_SIZE] = {0};
static char reviewValueLength[REVIEW_VALUE_SIZE] = {0};
static char reviewValuePath[BIP85_PATH_STRING_MAX_LENGTH] = {0};
// The one explanation row composed at display time; sized on its format, whose
// only argument is a word count of at most three digits.
#define EXPLANATION_ROW_SIZE 128
static char sskrNumbersText[EXPLANATION_ROW_SIZE] = {0};
_Static_assert(sizeof(UI_STR_NBGL_SSKR_NUMBERS_ROW_WORDS) <=
                   EXPLANATION_ROW_SIZE,
               "the composed row is sized on its own format; %d stands for a "
               "three-digit word count, which is narrower than the two "
               "characters it replaces");
static char confirmTitle[CONFIRM_TITLE_SIZE] = {0};
// Where the BIP85 result label is composed before it replaces headerText.
// Separate because SPRINTF() cannot read and write the same buffer.
static char resultLabel[HEADER_SIZE] = {0};

unsigned int tool_type;

/*
 * What the user picked from the menu. tool_type says which of the three kinds
 * of data is being entered and is what compare_recovery_phrase() dispatches
 * on; this says what it is being entered for, which the tool cannot express:
 * checking a phrase and backing one up are both TOOL_TYPE_BIP39.
 *
 * Defaulted to the first entry so that no screen reads it unset. Every path
 * to a screen that consults it goes through select_menu_callback(), which
 * writes it.
 *
 * volatile, and for one reason: the bound in display_check_result_page()
 * disappears without it. Every write to this variable in this file is a
 * constant between 0 and 3 and the variable is static, so the compiler proves
 * `user_intent < USER_INTENT_NB` and folds the check away -- measured: _etext
 * was byte-identical on all three touch targets with the bound present and
 * with it removed. What the bound is there for is a byte that changed without
 * anyone writing it, which is exactly the case that proof excludes. Same
 * reason `checkpoints` is volatile in compare_recovery_phrase_finish()
 * (src/common/common_seed.c).
 */
static volatile user_intent_e user_intent = USER_INTENT_CHECK;

static void display_home_page(void);
static void display_select_menu_page(void);
static void display_check_keyboard_page(void);
static void display_check_result_page(const bool result);
static void display_backup_explain_page(void);
static void display_sskr_numbers_page(void);
static void display_sskr_threshold_concept_page(void);
static void display_bip85_index_concept_page(void);
static void display_sskr_select_numshares_page(void);
static void display_bip85_concept_page(void);
static void display_recover_concept_page(void);
static void display_bip39_select_phrase_length_page(void);
static void display_generic_review(void);
static void display_sskr_select_numshares_page(void);
static void display_sskr_select_threshold_page(void);
static void display_bip85_select_app_page(void);
static void display_bip85_select_index_page(void);
static void display_bip85_select_password_length_page(void);
static void display_bip85_select_pin_length_page(void);
static void display_sskr_shares_review(void);
static void display_sskr_close_confirm_page(void);
static void display_sskr_generate_review_page(void);
static void display_bip85_generate_review_page(void);

/*
 * Utils
 */
static const char* buttonTexts[NB_MAX_SUGGESTION_BUTTONS] = {0};

// Where the characters those pointers point at actually live. Declared here
// rather than beside the keyboard it serves so that reset_globals() below can
// reach it: the two are one object, and only one of them was being cleared.
// The longest BIP39 word is 8 characters, 9 with its terminator, and at most
// NB_MAX_SUGGESTION_BUTTONS are shown.
static char wordCandidates[(BIP39_MAX_WORD_LENGTH + 1) *
                           NB_MAX_SUGGESTION_BUTTONS] = {0};

/*
 * Appends `src` to `dst`, never writing past `size`, and reports whether all
 * of it fitted.
 *
 * SPRINTF() would be shorter and is not available for this: string
 * conversions are refused anywhere under src/, because that is how a secret
 * leaks through a trace statement, and the rule is enforced by grep rather
 * than by reading -- so it applies to a screen's format string exactly as it
 * does to a PRINTF.
 *
 * The return value is checked at both call sites, and neither shows a label
 * that was composed only in part: each keeps what it had before the failed
 * append rather than the truncated result. On the BIP-85 result label that
 * means the header without the derivation path, which is a label that says
 * less than it should rather than one that says something wrong.
 *
 * What keeps it from happening at all is not this check but the
 * _Static_assert on HEADER_SIZE further down, which bounds the widest header
 * plus a seven-digit index, a newline and a full path against the buffer.
 * The check is what makes the failure legible if that assert is ever weakened.
 */
static bool append_bounded(char* dst, size_t size, const char* src) {
    const size_t used = strlen(dst);
    const size_t room = (used < size) ? size - used - 1 : 0;
    const size_t len = strlen(src);

    if (len > room) {
        return false;
    }
    memcpy(dst + used, src, len + 1);
    return true;
}

static void reset_globals() {
    bip39_mnemonic_reset();
    sskr_shares_reset();
    bip85_app_reset();
    memzero(buttonTexts, sizeof(buttonTexts[0]) * NB_MAX_SUGGESTION_BUTTONS);
    // buttonTexts is only the array of pointers; wordCandidates holds the
    // characters, and it was left behind. What survives there is the set of
    // BIP-39 words matching the prefix last typed -- not the phrase, but
    // enough to narrow one of its words -- and the rule this function applies
    // is "everything a screen composed", not "everything that is itself
    // secret". textToEnter needs nothing here: both keyboard dispatchers
    // memzero it before they route anywhere, so it cannot outlive the
    // keyboard.
    memzero(wordCandidates, sizeof(wordCandidates));
    memzero(headerText, sizeof(headerText));
    memzero(reviewText, sizeof(reviewText));
    // No secret in it -- a share count and a length range, never a word or a
    // share. Cleared anyway because headerText, one line up, carries the same
    // class of thing ("SSKR share 2 of 3") and is cleared: leaving one of the
    // two behind would make the rule here look like a judgement call.
    memzero(keypadTitle, sizeof(keypadTitle));
    // The review values, for the same reason and with one of them mattering
    // more than the rest: reviewValuePath is the BIP85 derivation path, which
    // is not a secret but does say exactly which secret was derived. The rule
    // applied here is "everything a screen composed", not "everything secret",
    // because deciding that case by case is how one gets left behind.
    memzero(reviewValueShares, sizeof(reviewValueShares));
    memzero(reviewValueThreshold, sizeof(reviewValueThreshold));
    memzero(reviewValueWords, sizeof(reviewValueWords));
    memzero(resultLabel, sizeof(resultLabel));
    memzero(reviewValueApp, sizeof(reviewValueApp));
    memzero(reviewValueIndex, sizeof(reviewValueIndex));
    memzero(reviewValueLength, sizeof(reviewValueLength));
    memzero(reviewValuePath, sizeof(reviewValuePath));
    memzero(confirmTitle, sizeof(confirmTitle));
    memzero(sskrNumbersText, sizeof(sskrNumbersText));
}

static void on_quit(void) { os_sched_exit(-1); }

/*
 * Choosing among N, and the rule for it.
 *
 * Every screen of this application that asks for one of a fixed set of
 * entries is drawn by nbgl_useCaseGenericConfiguration() over a single
 * BARS_LIST content, through this function. There are four of them -- the
 * menu of intentions, the BIP-39 phrase length, the PIN length, and the list
 * of BIP-85 secrets -- and there is no second idiom.
 *
 * What the rule excludes is the point of writing it down: no screen of this
 * family is built from nbgl_screenSet() and a stack of nbgl_button_t. Three
 * of the four were, and the four-entry one is what ended it. That stack grows
 * upwards from the bottom margin, so the entry that pushes it too far is
 * drawn *over* the title, and nothing in a test can see it -- Speculos
 * reports every text event at its full height with its full text, which is
 * the same silence reviews.assert_body_clears_button() exists to break. A
 * list paginates instead, so a fifth entry on any of these screens costs a
 * page rather than a silent overlap.
 *
 * Three further properties come with the component and are not separately
 * decided here:
 *
 *   - entries read top-down, in the order of the array they are given in. The
 *     button stack read bottom-up, so its first entry was the last line on
 *     screen, and the functional tests counted from the end the code did not;
 *
 *   - there is one back arrow, the SDK's, in the header that carries the
 *     title. generic_screen_set_back_button() drew a second one of its own
 *     geometry -- a BUTTON_DIAMETER square 4px from the top, against the
 *     SDK's BACK_BUTTON_HEADER_HEIGHT band -- and the two coincided on the
 *     three current devices by arithmetic rather than by design;
 *
 *   - the title wraps on words by itself, so none of these titles carries a
 *     hand-placed "\n". The two hand-built title mechanisms leave `wrapping`
 *     unset and break on characters, which is why every title they drew had
 *     one.
 *
 * No entry is emphasised, and the component is only half the reason:
 * nbgl_contentBarsList_t carries texts and tokens and nothing else, and a
 * black control in this application means an act with a consequence -- see
 * the note above display_explanation_page(). "The largest of three amounts"
 * was a third meaning for that signal, drawn on the two length screens, and
 * on the phrase length it answered a question about a fact ("how long *is*
 * your Recovery Phrase?") with a recommendation.
 *
 * The three-entry screens were captured both ways on all three devices
 * before this rule was settled, because a list of three on a large screen was
 * the objection to it: a list leaves the bottom of the screen empty -- about
 * 45% of Stax, 35% of apex_p -- where the button stack filled it. That is the
 * cost, and it buys the entries reading in the order they are written, one
 * back arrow instead of two geometries, and the overprint class of defect
 * gone from screens nothing could check. A short list at the top of a large
 * screen is also what the SDK's own settings screens look like, so it reads
 * as this device's shape rather than as an unfinished screen.
 *
 * And what it costs elsewhere: a bar has no second line here.
 * nbgl_layoutBar_t has a `subText` field, but the content that reaches it
 * through a generic
 * configuration does not -- nbgl_contentBarsList_t is barTexts, tokens,
 * nbBars and a tune. An entry says what it has to say in its own label or it
 * is renamed. Reaching subText would mean nbgl_layoutGet() and
 * nbgl_layoutAddTouchableBar(), which is the hand-built idiom again with the
 * pagination and the header to write by hand.
 */
static void display_choice_list(const char* title, const char* const* entries,
                                const uint8_t* tokens, uint8_t nbEntries,
                                nbgl_contentActionCallback_t onTouch,
                                nbgl_callback_t onBack) {
    /*
     * Static, and it has to be, for the reason display_review() gives: the
     * use case keeps the pointers it is handed and reads them again on every
     * page turn, so a local would be a dangling read. Uninitialised, because
     * BOLOS refuses a non-empty .data section -- hence the memset rather than
     * an initialiser.
     *
     * One set for all four screens: they are reached from different branches
     * and no two of them are on display at once.
     */
    static nbgl_content_t contents[1];
    static nbgl_genericContents_t generic;

    memset(contents, 0, sizeof(contents));

    contents[0].type = BARS_LIST;
    contents[0].content.barsList.barTexts = entries;
    contents[0].content.barsList.tokens = tokens;
    contents[0].content.barsList.nbBars = nbEntries;
    contents[0].content.barsList.tuneId = TUNE_TAP_CASUAL;
    contents[0].contentActionCallback = onTouch;

    generic.callbackCallNeeded = false;
    generic.contentsList = contents;
    generic.nbContents = 1;

    nbgl_useCaseGenericConfiguration(title, 0, &generic, onBack);
}

/*
 * The menu: one entry per intention.
 *
 * No icon, where the three-entry menu had the application's, and the reason
 * outlives the layout it was measured against: the icons this repository has
 * name formats -- icon_bip39, icon_sskr, icon_bip85 -- while these entries
 * name intentions. Generate and Recover would both have taken icon_sskr,
 * which is exactly what the previous menu did to its BIP85 entry: it wore the
 * SSKR icon, so two of three buttons were illustrated identically. A list
 * does not offer per-entry icons at all, so nothing here has to hold that
 * line any more -- it is written down because it is why nobody should go
 * looking for a component that would.
 *
 * One token per entry rather than one token read with the row index, for the
 * reason the BIP85 list gives at length: the index the SDK reports is a
 * position on the page it drew, and a fifth intention would put an entry on a
 * page of its own with an index of 0.
 */
enum __attribute__((packed)) select_menu_token {
    SELECT_MENU_CHECK_TOKEN = FIRST_USER_TOKEN,
    SELECT_MENU_BACKUP_TOKEN,
    SELECT_MENU_RECOVER_TOKEN,
    SELECT_MENU_DERIVE_TOKEN,
};

// In reading order, top-down, which is also the order the screen draws them.
static const char* const select_menu_entries[] = {
    UI_STR_NBGL_MENU_CHECK, UI_STR_NBGL_MENU_BACKUP, UI_STR_NBGL_MENU_RECOVER,
    UI_STR_NBGL_MENU_DERIVE};

static const uint8_t select_menu_tokens[] = {
    SELECT_MENU_CHECK_TOKEN, SELECT_MENU_BACKUP_TOKEN,
    SELECT_MENU_RECOVER_TOKEN, SELECT_MENU_DERIVE_TOKEN};

_Static_assert(ARRAYLEN(select_menu_tokens) == ARRAYLEN(select_menu_entries),
               "every entry of the menu needs the token that says which "
               "intention it is; the SDK reads the two arrays in step");
_Static_assert(USER_INTENT_NB == ARRAYLEN(select_menu_entries),
               "the menu shows one entry per intention");

static void select_menu_action(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);

    // Each entry sets both: the intention, and the kind of data the screens
    // it leads to will be handed. Checking a phrase and backing one up are
    // the same tool and different intentions, which is the whole reason the
    // two are separate variables.
    //
    // No default: the tokens are this screen's own enumeration, and a fifth
    // intention has to say here where it goes rather than falling through to
    // whichever branch was last.
    switch ((enum select_menu_token)token) {
        case SELECT_MENU_CHECK_TOKEN:
            user_intent = USER_INTENT_CHECK;
            tool_type = TOOL_TYPE_BIP39;
            /*
             * Straight to the length choice, with nothing explaining why the
             * Phrase is wanted -- the shape app-recovery-check has, and the
             * one this journey had before the menu existed.
             *
             * Entering the Phrase *is* the task here. Someone who chose
             * "Check Recovery Phrase" is not owed a screen telling them a
             * check needs the Phrase. The Backup journey keeps its own,
             * because there the user asked for Shares and being asked for
             * twenty-four words is a surprise that has to be accounted for.
             */
            display_bip39_select_phrase_length_page();
            break;
        case SELECT_MENU_BACKUP_TOKEN:
            user_intent = USER_INTENT_BACKUP;
            tool_type = TOOL_TYPE_BIP39;
            display_backup_explain_page();
            break;
        case SELECT_MENU_RECOVER_TOKEN:
            user_intent = USER_INTENT_RECOVER;
            tool_type = TOOL_TYPE_SSKR;
            display_recover_concept_page();
            break;
        case SELECT_MENU_DERIVE_TOKEN:
            user_intent = USER_INTENT_DERIVE;
            tool_type = TOOL_TYPE_BIP85;
            display_bip85_concept_page();
            break;
    }
}

static void display_select_menu_page(void) {
    display_choice_list(UI_STR_NBGL_MENU_TITLE, select_menu_entries,
                        select_menu_tokens, ARRAYLEN(select_menu_entries),
                        &select_menu_action, &display_home_page);
}

/*
 * Select Recover BIP39
 */
static void select_recover_bip39_choice(bool bip39_rec) {
    nbgl_layoutRelease(layout);
    if (bip39_rec) {
        SPRINTF(headerText, UI_STR_NBGL_BIP39_PHRASE_TITLE);
        strncpy(reviewText, bip39_mnemonic_get(), bip39_mnemonic_length_get());
        // Ensure null termination
        reviewText[bip39_mnemonic_length_get()] = '\0';
        display_generic_review();
    } else {
        // display_home_page() calls reset_globals(), and here that matters
        // more than anywhere else on this path: words_buffer holds the phrase
        // the shares just rebuilt. Declining erases it rather than leaving it
        // in RAM behind the home screen.
        display_home_page();
    }
}

/*
 * The screen that stands in front of a rebuilt recovery phrase.
 *
 * It used to be an offer -- "Recover BIP39 Phrase?", with a description
 * explaining that the user could choose to rebuild the phrase from their valid
 * shares. That question had already been answered twice by the time it
 * appeared: once at the menu, by choosing "Recover from backup", and once by
 * typing in the shares. What it never said was the only thing worth saying
 * there, which is that a recovery phrase is about to be drawn on the screen.
 *
 * The second sentence of the description is the point the previous change left
 * implicit. This path shows the rebuilt phrase whether or not it matches the
 * seed this Ledger holds: check_result_callback() gates it on
 * sskr_shares_check() succeeding -- that the shares recombined -- and not on
 * seed_match, unlike the backup flow beside it. That is deliberate and it is
 * what the feature is for: rebuilding your phrase from your own shares onto a
 * spare or replacement device is the case a backup exists to serve, and it is
 * exactly the case where the device does not already hold the phrase.
 * Requiring a match would delete the feature on the day it is needed.
 *
 * So the guard stays where it is and the warning carries what the guard does
 * not promise: what appears is what the entered shares rebuild, and not
 * necessarily this device's phrase. The verdict screen immediately before has
 * already reported whether the two agreed.
 */
void display_select_recover_bip39_page(void) {
    nbgl_useCaseChoice(&IMPORTANT_CIRCLE_ICON, UI_STR_NBGL_RECOVER_WARN_TITLE,
                       UI_STR_NBGL_RECOVER_WARN_DESC,
                       UI_STR_NBGL_CONTINUE_ANYWAY, UI_STR_NBGL_BACK_TO_SAFETY,
                       select_recover_bip39_choice);
}

/*
 * The explanation screens.
 *
 * Built from the layout API, because the motif has no use-case form: a title
 * over rows that each carry their own icon. Two facts side by side read as two
 * facts; folded into one paragraph the second is the tail of a sentence about
 * something else, and is what a reader skips.
 *
 * nbgl_useCaseAction() was tried instead and draws one centered paragraph, no
 * rows. nbgl_useCaseGenericReview() was tried before that and draws its reject
 * text on every page, so each explanation offered to cancel something the
 * reader had not yet been asked to do. Neither is this motif.
 *
 * What carries on is deliberately not a black button. A black button is what
 * this application uses for an act with a consequence -- entering a phrase,
 * generating shares, revealing a secret. Reading an explanation is none of
 * those, and giving it the same weight tells the reader it is one.
 *
 * A screen longer than the height allows is several of these in a row, each
 * carrying on into the next, rather than one screen with a smaller sentence.
 *
 * What the height allows, and it is a height rather than a row count. A row
 * costs INTER_ROWS_MARGIN plus its wrapped lines plus
 * LEFT_CONTENT_TEXT_PADDING, so three short rows can fit where two long ones do
 * not. Measured from the rendered screens: about 8 lines of text, comfortable
 * at 7; a one-line title is about 20 characters and a row line about 26.
 *
 * Flex is the binding target, not apex_p. Wrapping turns on single pixels --
 * apex fit a 29-character line in 236px of 236 while Flex broke the same string
 * at 22 -- so any string changed here goes back through a Speculos capture on
 * Flex before it is believed.
 */
enum {
    EXPLANATION_BACK_TOKEN = FIRST_USER_TOKEN,
    EXPLANATION_CONFIRM_TOKEN,
};

typedef struct {
    const char* title;
    const char* const* rows;
    const nbgl_icon_details_t* const* icons;
    uint8_t nbRows;
    const char* confirmText;
    /*
     * True on the screen that leads to an act rather than to more reading.
     *
     * A black button is what this application uses for an act with a
     * consequence -- entering a phrase, generating shares, revealing a secret.
     * The screens that only carry on into the next explanation get the grey
     * tap-to-continue instead, so the weight of the control says which of the
     * two the reader is about to do.
     */
    bool isAction;
    // Where carrying on leads. Going back always reaches home, so only this
    // side varies and only it is stored.
    nbgl_callback_t onConfirm;
} explanation_page_t;

// The page being drawn, for the shared callback to read. One explanation is on
// screen at a time, which is what makes a single pointer enough.
static const explanation_page_t* currentExplanation = NULL;

static void explanation_callback(int token, uint8_t index) {
    UNUSED(index);
    const explanation_page_t* page = currentExplanation;

    nbgl_layoutRelease(layout);
    if (token == EXPLANATION_CONFIRM_TOKEN) {
        ((nbgl_callback_t)PIC(page->onConfirm))();
    } else {
        display_home_page();
    }
}

static void display_explanation_page(const explanation_page_t* page) {
    currentExplanation = page;

    /*
     * tapActionText is what makes the whole content tappable and draws the
     * grey line at the bottom that names the gesture -- lib_nbgl turns it into
     * an UP_FOOTER_TEXT and wires the container to tapActionToken. It is how
     * the SDK's own read-then-continue screens carry on, and it leaves the
     * screen with no button at all.
     */
    nbgl_layoutDescription_t layoutDescription = {
        .modal = false,
        .tapActionText = page->isAction ? NULL : PIC(page->confirmText),
        .tapActionToken = EXPLANATION_CONFIRM_TOKEN,
        .tapTuneId = TUNE_TAP_CASUAL,
        .onActionCallback = &explanation_callback};
    layout = nbgl_layoutGet(&layoutDescription);

    nbgl_layoutHeader_t headerDesc = {
        .type = HEADER_BACK_AND_TEXT,
        .separationLine = false,
        .backAndText.token = EXPLANATION_BACK_TOKEN,
        .backAndText.tuneId = TUNE_TAP_CASUAL,
        .backAndText.text = NULL};
    nbgl_layoutAddHeader(layout, &headerDesc);

    /*
     * PIC() on the two arrays, and it is not decoration.
     *
     * nbgl_layoutAddLeftContent() translates what it reads out of them --
     * PIC(info->rowTexts[row]) and PIC(info->rowIcons[row]) -- but never the
     * arrays themselves: it indexes info->rowTexts directly. Handed a link-time
     * address, that read is the same fault bip85_select_app[] took this
     * application down with, found by the emulator rather than by reading.
     */
    nbgl_layoutLeftContent_t content = {
        .nbRows = page->nbRows,
        .title = PIC(page->title),
        .rowTexts = (const char**)PIC(page->rows),
        .rowIcons = (const nbgl_icon_details_t**)PIC(page->icons)};
    nbgl_layoutAddLeftContent(layout, &content);

    if (page->isAction) {
        nbgl_layoutButton_t buttonInfo = {.text = PIC(page->confirmText),
                                          .icon = NULL,
                                          .token = EXPLANATION_CONFIRM_TOKEN,
                                          .style = BLACK_BACKGROUND,
                                          .fittingContent = false,
                                          .onBottom = true,
                                          .tuneId = TUNE_TAP_CASUAL};
        nbgl_layoutAddButton(layout, &buttonInfo);
    }

    nbgl_layoutDraw(layout);
}

/*
 * The two numbers, read immediately before the keypad that asks for the first
 * of them. The words per Share depend on the phrase length, unknown at the top
 * of the journey and known here, so the second row is composed rather than
 * literal: 29, 38 or 46 words for a 12, 18 or 24-word phrase.
 */
static const char* k_sskr_numbers_rows[2] = {0};
static const nbgl_icon_details_t* const k_sskr_numbers_icons[] = {
    &SSKR_ICON_SMALL, &CHECKED_ICON};

static void display_sskr_numbers_page(void) {
    snprintf(sskrNumbersText, sizeof(sskrNumbersText),
             UI_STR_NBGL_SSKR_NUMBERS_ROW_WORDS,
             bolos_ux_sskr_share_wordcount(bip39_mnemonic_final_size_get()));
    k_sskr_numbers_rows[0] = UI_STR_NBGL_SSKR_NUMBERS_ROW_CREATE;
    k_sskr_numbers_rows[1] = sskrNumbersText;

    // const, like every other page: BOLOS refuses a non-empty .data section,
    // and only the array this points at changes, never the struct.
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_SSKR_NUMBERS_TITLE,
        .rows = k_sskr_numbers_rows,
        .icons = k_sskr_numbers_icons,
        .nbRows = 2,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_sskr_select_numshares_page};
    display_explanation_page(&page);
}

/*
 * The threshold, read immediately before the keypad titled "Enter threshold
 * value" -- the only place that word appears cold.
 */
static const char* const k_sskr_threshold_rows[] = {
    UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_REBUILD,
    UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_ROW_LOST};
static const nbgl_icon_details_t* const k_sskr_threshold_icons[] = {
    &SSKR_ICON_SMALL, &CHECKED_ICON};

static void display_sskr_threshold_concept_page(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_SSKR_THRESHOLD_CONCEPT_TITLE,
        .rows = k_sskr_threshold_rows,
        .icons = k_sskr_threshold_icons,
        .nbRows = 2,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_sskr_select_threshold_page};
    display_explanation_page(&page);
}

/*
 * What an index is, before the keypad that demands one.
 *
 * Neither icon means anything here, and that is stated rather than hidden: the
 * SDK ships no glyph for "one of many" or for a counter, and the app's own
 * marks name formats. BIP85_ICON_SMALL at least names the standard the index
 * belongs to; CHECKED_ICON on the second row is a placeholder for a glyph that
 * does not exist. Both should be revisited if the missing artwork is drawn.
 */
static const char* const k_bip85_index_rows[] = {
    UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_TELLS,
    UI_STR_NBGL_BIP85_INDEX_CONCEPT_ROW_COUNT};
static const nbgl_icon_details_t* const k_bip85_index_icons[] = {
    &BIP85_ICON_SMALL, &CHECKED_ICON};

static void display_bip85_index_concept_page(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_BIP85_INDEX_CONCEPT_TITLE,
        .rows = k_bip85_index_rows,
        .icons = k_bip85_index_icons,
        .nbRows = 2,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_bip85_select_index_page};
    display_explanation_page(&page);
}

/*
 * What deriving means, before an application is chosen. The first row is the
 * property that makes BIP-85 worth using at all; the other two are the trap it
 * comes with, and they are deliberately not what the review says. The review
 * *shows* the path, and showing is not telling.
 */
static const char* const k_bip85_concept_rows[] = {
    UI_STR_NBGL_BIP85_ROW_DERIVED, UI_STR_NBGL_BIP85_ROW_PATH,
    UI_STR_NBGL_BIP85_ROW_LOST};
// The format mark is gone from the rows: the whole screen is BIP-85, so a
// BIP-85 icon on one of three rows distinguishes nothing. ROUND_WARN_ICON is
// the SDK's own size-agnostic alias and needs none of its own.
static const nbgl_icon_details_t* const k_bip85_concept_icons[] = {
    &PRIVACY_ICON, &ROUND_WARN_ICON, &ROUND_WARN_ICON};

static void display_bip85_concept_page(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_BIP85_TITLE,
        .rows = k_bip85_concept_rows,
        .icons = k_bip85_concept_icons,
        .nbRows = 3,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_bip85_select_app_page};
    display_explanation_page(&page);
}

/*
 * What rebuilding asks for, before the first word.
 */
static const char* const k_recover_concept_rows[] = {
    UI_STR_NBGL_RECOVER_ROW_SUBSET, UI_STR_NBGL_RECOVER_ROW_ORDER};
static const nbgl_icon_details_t* const k_recover_concept_icons[] = {
    &SSKR_ICON_SMALL, &CHECKED_ICON};

static void display_recover_concept_page(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_RECOVER_TITLE,
        .rows = k_recover_concept_rows,
        .icons = k_recover_concept_icons,
        .nbRows = 2,
        .isAction = true,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_check_keyboard_page};
    display_explanation_page(&page);
}

/*
 * The Backup journey, on two screens.
 *
 * The first is SSKR: what the journey makes, and what has to be done with what
 * it makes. Those two are one subject and belong side by side -- splitting a
 * Phrase into Shares is only a backup if the Shares are kept apart, and a
 * reader who writes them all on one sheet has made a copy of their Phrase and
 * none of the safety.
 *
 * The second is why the Phrase is asked for, which is a different subject and
 * is the screen that leads to the keyboard. One screen could not hold both:
 * four rows do not fit, and the title had to promise one subject or the other.
 */
static const char* const k_backup_sskr_rows[] = {UI_STR_NBGL_BACKUP_ROW_SPLIT,
                                                 UI_STR_NBGL_BACKUP_ROW_APART};
static const nbgl_icon_details_t* const k_backup_sskr_icons[] = {
    &SSKR_ICON_SMALL, &PRIVACY_ICON};

static const char* const k_backup_phrase_rows[] = {
    UI_STR_NBGL_PHRASE_NOT_READABLE, UI_STR_NBGL_BACKUP_ROW_CHECKED};
static const nbgl_icon_details_t* const k_backup_phrase_icons[] = {
    &PRIVACY_ICON, &CHECKED_ICON};

static void display_backup_explain_page_2(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_BACKUP_PHRASE_TITLE,
        .rows = k_backup_phrase_rows,
        .icons = k_backup_phrase_icons,
        .nbRows = 2,
        .isAction = true,
        .confirmText = UI_STR_NBGL_BACKUP_EXPLAIN_CONFIRM,
        .onConfirm = &display_bip39_select_phrase_length_page};
    display_explanation_page(&page);
}

static void display_backup_explain_page(void) {
    static const explanation_page_t page = {
        .title = UI_STR_NBGL_BACKUP_SSKR_TITLE,
        .rows = k_backup_sskr_rows,
        .icons = k_backup_sskr_icons,
        .nbRows = 2,
        .confirmText = UI_STR_NBGL_EXPLANATION_CONTINUE,
        .onConfirm = &display_backup_explain_page_2};
    display_explanation_page(&page);
}

/*
 * Select mnemonic size page
 *
 * A choice among three, so it is the list of display_choice_list() and not a
 * keypad: see the note above display_sskr_select_numshares_page() for where
 * the line between the two is drawn.
 *
 * No icon, where this screen carried BIP39_ICON. The icons this repository
 * has name formats, and this screen asks a quantity; an icon saying "BIP39"
 * over "How long is your Recovery Phrase?" adds nothing the title has not
 * already said. The same reasoning that kept one off the menu.
 */
enum __attribute__((packed)) select_bip39_phrase_length_token {
    SELECT_BIP39_PHRASE_LENGTH_12_TOKEN = FIRST_USER_TOKEN,
    SELECT_BIP39_PHRASE_LENGTH_18_TOKEN,
    SELECT_BIP39_PHRASE_LENGTH_24_TOKEN,
};

// Ascending, which is the order the list draws them in. The button stack read
// 24, 18, 12 from the top, because it stacked upwards from the bottom and 12
// was written first; nothing chose that order, the layout did.
static const char* const bip39_phrase_length_entries[] = {
    UI_STR_WORDS_12, UI_STR_WORDS_18, UI_STR_WORDS_24};

static const uint8_t bip39_phrase_length_tokens[] = {
    SELECT_BIP39_PHRASE_LENGTH_12_TOKEN, SELECT_BIP39_PHRASE_LENGTH_18_TOKEN,
    SELECT_BIP39_PHRASE_LENGTH_24_TOKEN};

_Static_assert(ARRAYLEN(bip39_phrase_length_tokens) ==
                   ARRAYLEN(bip39_phrase_length_entries),
               "every length this screen offers needs the token that says "
               "which one it is; the SDK reads the two arrays in step");

// Back goes to whatever asked for this screen, which is three different
// things. Written on the intention rather than on the tool because Check and
// Backup share the tool and do not share a caller.
static void bip39_phrase_length_back(void) {
    switch (user_intent) {
        case USER_INTENT_DERIVE:
            display_bip85_select_app_page();
            break;
        case USER_INTENT_BACKUP:
            display_backup_explain_page();
            break;
        case USER_INTENT_CHECK:
        case USER_INTENT_RECOVER:
        case USER_INTENT_NB:
            // Recover enters ByteWords and never chooses a BIP-39 length; it
            // is grouped with Check so that this switch stays exhaustive and
            // a new intention is a -Wswitch diagnostic rather than a silent
            // fall onto someone else's back button.
            display_select_menu_page();
            break;
    }
}

static void select_bip39_phrase_length_action(int token, uint8_t index,
                                              int page) {
    UNUSED(index);
    UNUSED(page);

    // No default, as on every screen of this family: a fourth length has to
    // say here what size it sets.
    switch ((enum select_bip39_phrase_length_token)token) {
        case SELECT_BIP39_PHRASE_LENGTH_12_TOKEN:
            bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_12);
            break;
        case SELECT_BIP39_PHRASE_LENGTH_18_TOKEN:
            bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_18);
            break;
        case SELECT_BIP39_PHRASE_LENGTH_24_TOKEN:
            bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_24);
            break;
    }
    if (user_intent == USER_INTENT_DERIVE) {
        // The explanation, not the keypad. Only on this path: the same keypad
        // is re-entered from its own range error further down, and an
        // explanation between a rejected value and a second attempt would read
        // as a reprimand.
        display_bip85_index_concept_page();
    } else {
        display_check_keyboard_page();
    }
}

// Three intentions reach the length screen and two questions are asked on it.
// Check and Backup both type in a phrase that already exists, so both ask how
// long it is; Derive asks how long a phrase to produce. The ternary that used
// to do this had two branches because it had two callers.
static const char* bip39_length_title(void) {
    switch (user_intent) {
        case USER_INTENT_CHECK:
        case USER_INTENT_BACKUP:
            return UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK;
        case USER_INTENT_DERIVE:
            return UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE;
        case USER_INTENT_RECOVER:
        case USER_INTENT_NB:
            break;
    }
    // Recover does not come here; the question it would be asked is still the
    // one about a phrase being typed in, not one being produced.
    return UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK;
}

static void display_bip39_select_phrase_length_page(void) {
    display_choice_list(
        bip39_length_title(), bip39_phrase_length_entries,
        bip39_phrase_length_tokens, ARRAYLEN(bip39_phrase_length_entries),
        &select_bip39_phrase_length_action, &bip39_phrase_length_back);
}

/*
 * Word recover page
 */
#define BUTTON_VMARGIN 32

enum __attribute__((packed)) check {
    CHECK_BACK_BUTTON_TOKEN = FIRST_USER_TOKEN,
    CHECK_FIRST_SUGGESTION_TOKEN,
    CHECK_RESULT_TOKEN,
};

/*
 * The token the entered-text area carries, and why it is a small number.
 *
 * Neither dispatcher below handles it: they answer CHECK_BACK_BUTTON_TOKEN
 * and treat anything at or above CHECK_FIRST_SUGGESTION_TOKEN as the n-th
 * suggestion button. So this token has to stay *below* that run, or touching
 * the word being typed would be read as picking a suggestion that is not
 * there. It sat in the middle of the phrase-length screen's enumeration until
 * that screen became a list; it belongs to the keyboard and lives here now.
 */
enum __attribute__((packed)) keyboard_text {
    KBD_TEXT_TOKEN = 1,
};
_Static_assert((int)KBD_TEXT_TOKEN < (int)CHECK_FIRST_SUGGESTION_TOKEN,
               "the text area's token must not fall in the run the "
               "suggestion buttons occupy");

static char textToEnter[BIP39_MAX_WORD_LENGTH + 1] = {0};
static int keyboardIndex = 0;
static bool seed_match = false;

/*
 * Function called when a key of keyboard is touched
 */
static void key_press_callback(const char touchedKey) {
    size_t textLen = 0;
    uint32_t mask = 0;
    // Update word currently displayed
    const size_t previousTextLen = strlen(textToEnter);
    if (touchedKey == BACKSPACE_KEY) {
        if (previousTextLen == 0) {
            return;
        }
        textToEnter[previousTextLen - 1] = '\0';
        textLen = previousTextLen - 1;
    } else {
        // What keeps this inside textToEnter today is the keyboard mask, not
        // the array: bolos_ux_bip39_get_keyboard_mask() disables every letter
        // once no wordlist entry extends the prefix, and the longest BIP-39
        // word is BIP39_MAX_WORD_LENGTH characters. That is a guard computed in
        // another file, for another purpose, that this write happens to benefit
        // from. The buffer's own size is the thing that has to hold here.
        if (previousTextLen + 1 >= sizeof(textToEnter)) {
            return;
        }
        textToEnter[previousTextLen] = touchedKey;
        textToEnter[previousTextLen + 1] = '\0';
        textLen = previousTextLen + 1;
    }

    // Update the screen (written word, suggestions, ...)
    nbgl_layoutSuggestionButtons_t suggestionButtons = {
        .buttons = PIC(buttonTexts),
        .firstButtonToken = CHECK_FIRST_SUGGESTION_TOKEN,
        .nbUsedButtons = 0,
    };
    nbgl_layoutKeyboardContent_t keyboardContent = {
        .type = KEYBOARD_WITH_SUGGESTIONS,
        .title = PIC(headerText),
        .text = PIC(textToEnter),
        .numbered = true,
        .number = tool_type == TOOL_TYPE_BIP39
                      ? bip39_mnemonic_current_word_number_get() + 1
                      : sskr_shares_current_word_number_get() + 1,
        .grayedOut = false,
        .textToken = KBD_TEXT_TOKEN,
        .suggestionButtons = suggestionButtons,
        .tuneId = TUNE_TAP_CASUAL,
    };
    // A length, never the text. Accumulated over an entry session these
    // traces spell out the whole recovery phrase, or every share of it.
    PRINTF("Current text is %zu characters\n", textLen);

    if (textLen < 2) {
        // Suggestions only when the word contains 2+ letters
        nbgl_layoutUpdateKeyboardContent(layout, &keyboardContent);
    } else {
        const size_t nbMatchingWords =
            tool_type == TOOL_TYPE_BIP39
                ? bolos_ux_bip39_fill_with_candidates(
                      (unsigned char*)&(textToEnter[0]), strlen(textToEnter),
                      wordCandidates, buttonTexts)
                : bolos_ux_sskr_fill_with_candidates(
                      (unsigned char*)&(textToEnter[0]), strlen(textToEnter),
                      wordCandidates, buttonTexts);
        keyboardContent.suggestionButtons.nbUsedButtons = nbMatchingWords;
        nbgl_layoutUpdateKeyboardContent(layout, &keyboardContent);
    }
    if (textLen > 0) {
        mask =
            tool_type == TOOL_TYPE_BIP39
                ? bolos_ux_bip39_get_keyboard_mask(
                      (unsigned char*)&(textToEnter[0]), strlen(textToEnter))
                : bolos_ux_sskr_get_keyboard_mask(
                      (unsigned char*)&(textToEnter[0]), strlen(textToEnter));
    }
    nbgl_layoutDraw(layout);
    nbgl_layoutUpdateKeyboard(layout, keyboardIndex, mask, false, LOWER_CASE);
    nbgl_refreshSpecialWithPostRefresh(BLACK_AND_WHITE_REFRESH,
                                       POST_REFRESH_FORCE_POWER_ON);
}

static void bip39_keyboard_dispatcher(const int token, uint8_t index) {
    UNUSED(index);
    memzero(textToEnter, sizeof(textToEnter));
    nbgl_layoutRelease(layout);
    if (token == CHECK_BACK_BUTTON_TOKEN) {
        if (bip39_mnemonic_word_remove()) {
            display_check_keyboard_page();
        } else {
            bip39_mnemonic_reset();
            display_bip39_select_phrase_length_page();
        }
    } else if (token >= CHECK_FIRST_SUGGESTION_TOKEN) {
        // The length only, for the same reason as above. The word number is
        // traced by bip39_mnemonic_word_add() on the very next line, so
        // nothing is lost by leaving the word out.
        PRINTF("Selected word is %zu characters\n",
               strlen(buttonTexts[token - CHECK_FIRST_SUGGESTION_TOKEN]));
        bip39_mnemonic_word_add(
            buttonTexts[token - CHECK_FIRST_SUGGESTION_TOKEN],
            strlen(buttonTexts[token - CHECK_FIRST_SUGGESTION_TOKEN]));
        if (bip39_mnemonic_complete_check()) {
            display_check_result_page(bip39_mnemonic_check(&seed_match));
        } else {
            display_check_keyboard_page();
        }
    }
}

static void sskr_keyboard_dispatcher(const int token, uint8_t index) {
    UNUSED(index);
    memzero(textToEnter, sizeof(textToEnter));
    nbgl_layoutRelease(layout);
    if (token == CHECK_BACK_BUTTON_TOKEN) {
        if (sskr_shares_word_remove()) {
            display_check_keyboard_page();
        } else {
            sskr_shares_reset();
            display_select_menu_page();
        }
    } else if (token >= CHECK_FIRST_SUGGESTION_TOKEN) {
        // The length only, as above: these are the ByteWords of the shares.
        PRINTF("Selected word is %zu characters\n",
               strlen(buttonTexts[token - CHECK_FIRST_SUGGESTION_TOKEN]));
        sskr_shares_word_add(buttonTexts[token - CHECK_FIRST_SUGGESTION_TOKEN]);
        if (sskr_shares_complete_check()) {
            display_check_result_page(sskr_shares_check(&seed_match));
        } else {
            display_check_keyboard_page();
        }
    }
}

static void display_check_keyboard_page() {
    nbgl_layoutDescription_t layoutDescription = {
        .modal = false,
        .onActionCallback = tool_type == TOOL_TYPE_BIP39
                                ? &bip39_keyboard_dispatcher
                                : &sskr_keyboard_dispatcher};
    nbgl_layoutKbd_t kbdInfo = {.lettersOnly = true,   // use only letters
                                .mode = MODE_LETTERS,  // start in letters mode
                                .keyMask = 0,          // no inactive key
                                .callback = &key_press_callback};
    textToEnter[0] = '\0';
    memzero(buttonTexts, sizeof(buttonTexts[0]) * NB_MAX_SUGGESTION_BUTTONS);
    layout = nbgl_layoutGet(&layoutDescription);
    if (tool_type == TOOL_TYPE_BIP39) {
        snprintf(headerText, HEADER_SIZE, UI_STR_NBGL_ENTER_BIP39_WORD,
                 bip39_mnemonic_current_word_number_get() + 1,
                 bip39_mnemonic_final_size_get());
    } else if (tool_type == TOOL_TYPE_SSKR) {
        snprintf(headerText, HEADER_SIZE, UI_STR_NBGL_ENTER_SSKR_WORD,
                 sskr_shareindex_get() + 1,
                 sskr_shares_current_word_number_get() + 1);
    }

    nbgl_layoutHeader_t headerDesc = {
        .type = HEADER_BACK_AND_TEXT,
        .separationLine = false,
        .backAndText.token = CHECK_BACK_BUTTON_TOKEN,
        .backAndText.tuneId = TUNE_TAP_CASUAL,
        .backAndText.text = NULL};
    nbgl_layoutAddHeader(layout, &headerDesc);

    keyboardIndex = nbgl_layoutAddKeyboard(layout, &kbdInfo);

    nbgl_layoutSuggestionButtons_t suggestionButtons = {
        .buttons = PIC(buttonTexts),
        .firstButtonToken = CHECK_FIRST_SUGGESTION_TOKEN,
        .nbUsedButtons = 0,
    };
    nbgl_layoutKeyboardContent_t keyboardContent = {
        .type = KEYBOARD_WITH_SUGGESTIONS,
        .title = PIC(headerText),
        .text = PIC(textToEnter),
        .numbered = true,
        .number = tool_type == TOOL_TYPE_BIP39
                      ? bip39_mnemonic_current_word_number_get() + 1
                      : sskr_shares_current_word_number_get() + 1,
        .grayedOut = false,
        .textToken = KBD_TEXT_TOKEN,
        .suggestionButtons = suggestionButtons,
        .tuneId = TUNE_TAP_CASUAL,
    };
    nbgl_layoutAddKeyboardContent(layout, &keyboardContent);
    nbgl_layoutDraw(layout);
}

/*
 * Home page, infos & dispatcher
 */
static void display_home_page() {
    static const char* const infoTypes[] = {UI_STR_VERSION_LABEL, APPNAME};
    static const char* const infoContents[] = {APPVERSION,
                                               UI_STR_NBGL_HOME_COPYRIGHT};
    static const nbgl_contentInfoList_t infoList = {
        .nbInfos = 2, .infoTypes = infoTypes, .infoContents = infoContents};

    reset_globals();

    nbgl_homeAction_t action = {.text = UI_STR_NBGL_HOME_ACTION,
                                .callback = PIC(display_select_menu_page)};

    nbgl_useCaseHomeAndSettings(APPNAME, &ICON_APP_HOME,
                                UI_STR_NBGL_HOME_DESCRIPTION, INIT_HOME_PAGE,
                                NULL, &infoList, &action, on_quit);
}

/*
 * Result page
 */
/*
 * This runs when the user taps to leave the result screen, and it calls
 * bip39_mnemonic_check() / sskr_shares_check() a second time -- so
 * compare_recovery_phrase(), the device-seed derivation and the whole verdict,
 * all over again. That is a second derivation the application pays for, and it
 * would be easy to remove by reading the seed_match the first call already
 * left behind.
 *
 * Deliberately kept. It is a second, independent decision standing between a
 * single glitched verdict and the screen the user is sent on to, taken on
 * freshly derived bytes rather than on a stored flag. It was not put here for
 * that -- it is where the navigation happens to lead -- but it is worth what
 * it costs, and naming it here is what stops it being refactored away as
 * duplicate work.
 *
 * It is not a substitute for the hardening in compare_recovery_phrase_finish()
 * and must not be read as one: the result screen the user reads and acts on
 * has already been painted on the strength of the first call, and the BAGL
 * targets call once, not twice.
 */
static void check_result_callback(int token __attribute__((unused)),
                                  uint8_t index __attribute__((unused))) {
    // Where the verdict leads, per intention. Only the two flows that have
    // somewhere to go re-run the check; the others fall through to the home
    // page below. Written as a switch with no default so that a fifth
    // intention has to say here what it does after a verdict, rather than
    // inheriting "go home" from a branch nobody wrote for it.
    switch (user_intent) {
        case USER_INTENT_BACKUP:
            if (bip39_mnemonic_check(&seed_match) && seed_match) {
                // The numbers screen, not the keypad: it is the screen that
                // says what the two numbers the keypads ask for decide, and it
                // is only from here that the words-per-Share figure exists.
                display_sskr_numbers_page();
                return;
            }
            break;
        case USER_INTENT_RECOVER:
            if (sskr_shares_check(&seed_match)) {
                display_select_recover_bip39_page();
                return;
            }
            break;
        case USER_INTENT_CHECK:
        case USER_INTENT_DERIVE:
        case USER_INTENT_NB:
            // Check ends here -- that is what makes it a destination. Derive
            // never reaches this screen at all.
            break;
    }
    reset_globals();
    display_home_page();
}

// The three answers this screen gives. An index into the tables below, and
// nothing else: what used to sit here was 1 + tool_type * 2 + seed_match into
// a five-element row, an arithmetic on an enumeration value that a fourth
// tool type would have walked off the end of, guarded by a test naming the
// two values that were safe.
enum __attribute__((packed)) verdict_outcome {
    OUTCOME_INVALID = 0,
    OUTCOME_NOMATCH,
    OUTCOME_MATCH,
    OUTCOME_NB
};

/*
 * What the verdict says, per intention and per outcome.
 *
 * Two intentions enter a BIP-39 phrase and read different things here: the
 * check flow ends on this screen, the backup flow passes through it, and
 * "doesn't match the one present on this Ledger device" is a complete answer
 * only to the first. The row is the wording, the column is the answer.
 *
 * USER_INTENT_DERIVE has no row. The BIP85 flow goes to
 * display_generic_review() and never asks for a verdict, so there is nothing
 * true to put there; leaving it NULL means that a path which did arrive here
 * with it would draw a title over an empty body -- visibly wrong on screen --
 * instead of borrowing a sentence about a phrase it never compared.
 */
_Static_assert(USER_INTENT_NB == 4,
               "verdict_body[] and verdict_title[] have one row per intention, "
               "and neither the backup row nor the recover row says what the "
               "check row says on the same tool; a new intention needs a "
               "decision in both, not a default row");
static const char* const verdict_body[USER_INTENT_NB][OUTCOME_NB] = {
    [USER_INTENT_CHECK] = {UI_STR_NBGL_RESULT_BIP39_INVALID,
                           UI_STR_NBGL_RESULT_BIP39_NOMATCH,
                           UI_STR_NBGL_RESULT_BIP39_MATCH},
    [USER_INTENT_BACKUP] = {UI_STR_NBGL_RESULT_BIP39_INVALID,
                            UI_STR_NBGL_RESULT_BACKUP_NOMATCH,
                            UI_STR_NBGL_RESULT_BACKUP_MATCH},
    [USER_INTENT_RECOVER] = {UI_STR_NBGL_RESULT_SSKR_INVALID,
                             UI_STR_NBGL_RESULT_SSKR_NOMATCH,
                             UI_STR_NBGL_RESULT_SSKR_MATCH},
};

static void display_check_result_page(const bool result) {
    // Three distinct outcomes -- invalid, doesn't match, matches -- each with
    // its own title and icon, matching what the BAGL flows
    // (ux_bip39_invalid_flow / ux_bip39_check_nomatch_flow /
    // ux_bip39_check_match_flow and their SSKR equivalents) already give the
    // user.
    // The title names the object the user handed over, which is not the same
    // object on every journey: Check and Backup take a phrase, Recover takes a
    // set of shares. It was a single row, so the Recover verdict was titled
    // after a phrase that screen never received -- see
    // UI_STR_NBGL_RESULT_SHARES_VALID_TITLE. Same shape as verdict_body[][],
    // and indexed by the same two values.
    static const char* const verdict_title[USER_INTENT_NB][OUTCOME_NB] = {
        [USER_INTENT_CHECK] = {UI_STR_NBGL_RESULT_PHRASE_INVALID_TITLE,
                               UI_STR_NBGL_RESULT_PHRASE_NOMATCH_TITLE,
                               UI_STR_NBGL_RESULT_PHRASE_VALID_TITLE},
        [USER_INTENT_BACKUP] = {UI_STR_NBGL_RESULT_PHRASE_INVALID_TITLE,
                                UI_STR_NBGL_RESULT_PHRASE_NOMATCH_TITLE,
                                UI_STR_NBGL_RESULT_PHRASE_VALID_TITLE},
        [USER_INTENT_RECOVER] = {UI_STR_NBGL_RESULT_SHARES_INVALID_TITLE,
                                 UI_STR_NBGL_RESULT_SHARES_NOMATCH_TITLE,
                                 UI_STR_NBGL_RESULT_SHARES_VALID_TITLE},
    };
    static const nbgl_icon_details_t* const icons[OUTCOME_NB] = {
        &DENIED_CIRCLE_ICON, &IMPORTANT_CIRCLE_ICON, &CHECK_CIRCLE_ICON};

    // result is false only when the phrase itself is not well formed, in
    // which case seed_match is never set true; the sum is therefore always
    // 0, 1 or 2, indexing the invalid / nomatch / match outcome respectively.
    const uint8_t outcome = (uint8_t)(result + seed_match);

    // The row index, bounded at run time and not only at compile time.
    //
    // The static assertion on verdict_body[] says a new intention has to be
    // given a row. It says nothing about a byte that is not an intention at
    // all: user_intent is a packed enum, one byte of static RAM, and the two
    // tables below are the only ones indexed by it. USER_INTENT_NB is itself
    // out of range and is a case label in three switches, so it is a value the
    // code already handles rather than one nothing can hold.
    //
    // The arithmetic this table replaced carried such a bound -- it clamped
    // to 1 for any tool_type outside the two it named -- and dropping it in
    // exchange for compile-time assertions would be a step back in a file
    // whose verdict is deliberately hardened against a single fault
    // (compare_recovery_phrase_finish(), src/common/common_seed.c). Out of
    // range reads as a check, which is the flow that ends here and shows the
    // least.
    const user_intent_e wording =
        (user_intent < USER_INTENT_NB) ? user_intent : USER_INTENT_CHECK;

    nbgl_pageInfoDescription_t info = {
        .centeredInfo.icon = icons[outcome],
        .centeredInfo.text1 = verdict_title[wording][outcome],
        .centeredInfo.text2 = verdict_body[wording][outcome],
        // Advice only on the invalid outcome; NULL elsewhere.
        // LARGE_CASE_GRAY_INFO is required for this to render as its own gray
        // line -- see the comment on UI_STR_NBGL_RESULT_INVALID_ADVICE in
        // ui_strings.h.
        .centeredInfo.text3 = result ? NULL : UI_STR_NBGL_RESULT_INVALID_ADVICE,
        .centeredInfo.style = LARGE_CASE_GRAY_INFO,
        .centeredInfo.offsetY = -16,
        // Only one of the six screens this draws continues rather than ends:
        // a phrase that matched, in the backup flow. A failure in that flow
        // is still a full stop, and still says so.
        .footerText =
            (user_intent == USER_INTENT_BACKUP && outcome == OUTCOME_MATCH)
                ? UI_STR_NBGL_RESULT_TAP_TO_CONTINUE
                : UI_STR_NBGL_RESULT_TAP_TO_DISMISS,
        .footerToken = CHECK_RESULT_TOKEN,
        .bottomButtonStyle = NO_BUTTON_STYLE,
        .tapActionText = NULL,
        .topRightStyle = NO_BUTTON_STYLE,
        .actionButtonText = NULL,
        .tuneId = TUNE_TAP_CASUAL};
    pageContext = nbgl_pageDrawInfo(&check_result_callback, NULL, &info);
    nbgl_refresh();
}

/*
 * Select number of shares page
 */

enum __attribute__((packed)) sskr_gen {
    SSKR_GEN_BACK_BUTTON_TOKEN = FIRST_USER_TOKEN
};

// UI_STR_NBGL_SSKR_NUMSHARES_TITLE and its error message both spell out 16 as
// the number of shares this screen accepts. The bound the validator below
// applies is SSS_MAX_SHARE_COUNT, which is 16 on every target that links this
// file -- it is 10 only under TARGET_NANOS, which is BAGL and never compiles
// it. Asserting the two agree is what stops a label from promising a share
// count the Shamir layer would refuse.
_Static_assert(SSS_MAX_SHARE_COUNT == 16,
               "the SSKR share-count screen spells its upper bound out in "
               "UI_STR_NBGL_SSKR_NUMSHARES_TITLE and its error message; "
               "both have to be changed with SSS_MAX_SHARE_COUNT");

// The threshold title composes SSS_MAX_SHARE_COUNT into a "%d" whose width the
// keypad title buffer is sized on. Two digits is what that sizing assumes.
_Static_assert(SSS_MAX_SHARE_COUNT <= 99,
               "the composed threshold title assumes a two-digit share count");

/*
 * The review values, bounded against the constants that supply them rather
 * than against the formats that print them.
 *
 * The word total is the one that does not fit the two-digit assumption every
 * other composed string in this file makes: sixteen shares of a 24-word phrase
 * is 736 words, so "%d (%d per share)" reaches 18 characters where its own
 * format literal is 17. That is exactly the case sizing a buffer on
 * `sizeof(format)` gets wrong, which is why these are sized on a constant and
 * checked here instead.
 *
 * SSKR_LONGEST_SHARE_WORDCOUNT is not read from bolos_ux_sskr_share_wordcount()
 * -- that is a function, and this is a compile-time bound. It is the value that
 * function returns for the longest phrase, which
 * tests/unit/tests/sskr_share_wordcount.c pins against a generated set.
 */
#define SSKR_LONGEST_SHARE_WORDCOUNT 46
_Static_assert(SSS_MAX_SHARE_COUNT* SSKR_LONGEST_SHARE_WORDCOUNT <= 9999,
               "the word total is composed into a '%d' that REVIEW_VALUE_SIZE "
               "has to hold");
// The widest composed review value is the word total: its two "%d" are two
// characters each in the format and expand to at most four and two digits, so
// the buffer needs two characters more than the literal.
_Static_assert(sizeof(UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE) + 2 <=
                   REVIEW_VALUE_SIZE,
               "the word total value has to fit its widest composition");
_Static_assert(sizeof(UI_STR_NBGL_SSKR_REVIEW_COUNT_VALUE) <= REVIEW_VALUE_SIZE,
               "the share count and threshold are two-digit arguments, so the "
               "format literal is at least as wide as what it composes");

_Static_assert(sizeof(UI_STR_NBGL_BIP85_REVIEW_LENGTH_WORDS) <=
                       REVIEW_VALUE_SIZE &&
                   sizeof(UI_STR_NBGL_BIP85_REVIEW_LENGTH_CHARACTERS) <=
                       REVIEW_VALUE_SIZE &&
                   sizeof(UI_STR_NBGL_BIP85_REVIEW_LENGTH_DIGITS) <=
                       REVIEW_VALUE_SIZE,
               "all three length phrases have single or two-digit arguments, "
               "so each format literal is at least as wide as what it "
               "composes");

// The BIP85 index reaches seven digits (BIP85_INDEX_MAX_NUMBER_LENGTH), where
// its "%d" format is two characters -- five more than the literal.
_Static_assert(sizeof(UI_STR_NBGL_BIP85_REVIEW_INDEX_VALUE) + 5 <=
                   REVIEW_VALUE_SIZE,
               "the BIP85 index composes up to seven digits into a '%d'");

// The BIP85 result label is the longest thing HEADER_SIZE has to hold: the
// widest of the four result headers with a seven-digit index, a newline, and
// a full derivation path. Bounded against the parts rather than against the
// separator that joins them, which accounts for one character.
_Static_assert(sizeof(UI_STR_NBGL_BIP85_BASE64_HEADER) + 5 + 1 +
                       BIP85_PATH_STRING_MAX_LENGTH <=
                   HEADER_SIZE,
               "headerText has to hold a BIP85 result header, a newline and a "
               "full derivation path; a label cut short would show a path that "
               "is wrong rather than one that is missing");

// The close confirmation composes the share count into a title sized on its
// own format, which holds only while that count stays two digits.
_Static_assert(SSS_MAX_SHARE_COUNT <= 99,
               "UI_STR_NBGL_SSKR_CLOSE_CONFIRM_TITLE sizes its buffer on its "
               "format literal, which assumes a two-digit share count");

/*
 * Asking for a number, and when it is typed rather than chosen.
 *
 * A number is typed on nbgl_useCaseKeypad() when what is being asked for is a
 * value in an interval: the share count (1..16), the threshold (2..the count
 * just entered), the BIP-85 index (0..2^31-1), the password length (a
 * sixty-odd-value range that depends on the application). Four screens, and
 * they all share the same shape -- a title composed with the bound, a
 * validation callback, and an nbgl_useCaseStatus() that returns to this same
 * keypad rather than to the head of the flow, because the only thing wrong is
 * the number that was just typed.
 *
 * A number is chosen from display_choice_list() when the answer is one of a
 * handful of named values: three phrase lengths, three PIN lengths. What that
 * excludes is a keypad in front of a screen that would then refuse most of
 * what the keypad can express -- a PIN keypad accepts 5 and then rejects it,
 * and a range error for a value the screen itself offered to type is a screen
 * arguing with its own control.
 *
 * The line is the interval, not the count. Sixteen shares are typed although
 * sixteen is a small number, because 1..16 is an interval and a list of
 * sixteen bars is four pages of scrolling for something a keypad answers in
 * one gesture.
 */

// The smallest threshold this screen accepts, and the one its title announces.
// A threshold of 1 over more than one share means any single share rebuilds
// the secret; sskr_threshold_validate() refuses that, so the floor is 2 as
// soon as there is more than one share. Read by the title and by the check,
// which is what stops the screen promising a value the check would reject.
static uint8_t sskr_threshold_min(void) {
    return sskr_sharenum_get() > 1 ? 2 : 1;
}

static void sskr_sharenum_validate(const uint8_t* sharenumentry,
                                   uint8_t length) {
    // Code to validate the entered shares number

    sskr_sharenum_set(0);

    for (uint8_t i = 0; i < length; i++) {
        sskr_sharenum_set(10 * sskr_sharenum_get() + sharenumentry[i] - '0');
    }

    PRINTF("Number of shares entered is '%d'\n", sskr_sharenum_get());

    if (sskr_sharenum_get() > 0 && sskr_sharenum_get() <= SSS_MAX_SHARE_COUNT) {
        // The explanation, not the keypad, and only from here. The three
        // status pages further down return straight to
        // display_sskr_select_threshold_page(), because an explanation between
        // a refused value and a second attempt reads as a reprimand.
        display_sskr_threshold_concept_page();
    } else {
        // Back to this keypad, not to the head of the flow: the only thing
        // wrong is the number that was just typed, and it is the only thing
        // worth asking for again.
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR, false,
                           display_sskr_select_numshares_page);
    }
}

void display_sskr_select_numshares_page() {
    // The back arrow leaves for the home page rather than for the screen
    // before, which is the verdict on a phrase that has just been typed and
    // cannot be usefully drawn again. Leaving is also the only thing worth
    // doing at that point, and display_home_page() calls reset_globals(): the
    // phrase in RAM is erased in one gesture, where the "Generate SSKR
    // Phrase?" offer this used to return to took two.
    nbgl_useCaseKeypad(UI_STR_NBGL_SSKR_NUMSHARES_TITLE, 1,
                       SSKR_MAX_NUMBER_LENGTH, false, false,
                       sskr_sharenum_validate, display_home_page);
}

static void review_done(void) {
    reset_globals();
    display_home_page();
}

static void display_generic_review(void) {
    static nbgl_layoutTagValue_t pairs[1];
    static const nbgl_content_t content[1] = {
        {.type = TAG_VALUE_LIST,
         .contentActionCallback = NULL,
         .content.tagValueList.nbPairs = 1,
         .content.tagValueList.nbMaxLinesForValue = 0,
         .content.tagValueList.wrapping = true,
         .content.tagValueList.pairs = (nbgl_layoutTagValue_t*)pairs}};
    static const nbgl_genericContents_t genericContent = {
        .callbackCallNeeded = false, .contentsList = content, .nbContents = 1};

    pairs[0].item = PIC(headerText);
    pairs[0].value = PIC(reviewText);

    nbgl_useCaseGenericReview(&genericContent, UI_STR_NBGL_CLOSE, review_done);
}

static void review_sskr_shares_contentGetter(uint8_t index,
                                             nbgl_content_t* genericreview) {
    static nbgl_layoutTagValue_t pairs[1];

    genericreview->type = TAG_VALUE_LIST;
    genericreview->contentActionCallback = NULL;
    genericreview->content.tagValueList.nbPairs = 1;
    genericreview->content.tagValueList.nbMaxLinesForValue = 0;
    genericreview->content.tagValueList.wrapping = true;
    genericreview->content.tagValueList.pairs = (nbgl_layoutTagValue_t*)pairs;

    // SPRINTF() is snprintf() bounded by sizeof(headerText) (os_print.h), so
    // this is already truncated and null-terminated whatever either %d
    // expands to -- which is why the hand-written termination that used to
    // sit here, and which assumed a one- or two-digit index, is gone.
    SPRINTF(headerText, UI_STR_NBGL_SSKR_SHARE_HEADER, index + 1,
            sskr_sharecount_get());
    pairs[0].item = PIC(headerText);

    size_t offset;
    size_t length;
    if (!bolos_ux_sskr_share_slice(sskr_shares_length_get(),
                                   sskr_sharecount_get(), index, &offset,
                                   &length)) {
        // Unreachable while nbContents is the share count and that count is
        // non-zero: nbgl_useCaseGenericReview() never asks for a page past
        // it. Kept because the division below is the one this file cannot
        // recover from.
        offset = 0;
        length = 0;
    }

    strncpy(reviewText, sskr_shares_get() + offset, length);
    // Ensure null termination
    reviewText[length] = '\0';
    pairs[0].value = PIC(reviewText);
}

/*
 * Closing the shares screen destroys them, so it now says so first.
 *
 * review_done() ran reset_globals() and went home: the back arrow on the last
 * share was indistinguishable from the back arrow anywhere else, except that
 * it cost the twenty-four words needed to make the set again.
 *
 * nbgl_useCaseConfirm() and not nbgl_useCaseChoice(), which is what this first
 * used. The two look alike and behave differently in the one way that matters
 * here: Confirm is a *modal*, drawn over the screen that is already up, and
 * declining it releases the modal and redraws that screen. Choice replaces the
 * screen, so declining meant redrawing the share review by hand -- which
 * worked, but reimplemented what the component does on its own.
 *
 * That is also why this is the one confirmation in the application with no
 * reject callback and no "Back to safety": there is nothing to route a refusal
 * to. Refusing here is not a refusal to reveal, it is a refusal to leave, and
 * the shares are still on the screen underneath.
 *
 * Nothing regenerates on the way back, which matters:
 * bolos_ux_bip39_to_sskr_convert() memzeroes the mnemonic buffer it read, so a
 * second generation would split an erased phrase. The shares themselves are
 * still in sskr_shares_get(), untouched -- nothing on this path resets them.
 */
static void display_sskr_shares_review(void) {
    static nbgl_genericContents_t genericContent;
    genericContent.callbackCallNeeded = true;
    genericContent.contentGetterCallback = review_sskr_shares_contentGetter;
    genericContent.nbContents = sskr_sharecount_get();

    nbgl_useCaseGenericReview(&genericContent, UI_STR_NBGL_CLOSE,
                              display_sskr_close_confirm_page);
}

static void sskr_shares_close_confirmed(void) {
    reset_globals();
    display_home_page();
}

static void display_sskr_close_confirm_page(void) {
    // SPRINTF() is snprintf() bounded by the destination's size (os_print.h),
    // so the title is truncated and terminated whatever the count expands to.
    // The count is at most SSS_MAX_SHARE_COUNT, which the _Static_assert below
    // holds to two digits.
    SPRINTF(confirmTitle, UI_STR_NBGL_SSKR_CLOSE_CONFIRM_TITLE,
            sskr_sharecount_get());

    // The icon is the component's own: nbgl_useCaseConfirm() sets
    // IMPORTANT_CIRCLE_ICON itself and takes no icon argument.
    nbgl_useCaseConfirm(confirmTitle, UI_STR_NBGL_SSKR_CLOSE_CONFIRM_DESC,
                        UI_STR_NBGL_SSKR_CLOSE_CONFIRM_YES,
                        UI_STR_NBGL_SSKR_CLOSE_CONFIRM_NO,
                        sskr_shares_close_confirmed);
}

static void generate_and_display_sskr_shares(void) {
    sskr_shares_from_bip39_mnemonic();
    display_sskr_shares_review();
}

/*
 * The review, and the number that was never on screen before it.
 *
 * "Words to copy" is the point. A 3-of-5 over a 24-word phrase is 230 words to
 * write out by hand, and the only thing the application used to say about that
 * was the threshold keypad, two screens earlier. The per-share figure is kept
 * beside the total because it is the one that says how big each sheet is.
 *
 * The totals come from bolos_ux_sskr_share_wordcount(), which is the
 * generator's own arithmetic rather than a table -- nothing is generated yet
 * at this point, so bolos_ux_sskr_share_slice() and sskr_sharecount_get()
 * cannot answer. See src/common/sskr/common_sskr.h for why that second route
 * exists and what holds it to the first.
 *
 * Rejecting costs two gestures rather than one, and that is the SDK's doing:
 * bundleNavReviewChoice() (lib_nbgl/src/nbgl_use_case.c) answers a rejected
 * review with a hardcoded "Reject operation?" confirmation before it calls
 * back. Both gestures end here, at display_home_page() and so at
 * reset_globals().
 */
/*
 * The component both reviews are built on, and why it is this one.
 *
 * nbgl_useCaseGenericReview() walks a list of contents: the tag/value pairs
 * first, then an INFO_LONG_PRESS page carrying the alert icon, the sentence
 * about what is going to be drawn, and the button that performs the act. There
 * is no separate warning screen after it, which is what makes accepting the
 * review the whole of "accept what it offered".
 *
 * nbgl_useCaseStaticReviewLight() was used here first and this replaces it,
 * for two reasons, both measured under Speculos on Flex rather than read:
 *
 *   - it ignores the reject text it is given. It writes UNUSED(rejectText) and
 *     calls prepareNavInfo(..., getRejectReviewText(TYPE_OPERATION)), so the
 *     footer said "Reject" whatever was passed. This screen is not rejecting
 *     an operation someone else proposed -- the user is abandoning a setting
 *     they chose themselves -- and the footer now reads UI_STR_NBGL_CANCEL
 *     because the use case honours the argument.
 *
 *   - its long-press button did not require the long press. The same tap, in
 *     the same test, revealed the secret under the old component and does
 *     nothing under this one: the hold is real here. That is the whole point
 *     of the control on a page that is about to draw a secret.
 *
 * Neither nbgl_useCaseReview() nor nbgl_useCaseReviewLight() would do instead:
 * they hardcode "Hold to sign" and "Approve", and neither draws anything at
 * all in this application -- the screen underneath simply stays up, from a
 * keypad validation callback and from a plain button callback alike.
 *
 * The button's token is a user token routed through the content's own
 * contentActionCallback. Unknown tokens fall through to it, which is what lets
 * a generic review carry a working button of its own.
 */
#define REVIEW_CONFIRM_TOKEN (FIRST_USER_TOKEN + 20)

// Where display_review() leaves what accepting leads to, for the callback
// below to reach. One review is on screen at a time, which is what makes a
// single pointer enough.
//
// Only the accepting side is stored. Both reviews refuse the same way -- back
// to the home page, which is also reset_globals() -- so a per-review reject
// callback would be two identical functions and a parameter that never varies.
static nbgl_callback_t reviewOnApprove = NULL;

static void review_action_callback(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);
    if (token == REVIEW_CONFIRM_TOKEN && reviewOnApprove != NULL) {
        reviewOnApprove();
    }
}

static void display_review(const nbgl_contentTagValue_t* pairs, uint8_t nbPairs,
                           const char* warning, const char* finishButton,
                           nbgl_callback_t onApprove) {
    /*
     * Static, and it has to be: nbgl_useCaseGenericReview() keeps the pointer
     * it is handed and reads it again on every page turn, so a local would be
     * a dangling read on the first tap. Uninitialised, because BOLOS refuses a
     * non-empty .data section.
     */
    static nbgl_content_t contents[2];
    static nbgl_genericContents_t generic;

    memset(contents, 0, sizeof(contents));

    contents[0].type = TAG_VALUE_LIST;
    contents[0].content.tagValueList.pairs = pairs;
    contents[0].content.tagValueList.nbPairs = nbPairs;
    contents[0].content.tagValueList.startIndex = 0;
    contents[0].content.tagValueList.callback = NULL;
    contents[0].content.tagValueList.actionCallback = NULL;
    contents[0].content.tagValueList.hideEndOfLastLine = false;
    contents[0].content.tagValueList.nbMaxLinesForValue = 0;
    contents[0].content.tagValueList.token = 0;
    // Set for completeness only: displayGenericContextPage() forces it false
    // for every TAG_VALUE_LIST, so the values are drawn in LARGE_MEDIUM_FONT
    // whatever is asked for here.
    contents[0].content.tagValueList.smallCaseForValue = false;
    // Wraps values on spaces rather than mid-word. The BIP85 path has no space
    // in it and so cannot be wrapped at all -- it is given a pair of its own
    // for that reason, and its width is measured under Speculos rather than
    // assumed.
    contents[0].content.tagValueList.wrapping = true;

    contents[1].type = INFO_LONG_PRESS;
    contents[1].content.infoLongPress.text = warning;
    contents[1].content.infoLongPress.icon = &IMPORTANT_CIRCLE_ICON;
    contents[1].content.infoLongPress.longPressText = finishButton;
    contents[1].content.infoLongPress.longPressToken = REVIEW_CONFIRM_TOKEN;
    contents[1].content.infoLongPress.tuneId = TUNE_TAP_CASUAL;
    contents[1].contentActionCallback = &review_action_callback;

    generic.callbackCallNeeded = false;
    generic.contentsList = contents;
    generic.nbContents = 2;

    reviewOnApprove = onApprove;
    nbgl_useCaseGenericReview(&generic, UI_STR_NBGL_CANCEL, &display_home_page);
}

static void display_sskr_generate_review_page(void) {
    static nbgl_contentTagValue_t pairs[4];

    const uint8_t words_per_share =
        bolos_ux_sskr_share_wordcount(bip39_mnemonic_final_size_get());

    // SPRINTF() is snprintf() bounded by the destination (os_print.h), and the
    // _Static_asserts above bound every argument, so each of these is
    // terminated whatever its numbers expand to.
    SPRINTF(reviewValueShares, UI_STR_NBGL_SSKR_REVIEW_COUNT_VALUE,
            sskr_sharenum_get());
    SPRINTF(reviewValueThreshold, UI_STR_NBGL_SSKR_REVIEW_COUNT_VALUE,
            sskr_threshold_get());
    // A single share is a legitimate scheme -- sskr_threshold_min() allows
    // 1-of-1 -- and there the total and the per-share figure are the same
    // number, so the parenthesis would repeat it.
    if (sskr_sharenum_get() > 1) {
        SPRINTF(reviewValueWords, UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE,
                words_per_share * sskr_sharenum_get(), words_per_share);
    } else {
        SPRINTF(reviewValueWords, UI_STR_NBGL_SSKR_REVIEW_WORDS_VALUE_SINGLE,
                words_per_share * sskr_sharenum_get());
    }

    // Format first, as the BIP85 review leads with the application: what this
    // is, then the parameters that shape it, then what it will cost to copy.
    pairs[0].item = UI_STR_NBGL_SSKR_REVIEW_ITEM_FORMAT;
    pairs[0].value = UI_STR_NBGL_SSKR_REVIEW_FORMAT_VALUE;
    pairs[1].item = UI_STR_NBGL_SSKR_REVIEW_ITEM_SHARES;
    pairs[1].value = PIC(reviewValueShares);
    pairs[2].item = UI_STR_NBGL_SSKR_REVIEW_ITEM_THRESHOLD;
    pairs[2].value = PIC(reviewValueThreshold);
    pairs[3].item = UI_STR_NBGL_SSKR_REVIEW_ITEM_WORDS;
    pairs[3].value = PIC(reviewValueWords);

    display_review(pairs, 4, UI_STR_NBGL_SSKR_REVEAL_WARN,
                   UI_STR_NBGL_SSKR_REVIEW_FINISH,
                   &generate_and_display_sskr_shares);
}

static void sskr_threshold_validate(const uint8_t* thresholdentry,
                                    uint8_t length) {
    // Code to validate the entered threshold number

    sskr_threshold_set(0);

    for (uint8_t i = 0; i < length; i++) {
        sskr_threshold_set(10 * sskr_threshold_get() + thresholdentry[i] - '0');
    }

    PRINTF("Threshold value entered is '%d'\n", sskr_threshold_get());

    // All three send the user back to this same keypad rather than to the
    // head of the flow. The share count entered on the previous screen is
    // valid, was accepted, and is not what is being corrected -- returning to
    // the head threw it away and made it be typed again before the threshold
    // could be fixed. Nothing on this path resets it: reset_globals() and
    // sskr_shares_reset() are reached only from display_home_page(),
    // review_done() and the check flow, none of which any of these three
    // branches goes through.
    if (sskr_threshold_get() < 1) {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_ZERO_ERROR, false,
                           display_sskr_select_threshold_page);
    } else if (sskr_threshold_get() > sskr_sharenum_get()) {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_RANGE_ERROR, false,
                           display_sskr_select_threshold_page);
    } else if (sskr_threshold_get() < sskr_threshold_min()) {
        // Below the floor is 1-of-m, and only that: the branch above has
        // already taken everything under 1, and sskr_threshold_min() is 1
        // whenever a threshold of 1 is legitimate. Written against the floor
        // rather than against `== 1 && sharenum > 1` so that the value the
        // title announces and the value this rejects are the same call.
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR, false,
                           display_sskr_select_threshold_page);
    } else {
        // The threshold being accepted no longer generates anything. It leads
        // to the review, which is the first screen that says how much there is
        // to write down, and generation happens only after that review has
        // been approved and the warning after it accepted.
        display_sskr_generate_review_page();
    }
}

void display_sskr_select_threshold_page() {
    // Neither bound is a constant, so the title is composed here from the two
    // calls sskr_threshold_validate() makes above. The upper bound is the
    // share count entered on the previous screen; the lower one is 2 as soon
    // as there is more than one share, because a threshold of 1 over several
    // shares is 1-of-m and is refused -- announcing (1 - N) there would have
    // promised a value the next screen rejects.
    //
    // snprintf()'s return value is not read: the buffer is sized on the
    // format itself and the _Static_asserts above bound every argument to two
    // digits, so there is nothing for a check to find. (It would find it if
    // there were: on the three SDKs that compile this file snprintf() returns
    // the length it would have needed. The nanos SDK returns 0 from every exit
    // and documents it, but never compiles this file.)
    snprintf(keypadTitle, sizeof(keypadTitle), UI_STR_NBGL_SSKR_THRESHOLD_TITLE,
             sskr_threshold_min(), sskr_sharenum_get());

    // Draw the keypad
    nbgl_useCaseKeypad(keypadTitle, 1, SSKR_MAX_NUMBER_LENGTH, false, false,
                       sskr_threshold_validate,
                       display_sskr_select_numshares_page);
}

/*
 * The four secrets this flow derives, in the order of `enum bip85_app_type`.
 *
 * That order is not a presentation choice: the review reads
 * bip85_select_app[bip85_type_get()] to say back which one was chosen, so a
 * table sorted for the screen would make it name a different application than
 * the one being derived.
 *
 * The DICE entry is labelled "PIN", which is a use and not the primitive, and
 * that is why the review does not read this table for it: a second use of
 * DICE -- generic rolls, any number of sides -- would be a fifth entry
 * sharing the fourth's application type, and one entry cannot name two.
 * bip85_review_app_name() reads bip85_dice_use_get() instead.
 *
 * `const char* const`, rather than an array of mutable pointers, because
 * BARS_LIST below takes exactly that: the SDK reads the array out of flash
 * and translates each entry itself (nbgl_use_case.c PIC()s barTexts).
 */
static const char* const bip85_select_app[] = {
    UI_STR_NBGL_BIP85_APP_BIP39, UI_STR_NBGL_BIP85_APP_PWD_BASE64,
    UI_STR_NBGL_BIP85_APP_PWD_BASE85, UI_STR_NBGL_BIP85_APP_PIN};

/*
 * One token per entry, rather than one shared token read with the row index.
 *
 * The index the SDK reports is a position on the page it drew, and this list
 * is one entry away from paginating -- five bars is what fits, and a sixth
 * secret would put the last of them on a page of its own with an index of 0.
 * A token says which secret was touched whatever page it was drawn on.
 */
enum __attribute__((packed)) select_bip85_app_token {
    SELECT_BIP85_APP_BIP39_TOKEN = FIRST_USER_TOKEN,
    SELECT_BIP85_APP_PWD_BASE64_TOKEN,
    SELECT_BIP85_APP_PWD_BASE85_TOKEN,
    SELECT_BIP85_APP_PIN_TOKEN,
};

static const uint8_t bip85_select_app_tokens[] = {
    SELECT_BIP85_APP_BIP39_TOKEN, SELECT_BIP85_APP_PWD_BASE64_TOKEN,
    SELECT_BIP85_APP_PWD_BASE85_TOKEN, SELECT_BIP85_APP_PIN_TOKEN};

_Static_assert(ARRAYLEN(bip85_select_app_tokens) == ARRAYLEN(bip85_select_app),
               "every entry of the BIP85 list needs the token that says which "
               "secret it is; the SDK reads the two arrays in step");

/*
 * Deriving, and then showing what was derived.
 *
 * This is what used to sit inline in bip85_index_validate(), on three branches
 * that each generated a secret and drew it in the same breath. It is a
 * function of its own now for the same reason the SSKR path grew one: the
 * derivation has to happen after the review and the warning, not on the way to
 * them, and something has to be callable from the far side of two callbacks.
 *
 * The switch has no default and covers the enumeration, so a fourth
 * application is a -Wswitch diagnostic here rather than a screen that draws an
 * empty review. The old default branch, which sent the user back to the
 * application list, is gone with it: it could only be reached by a
 * bip85_type_get() no button sets.
 */
static void bip85_generate_and_display(void) {
    switch ((enum bip85_app_type)bip85_type_get()) {
        case BIP85_APP_BIP39:
            bip85_app_bip39_gen();
            SPRINTF(headerText, UI_STR_NBGL_BIP85_BIP39_HEADER,
                    bip85_index_get());
            strncpy(reviewText, bip39_mnemonic_get(),
                    bip39_mnemonic_length_get());
            // Ensure null termination
            reviewText[bip39_mnemonic_length_get()] = '\0';
            break;
        case BIP85_APP_PWD_BASE64:
            SPRINTF(headerText, UI_STR_NBGL_BIP85_BASE64_HEADER,
                    bip85_index_get());
            strncpy(reviewText, (const char*)bip85_app_pwd_base64_gen(),
                    bip85_length_get());
            // Ensure null termination
            reviewText[bip85_length_get()] = '\0';
            break;
        case BIP85_APP_PWD_BASE85:
            SPRINTF(headerText, UI_STR_NBGL_BIP85_BASE85_HEADER,
                    bip85_index_get());
            strncpy(reviewText, (const char*)bip85_app_pwd_base85_gen(),
                    bip85_length_get());
            // Ensure null termination
            reviewText[bip85_length_get()] = '\0';
            break;
        case BIP85_APP_DICE: {
            /*
             * The only derivation here that can come back with nothing, and
             * the only one whose failure would be invisible on screen: a PIN
             * short of a digit still looks like a PIN. bip85_app_pin_gen()
             * checks the rolls it got against the rolls that were asked for
             * and erases everything rather than returning what it has, so
             * there is nothing to draw and nothing partial to inspect.
             *
             * Going home rather than back to a parameter screen, because
             * display_home_page() is what calls reset_globals(): the index
             * and the length that produced this are cleared with the rest.
             */
            const char* pin = bip85_app_pin_gen();
            if (pin == NULL) {
                memzero(headerText, sizeof(headerText));
                memzero(reviewText, sizeof(reviewText));
                nbgl_useCaseStatus(UI_STR_NBGL_BIP85_PIN_DERIVE_ERROR, false,
                                   display_home_page);
                return;
            }
            SPRINTF(headerText, UI_STR_NBGL_BIP85_PIN_HEADER,
                    bip85_index_get());
            strncpy(reviewText, pin, bip85_length_get());
            // Ensure null termination
            reviewText[bip85_length_get()] = '\0';
            break;
        }
    }

    /*
     * The path goes into the label, not into a row of its own.
     *
     * The review before this screen showed it too, and that is deliberate --
     * one is where the user decides, this is where the user copies. But the
     * first attempt put it here as a second tag/value pair and measurement
     * killed that: a 24-word phrase is long enough that the two pairs
     * paginated onto separate pages, so the path was one swipe away from the
     * words it belongs to. A pair is never split across pages, so folding the
     * path into the item of the pair that carries the secret is what
     * guarantees the two are read, and copied, together.
     *
     * Composed from headerText rather than into it: SPRINTF() cannot take its
     * own destination as an argument.
     */
    resultLabel[0] = '\0';
    if (append_bounded(resultLabel, sizeof(resultLabel), headerText) &&
        append_bounded(resultLabel, sizeof(resultLabel),
                       UI_STR_NBGL_BIP85_RESULT_LABEL_SEPARATOR) &&
        append_bounded(resultLabel, sizeof(resultLabel), reviewValuePath)) {
        strncpy(headerText, resultLabel, sizeof(headerText) - 1);
        headerText[sizeof(headerText) - 1] = '\0';
    }

    display_generic_review();
}

/*
 * What the review calls the application, which for DICE is the use rather
 * than the primitive.
 *
 * bip85_select_app[] is indexed by application type and its DICE entry is the
 * button that chose it, labelled "PIN". Reading the use here rather than that
 * entry is what keeps this row right when a second use of DICE is added: two
 * buttons would then share one application type, and the table could only
 * name one of them.
 *
 * PIC() on the table, and it is not decoration. bip85_select_app[] is a table
 * of pointers to literals, so each entry is a link-time address that has to
 * be translated before the string is read. Handing an untranslated one to a
 * tag/value pair takes the application down -- measured under Speculos on
 * Flex, where this without PIC() killed it on the way into the review, and
 * with it the review draws.
 *
 * The same pointers are assigned straight to button labels on the selection
 * screen without PIC(), which is why this looked safe: that path survives
 * because of what the button drawing does with them, not because the pointers
 * are usable as they stand.
 */
static const char* bip85_review_app_name(void) {
    if ((enum bip85_app_type)bip85_type_get() == BIP85_APP_DICE) {
        switch ((enum bip85_dice_use)bip85_dice_use_get()) {
            case BIP85_DICE_USE_PIN:
                return UI_STR_NBGL_BIP85_APP_PIN;
            case BIP85_DICE_USE_NB:
                break;
        }
    }
    return (const char*)PIC(bip85_select_app[bip85_type_get()]);
}

// The third arm of the same distinction the two switches in the review make. A
// BIP39 derivation is twenty-four words that would restore *a* wallet, so its
// warning says which one it is not; a password cannot be mistaken for a
// recovery phrase, so saying it is not one would be a sentence about nothing.
// See UI_STR_NBGL_BIP85_REVEAL_WARN_BIP39 in ui_strings.h.
//
// A switch rather than the pair of ifs this was: the warning is the last thing
// between the user and the secret, and a fifth application must not inherit
// whichever sentence happened to be the fallback. That is why there is no
// `default:` -- adding one would silence -Wswitch, which is exactly the
// compiler error a fifth application has to hit.
//
// One return per case rather than a variable assigned in each: assigning
// leaves the initialiser dead on every path, which the Clang static analyzer
// reports as deadcode.DeadStores. The trailing return is not that initialiser
// moved -- it is reachable only if the stored type is outside the enum, and
// returns the sentence that claims the least.
static const char* bip85_reveal_warning(void) {
    switch ((enum bip85_app_type)bip85_type_get()) {
        case BIP85_APP_BIP39:
            return UI_STR_NBGL_BIP85_REVEAL_WARN_BIP39;
        case BIP85_APP_DICE:
            return UI_STR_NBGL_BIP85_REVEAL_WARN_PIN;
        case BIP85_APP_PWD_BASE64:
        case BIP85_APP_PWD_BASE85:
            return UI_STR_NBGL_BIP85_REVEAL_WARN_PWD;
    }
    return UI_STR_NBGL_BIP85_REVEAL_WARN_PWD;
}

/*
 * The review, and the path.
 *
 * The three parameters of a BIP-85 derivation are collected on three separate
 * screens and were never shown together anywhere; the path they combine into
 * was never shown at all. That is what made a derived password unusable as
 * anything but a one-off: reproducing it elsewhere -- which is the only reason
 * to derive rather than store -- needs `m/83696968'/707764'/21'/0'`, and the
 * only way to recover that from this application was to remember which three
 * screens had been tapped.
 *
 * The path is not assembled here. bip85_app_path_format() calls the formatter
 * that lives beside the derivation it pairs with, with the arguments that
 * derivation is called with, so what is drawn is what will be walked. When it
 * cannot render one -- which for these three applications means only that the
 * buffer was too small -- nothing is shown rather than something plausible,
 * and the review is not opened at all.
 */
static void display_bip85_generate_review_page(void) {
    static nbgl_contentTagValue_t pairs[5];

    /*
     * Before the derivation, and that ordering is load-bearing.
     *
     * app_data.length is both what the user asked for and where the derivation
     * puts what it produced -- bip85_app_generate() assigns its own return
     * value back into it. The path is built from the *requested* length, so
     * formatting it after the derivation would print whatever the derivation
     * returned instead. It is called once, here, and the result screen reuses
     * reviewValuePath rather than rebuilding it, which is what keeps the two
     * screens showing the same path.
     */
    if (!bip85_app_path_format(reviewValuePath, sizeof(reviewValuePath))) {
        // Unreachable from the screens: every button sets one of the three
        // applications, and BIP85_PATH_STRING_MAX_LENGTH covers the widest
        // path any of them can build. Kept because the alternative to
        // returning here is drawing a review whose path field is empty, which
        // is the one thing this screen must never do -- an absent path reads
        // as "no path needed" rather than as a failure.
        nbgl_useCaseStatus(UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR, false,
                           display_bip85_select_index_page);
        return;
    }

    SPRINTF(reviewValueIndex, UI_STR_NBGL_BIP85_REVIEW_INDEX_VALUE,
            bip85_index_get());

    // The two kinds of length this flow has. A BIP39 derivation is measured in
    // words and takes its size from the length screen the flow shares with the
    // check flow; a password is measured in characters and takes it from its
    // own keypad. Naming the unit is what stops "24" and "24" meaning two
    // different things on the same row.
    switch ((enum bip85_app_type)bip85_type_get()) {
        case BIP85_APP_BIP39:
            SPRINTF(reviewValueLength, UI_STR_NBGL_BIP85_REVIEW_LENGTH_WORDS,
                    bip39_mnemonic_final_size_get());
            break;
        case BIP85_APP_PWD_BASE64:
        case BIP85_APP_PWD_BASE85:
            SPRINTF(reviewValueLength,
                    UI_STR_NBGL_BIP85_REVIEW_LENGTH_CHARACTERS,
                    bip85_length_get());
            break;
        case BIP85_APP_DICE:
            // From the roll count, which is where this flow put it, and not
            // from bip85_length_get(): that one is the app buffer's length
            // and is written by the derivation itself, so reading it here --
            // before anything has been derived -- would show whatever the
            // previous journey left behind.
            SPRINTF(reviewValueLength, UI_STR_NBGL_BIP85_REVIEW_LENGTH_DIGITS,
                    bip85_dice_rolls_get());
            break;
    }

    pairs[0].item = UI_STR_NBGL_BIP85_REVIEW_ITEM_APP;
    // The language belongs to the BIP39 application alone -- it is the `0'`
    // after `39'` in the path, and the other three have no such component --
    // so it is appended here rather than carried by the name itself.
    if (bip85_type_get() == BIP85_APP_BIP39) {
        reviewValueApp[0] = '\0';
        if (append_bounded(reviewValueApp, sizeof(reviewValueApp),
                           bip85_review_app_name()) &&
            append_bounded(reviewValueApp, sizeof(reviewValueApp),
                           UI_STR_NBGL_BIP85_REVIEW_APP_LANGUAGE)) {
            pairs[0].value = PIC(reviewValueApp);
        } else {
            pairs[0].value = bip85_review_app_name();
        }
    } else {
        pairs[0].value = bip85_review_app_name();
    }
    pairs[1].item = UI_STR_NBGL_BIP85_REVIEW_ITEM_LENGTH;
    pairs[1].value = PIC(reviewValueLength);
    pairs[2].item = UI_STR_NBGL_BIP85_REVIEW_ITEM_INDEX;
    pairs[2].value = PIC(reviewValueIndex);
    pairs[3].item = UI_STR_NBGL_BIP85_REVIEW_ITEM_PATH;
    pairs[3].value = PIC(reviewValuePath);

    display_review(pairs, 4, bip85_reveal_warning(),
                   UI_STR_NBGL_BIP85_REVIEW_FINISH,
                   &bip85_generate_and_display);
}

static void bip85_index_validate(const uint8_t* indexentry, uint8_t length) {
    // Code to validate BIP85 index

    bip85_index_set(0);

    for (uint8_t i = 0; i < length; i++) {
        bip85_index_set(10 * bip85_index_get() + indexentry[i] - '0');
    }
    PRINTF("BIP85 index entered is '%d'\n", bip85_index_get());

    // The bound below and the message on the else branch are two different
    // numbers on purpose, so neither is a typo for the other.
    //
    // `UINT32_MAX >> 1` is what the derivation needs: the index goes into a
    // hardened BIP32 path component as `0x80000000 | index`, so anything that
    // does not fit in 31 bits would run into the hardening bit.
    //
    // The keypad above (`display_bip85_select_index_page()`) caps entry at
    // `BIP85_INDEX_MAX_NUMBER_LENGTH` digits, so the largest value that can
    // reach this function is 9,999,999 -- well inside 31 bits, and far too
    // small for the `10 * index + digit` accumulation to overflow the
    // `uint32_t` it runs in. The else branch is therefore unreachable from
    // the screens, and the check is kept as a guard on the derivation
    // precondition rather than on the entry.
    //
    // The message states the keypad's limit rather than the 31-bit one
    // because it is addressed to whoever is typing, and 9,999,999 is what
    // they can actually enter.
    if (bip85_index_get() <= (UINT32_MAX >> 1)) {
        // The index being accepted no longer derives anything. All three
        // applications now go to the same review, which is the only screen in
        // this flow that shows the derivation path -- and the path is what
        // makes the result reproducible anywhere else, which is the whole
        // reason to derive a secret rather than store one.
        display_bip85_generate_review_page();
    } else {
        // Back to the index keypad. The screen this used to return to,
        // display_bip39_select_phrase_length_page(), is neither the field at
        // fault nor even on the password branches of this flow. As the comment
        // above says, this branch is unreachable from the keypad; the callback
        // is corrected so that the guard, if it ever does fire, lands where
        // the other five now do.
        nbgl_useCaseStatus(UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR, false,
                           display_bip85_select_index_page);
    }
}

void display_bip85_select_index_page() {
    // Draw the keypad
    nbgl_useCaseKeypad(UI_STR_NBGL_BIP85_INDEX_TITLE, 1,
                       BIP85_INDEX_MAX_NUMBER_LENGTH, false, false,
                       bip85_index_validate, display_bip85_select_app_page);
}

// The password lengths this screen accepts, per application. Same values as
// bip85_pwd_base64_len_valid() and bip85_pwd_base85_len_valid()
// (src/common/bip85/seed_bip85.c), which guard the derivation itself; these
// are the screen's own copy, named here so that the title announcing them, the
// branch applying them and the error message quoting them all read one pair of
// numbers rather than three sets of literals.
#define BIP85_PWD_BASE64_LENGTH_MIN 20
#define BIP85_PWD_BASE64_LENGTH_MAX (BASE64_ENCODE_LENGTH - 2)
#define BIP85_PWD_BASE85_LENGTH_MIN 10
#define BIP85_PWD_BASE85_LENGTH_MAX BASE85_ENCODE_LENGTH

// Same two-digit assumption the keypad title buffer is sized on. All four,
// not just the maxima: the title composes the minimum through a "%d" of the
// same width, and a three-digit minimum would truncate the title silently
// while leaving an assertion on the maxima alone perfectly happy.
_Static_assert(BIP85_PWD_BASE64_LENGTH_MIN <= 99 &&
                   BIP85_PWD_BASE64_LENGTH_MAX <= 99 &&
                   BIP85_PWD_BASE85_LENGTH_MIN <= 99 &&
                   BIP85_PWD_BASE85_LENGTH_MAX <= 99,
               "the composed password-length title assumes two-digit bounds");

static void bip85_password_length_bounds(uint8_t* min, uint8_t* max) {
    // Base85 is the else rather than a third case because this screen is only
    // ever reached from the two password buttons or from its own refusal --
    // bip85_type_get() is BIP85_APP_PWD_BASE64 or BIP85_APP_PWD_BASE85 here,
    // never BIP85_APP_BIP39, which has no length to choose and no keypad.
    if (bip85_type_get() == BIP85_APP_PWD_BASE64) {
        *min = BIP85_PWD_BASE64_LENGTH_MIN;
        *max = BIP85_PWD_BASE64_LENGTH_MAX;
    } else {
        *min = BIP85_PWD_BASE85_LENGTH_MIN;
        *max = BIP85_PWD_BASE85_LENGTH_MAX;
    }
}

static void bip85_password_length_validate(const uint8_t* lengthentry,
                                           uint8_t length) {
    // Code to validate BIP85 index

    bip85_length_set(0);

    for (uint8_t i = 0; i < length; i++) {
        bip85_length_set(10 * bip85_length_get() + lengthentry[i] - '0');
    }
    PRINTF("BIP85 password length entered is '%d'\n", bip85_length_get());

    uint8_t password_length_min;
    uint8_t password_length_max;
    bip85_password_length_bounds(&password_length_min, &password_length_max);

    // static for the same reason keypadTitle is, and it is the same mistake:
    // nbgl_useCaseStatus() keeps the pointer rather than the text
    // (`info.centeredInfo.text1 = message`, lib_nbgl/src/nbgl_use_case.c),
    // nbgl_layoutAddCenteredInfo() stores it in the text area, and the status
    // page stays up for three seconds after this function has returned. The
    // first draw is synchronous and reads a live frame; any redraw after that
    // -- nbgl_screenRedraw() walks the whole object tree, and the UX layer
    // calls it on a system redisplay -- would read a stack frame the event
    // loop has since reused. This is the only status message in the file that
    // is composed rather than a literal, so it is the only one that had the
    // problem.
    static char message[50] = {0};

    if ((bip85_length_get() >= password_length_min) &&
        (bip85_length_get() <= password_length_max)) {
        // The explanation, as the BIP39 branch does. The index means the same
        // thing for all four applications, so all four have to meet it: this
        // is one of three routes into that keypad from a value the user has
        // just accepted, and it was the one that skipped the screen.
        display_bip85_index_concept_page();
    } else {
        snprintf(message, sizeof(message),
                 UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR, password_length_min,
                 password_length_max);
        // Back to this keypad rather than to the application menu: the chosen
        // application is what decides the bounds, it was not the mistake, and
        // re-choosing it was the price of correcting a length.
        nbgl_useCaseStatus((const char*)message, false,
                           display_bip85_select_password_length_page);
    }
}

void display_bip85_select_password_length_page() {
    // Both bounds depend on the application chosen on the previous screen, so
    // the title is composed here from the same helper the validator above
    // reads. Return value unused, as on the threshold title.
    uint8_t password_length_min;
    uint8_t password_length_max;
    bip85_password_length_bounds(&password_length_min, &password_length_max);
    snprintf(keypadTitle, sizeof(keypadTitle),
             UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE, password_length_min,
             password_length_max);

    // Draw the keypad
    nbgl_useCaseKeypad(keypadTitle, 1, 2, false, false,
                       bip85_password_length_validate,
                       display_bip85_select_app_page);
}

/*
 * How many digits, asked as a list of three rather than with a keypad.
 *
 * The password length is a number in a range of sixty-odd values and gets the
 * keypad it needs; a PIN is one of three lengths, and a keypad for it would
 * put a number pad in front of someone about to be shown a number, with a
 * range error waiting behind every other value it accepts. Same shape as the
 * phrase-length screen, for the same reason.
 *
 * No icon, where this screen carried BIP85_ICON, and no emphasised entry,
 * where the longest PIN was drawn black. Both go for the reasons written
 * above display_choice_list(): the icons this repository has name formats and
 * this screen asks a quantity, and a black control in this application means
 * an act with a consequence. What the black entry said -- "8 is the safest of
 * the three" -- a bar cannot say, because a bar is one line of text; it is
 * not said elsewhere either, and that is a loss this rule pays for rather
 * than a detail that was overlooked.
 *
 * These are the three values the derivation is asked for, so they are named
 * here and bounded against the preset rather than typed as literals beside
 * the labels: bip85_app_pin_gen() refuses anything outside
 * [BIP85_DICE_PIN_DIGITS_MIN, BIP85_DICE_PIN_DIGITS_MAX] before it derives,
 * and a button that could reach it would be a screen offering a length the
 * next step rejects.
 */
#define BIP85_PIN_DIGITS_SHORT BIP85_DICE_PIN_DIGITS_MIN
#define BIP85_PIN_DIGITS_MEDIUM 6
#define BIP85_PIN_DIGITS_LONG BIP85_DICE_PIN_DIGITS_MAX

_Static_assert(BIP85_PIN_DIGITS_SHORT >= BIP85_DICE_PIN_DIGITS_MIN &&
                   BIP85_PIN_DIGITS_MEDIUM > BIP85_PIN_DIGITS_SHORT &&
                   BIP85_PIN_DIGITS_LONG > BIP85_PIN_DIGITS_MEDIUM &&
                   BIP85_PIN_DIGITS_LONG <= BIP85_DICE_PIN_DIGITS_MAX,
               "every length this screen offers has to be one the PIN "
               "derivation accepts, in the order the buttons are drawn");

enum __attribute__((packed)) select_bip85_pin_length_token {
    SELECT_BIP85_PIN_LENGTH_SHORT_TOKEN = FIRST_USER_TOKEN,
    SELECT_BIP85_PIN_LENGTH_MEDIUM_TOKEN,
    SELECT_BIP85_PIN_LENGTH_LONG_TOKEN,
};

// Ascending, which is the order the list draws them in, and the order the
// three constants above are declared in.
static const char* const bip85_pin_length_entries[] = {
    UI_STR_NBGL_BIP85_PIN_DIGITS_4, UI_STR_NBGL_BIP85_PIN_DIGITS_6,
    UI_STR_NBGL_BIP85_PIN_DIGITS_8};

static const uint8_t bip85_pin_length_tokens[] = {
    SELECT_BIP85_PIN_LENGTH_SHORT_TOKEN, SELECT_BIP85_PIN_LENGTH_MEDIUM_TOKEN,
    SELECT_BIP85_PIN_LENGTH_LONG_TOKEN};

_Static_assert(ARRAYLEN(bip85_pin_length_tokens) ==
                   ARRAYLEN(bip85_pin_length_entries),
               "every length this screen offers needs the token that says "
               "which one it is; the SDK reads the two arrays in step");

static void select_bip85_pin_length_action(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);

    // No default, as on every screen of this family: a fourth length has to
    // say here how many rolls it asks for.
    switch ((enum select_bip85_pin_length_token)token) {
        case SELECT_BIP85_PIN_LENGTH_SHORT_TOKEN:
            bip85_dice_rolls_set(BIP85_PIN_DIGITS_SHORT);
            break;
        case SELECT_BIP85_PIN_LENGTH_MEDIUM_TOKEN:
            bip85_dice_rolls_set(BIP85_PIN_DIGITS_MEDIUM);
            break;
        case SELECT_BIP85_PIN_LENGTH_LONG_TOKEN:
            bip85_dice_rolls_set(BIP85_PIN_DIGITS_LONG);
            break;
    }
    // The explanation of what an index is, as both other branches of this
    // flow reach it: the index means the same thing for all four
    // applications, so all four meet the same screen.
    display_bip85_index_concept_page();
}

static void display_bip85_select_pin_length_page(void) {
    display_choice_list(
        UI_STR_NBGL_BIP85_PIN_LENGTH_TITLE, bip85_pin_length_entries,
        bip85_pin_length_tokens, ARRAYLEN(bip85_pin_length_entries),
        &select_bip85_pin_length_action, &display_bip85_select_app_page);
}

/*
 * Which secret to derive, as a list.
 *
 * The first screen of this family to be drawn as one, and the reason the
 * other three followed: it was four hand-built buttons stacked from the
 * bottom, and the fourth grew up into the title -- Flex drew "PIN" across the
 * second line of the question. The rule that came out of it, and everything
 * it excludes, is written above display_choice_list().
 *
 * The list reads top-down in the order of the table above. Nothing in the
 * application depended on the bottom-up order the buttons had; the functional
 * tests did, and they now count from the top as the screen does.
 */
static void bip85_select_app_action(int token, uint8_t index, int page) {
    UNUSED(index);
    UNUSED(page);

    // No default: the tokens are this screen's own enumeration, and a fifth
    // secret has to say here where it goes rather than falling through to
    // whichever branch was last.
    switch ((enum select_bip85_app_token)token) {
        case SELECT_BIP85_APP_BIP39_TOKEN:
            bip85_type_set(BIP85_APP_BIP39);
            display_bip39_select_phrase_length_page();
            break;
        case SELECT_BIP85_APP_PWD_BASE64_TOKEN:
            bip85_type_set(BIP85_APP_PWD_BASE64);
            display_bip85_select_password_length_page();
            break;
        case SELECT_BIP85_APP_PWD_BASE85_TOKEN:
            bip85_type_set(BIP85_APP_PWD_BASE85);
            display_bip85_select_password_length_page();
            break;
        case SELECT_BIP85_APP_PIN_TOKEN:
            // The application is DICE, which is what BIP-85 defines and what
            // the path on the review will name; the PIN is the use it is
            // being put to. Both are set here, on the entry that chose them,
            // rather than being inferred later from a parameter.
            bip85_type_set(BIP85_APP_DICE);
            bip85_dice_use_set(BIP85_DICE_USE_PIN);
            display_bip85_select_pin_length_page();
            break;
    }
}

static void display_bip85_select_app_page(void) {
    // Back goes to the menu, which is where this screen is reached from --
    // the same destination the hand-built back arrow had.
    display_choice_list(UI_STR_NBGL_BIP85_SELECT_APP_TITLE, bip85_select_app,
                        bip85_select_app_tokens, ARRAYLEN(bip85_select_app),
                        &bip85_select_app_action, &display_select_menu_page);
}

/*
 * Public function
 */
void ui_idle_init(void) { display_home_page(); }
#endif
