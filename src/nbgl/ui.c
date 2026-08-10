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
#include <nbgl_page.h>
#include <nbgl_use_case.h>

#include "../common/bip39/common_bip39.h"
#include "../common/bip85/common_bip85.h"
#include "../common/sskr/common_sskr.h"
#include "../common/ui_strings.h"
#include "../ui.h"
#include "./bip39_mnemonic.h"
#include "./bip85_app.h"
#include "./layout_generic_screen.h"
#include "./sskr_shares.h"

#define HEADER_SIZE 50

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
static void display_bip39_select_phrase_length_page(void);
static void display_generic_review(void);
static void display_sskr_select_numshares_page(void);
static void display_sskr_select_threshold_page(void);
static void display_bip85_select_app_page(void);
static void display_bip85_select_index_page(void);
static void display_bip85_select_password_length_page(void);

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
}

static void on_quit(void) { os_sched_exit(-1); }

/*
 * The menu: one entry per intention
 */
enum __attribute__((packed)) select_menu {
    SELECT_MENU_TEXT_INDEX = 0,
    /*
     * generic_screen_configure_buttons() stacks its buttons upwards from the
     * bottom of the screen, so the first button child is the *last* line the
     * user reads. These four are declared in that order -- bottom entry
     * first -- and named for the intention they carry rather than for where
     * they sit, which is how the reading order stays Check, Generate,
     * Recover, Derive from the top without any of the code below depending
     * on knowing that.
     */
    SELECT_MENU_DERIVE_INDEX,
    SELECT_MENU_RECOVER_INDEX,
    SELECT_MENU_BACKUP_INDEX,
    SELECT_MENU_CHECK_INDEX,
    SELECT_MENU_BACK_BUTTON_INDEX,
    SELECT_MENU_NB_CHILDREN
};

// One button per intention. Asserted against the enumeration rather than
// counted by hand, because this number is what nbgl_objPoolGetArray() below
// writes into consecutive children: a value smaller than the run of button
// indices leaves an unallocated child, a larger one runs into the back
// button.
#define SELECT_MENU_NB_BUTTONS 4
_Static_assert(SELECT_MENU_BACK_BUTTON_INDEX - SELECT_MENU_DERIVE_INDEX ==
                   SELECT_MENU_NB_BUTTONS,
               "the menu allocates SELECT_MENU_NB_BUTTONS buttons into the "
               "children between SELECT_MENU_DERIVE_INDEX and the back "
               "button; adding an entry means changing both");
_Static_assert(USER_INTENT_NB == SELECT_MENU_NB_BUTTONS,
               "the menu shows one entry per intention");

static void select_menu_callback(nbgl_obj_t* obj, nbgl_touchType_t eventType) {
    nbgl_obj_t** screenChildren = nbgl_screenGetElements(0);
    if (eventType != TOUCHED) {
        return;
    }
    io_seproxyhal_play_tune(TUNE_TAP_CASUAL);
    nbgl_layoutRelease(layout);
    // Each entry sets both: the intention, and the kind of data the screens
    // it leads to will be handed. Checking a phrase and backing one up are
    // the same tool and different intentions, which is the whole reason the
    // two are separate variables.
    if (obj == screenChildren[SELECT_MENU_CHECK_INDEX]) {
        user_intent = USER_INTENT_CHECK;
        tool_type = TOOL_TYPE_BIP39;
        display_bip39_select_phrase_length_page();
    } else if (obj == screenChildren[SELECT_MENU_BACKUP_INDEX]) {
        user_intent = USER_INTENT_BACKUP;
        tool_type = TOOL_TYPE_BIP39;
        display_backup_explain_page();
    } else if (obj == screenChildren[SELECT_MENU_RECOVER_INDEX]) {
        user_intent = USER_INTENT_RECOVER;
        tool_type = TOOL_TYPE_SSKR;
        display_check_keyboard_page();
    } else if (obj == screenChildren[SELECT_MENU_DERIVE_INDEX]) {
        user_intent = USER_INTENT_DERIVE;
        tool_type = TOOL_TYPE_BIP85;
        display_bip85_select_app_page();
    } else if (obj == screenChildren[SELECT_MENU_BACK_BUTTON_INDEX]) {
        display_home_page();
        return;
    }
}

static void display_select_menu_page(void) {
    nbgl_obj_t** screenChildren;

    // From top to bottom:
    // <return back arrow> + <text> + <4 buttons>
    //
    // No icon, where the three-entry menu had the application's. Two reasons,
    // both checked rather than preferred:
    //
    //   - there is no room. On apex_p a button is 56px on a 400px screen and
    //     the stack starts BORDER_MARGIN from the bottom, so the fourth one
    //     occupies y=136 to 192, and the icon and the title together ran from
    //     74 to 200. The title alone, hung under the back button, ends well
    //     above it;
    //   - the icons this repository has name formats -- icon_bip39,
    //     icon_sskr, icon_bip85 -- while these entries name intentions.
    //     Generate and Recover would both have taken icon_sskr, which is
    //     exactly what the previous menu did to its BIP85 entry: it wore the
    //     SSKR icon, so two of three buttons were illustrated identically.
    //
    // No emphasised button either. The black button of an NBGL screen is its
    // primary action, and these four are peers.
    nbgl_screenSet(&screenChildren, SELECT_MENU_NB_CHILDREN, NULL,
                   (nbgl_touchCallback_t)&select_menu_callback);

    screenChildren[SELECT_MENU_TEXT_INDEX] =
        (nbgl_obj_t*)generic_screen_set_top_title();
    ((nbgl_text_area_t*)screenChildren[SELECT_MENU_TEXT_INDEX])->text =
        UI_STR_NBGL_MENU_TITLE;

    nbgl_objPoolGetArray(
        BUTTON, SELECT_MENU_NB_BUTTONS, 0,
        (nbgl_obj_t**)&screenChildren[SELECT_MENU_DERIVE_INDEX]);
    generic_screen_configure_buttons(
        (nbgl_button_t**)&screenChildren[SELECT_MENU_DERIVE_INDEX],
        SELECT_MENU_NB_BUTTONS);

    ((nbgl_button_t*)screenChildren[SELECT_MENU_CHECK_INDEX])->text =
        UI_STR_NBGL_MENU_CHECK;
    ((nbgl_button_t*)screenChildren[SELECT_MENU_BACKUP_INDEX])->text =
        UI_STR_NBGL_MENU_BACKUP;
    ((nbgl_button_t*)screenChildren[SELECT_MENU_RECOVER_INDEX])->text =
        UI_STR_NBGL_MENU_RECOVER;
    ((nbgl_button_t*)screenChildren[SELECT_MENU_DERIVE_INDEX])->text =
        UI_STR_NBGL_MENU_DERIVE;

    // create back button
    screenChildren[SELECT_MENU_BACK_BUTTON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_back_button();

    nbgl_screenRedraw();
}

/*
 * Select Recover BIP39
 */
static void select_recover_bip39_choice(bool bip39_rec) {
    nbgl_layoutRelease(layout);
    if (bip39_rec) {
        SPRINTF(headerText, UI_STR_BIP39_PHRASE_TITLE);
        strncpy(reviewText, bip39_mnemonic_get(), bip39_mnemonic_length_get());
        // Ensure null termination
        reviewText[bip39_mnemonic_length_get()] = '\0';
        display_generic_review();
    } else {
        display_home_page();
    }
}

void display_select_recover_bip39_page(void) {
    nbgl_useCaseChoice(&BIP39_ICON, UI_STR_NBGL_RECOVER_BIP39_TITLE,
                       UI_STR_NBGL_RECOVER_BIP39_DESC,
                       UI_STR_NBGL_RECOVER_BIP39_CONFIRM, UI_STR_NBGL_CANCEL,
                       select_recover_bip39_choice);
}

/*
 * Backing up: why the phrase is asked for first
 *
 * The one screen this change adds to the path the user walks. "Generate
 * backup shares" leads here, and here explains why an application running on
 * a device that already holds the phrase is about to ask for twenty-four
 * words -- compare_recovery_phrase() gets a seed back from the device, never
 * the words, so the words have to come from the person.
 *
 * It takes the place of "Generate SSKR Phrase?", which used to sit *after*
 * the verdict and ask whether to do the thing the user had not asked for.
 * Declining still lands on the home page, and still through
 * display_home_page(), which is what calls reset_globals(); nothing is entered
 * yet at this point.
 */
static void backup_explain_choice(bool proceed) {
    nbgl_layoutRelease(layout);
    if (proceed) {
        display_bip39_select_phrase_length_page();
    } else {
        display_home_page();
    }
}

static void display_backup_explain_page(void) {
    nbgl_useCaseChoice(&SSKR_ICON, UI_STR_NBGL_BACKUP_EXPLAIN_TITLE,
                       UI_STR_NBGL_BACKUP_EXPLAIN_DESC,
                       UI_STR_NBGL_BACKUP_EXPLAIN_CONFIRM, UI_STR_NBGL_CANCEL,
                       backup_explain_choice);
}

/*
 * Select mnemonic size page
 */
enum __attribute__((packed)) select_bip39_phrase_length {
    SELECT_BIP39_PHRASE_LENGTH_ICON_INDEX = 0,
    SELECT_BIP39_PHRASE_LENGTH_TEXT_INDEX,
    SELECT_BIP39_PHRASE_LENGTH_BUTTON_12_INDEX,
    SELECT_BIP39_PHRASE_LENGTH_BUTTON_18_INDEX,
    SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX,
    SELECT_BIP39_PHRASE_LENGTH_BACK_BUTTON_INDEX,
    SELECT_BIP39_PHRASE_LENGTH_NB_CHILDREN,
    KBD_TEXT_TOKEN
};

static const char* bip39_passphraseLength[] = {UI_STR_WORDS_12, UI_STR_WORDS_18,
                                               UI_STR_WORDS_24};
static void select_bip39_phrase_length_callback(nbgl_obj_t* obj,
                                                nbgl_touchType_t eventType) {
    nbgl_obj_t** screenChildren = nbgl_screenGetElements(0);
    if (eventType != TOUCHED) {
        return;
    }
    io_seproxyhal_play_tune(TUNE_TAP_CASUAL);
    nbgl_layoutRelease(layout);
    if (obj == screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_12_INDEX]) {
        bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_12);
    } else if (obj ==
               screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_18_INDEX]) {
        bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_18);
    } else if (obj ==
               screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX]) {
        bip39_mnemonic_final_size_set(BIP39_MNEMONIC_SIZE_24);
    } else if (obj ==
               screenChildren[SELECT_BIP39_PHRASE_LENGTH_BACK_BUTTON_INDEX]) {
        // Back goes to whatever asked for this screen, which is now three
        // different things. Written on the intention rather than on the tool
        // because Check and Backup share the tool and do not share a caller.
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
                // Recover enters ByteWords and never chooses a BIP-39 length;
                // it is grouped with Check so that this switch stays
                // exhaustive and a new intention is a -Wswitch diagnostic
                // rather than a silent fall onto someone else's back button.
                display_select_menu_page();
                break;
        }
        return;
    }
    if (user_intent == USER_INTENT_DERIVE) {
        display_bip85_select_index_page();
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
    nbgl_obj_t** screenChildren;

    // From top to bottom:
    // <return back arrow> + <icon> + <text> + <3 buttons>
    nbgl_screenSet(&screenChildren, SELECT_BIP39_PHRASE_LENGTH_NB_CHILDREN,
                   NULL,
                   (nbgl_touchCallback_t)&select_bip39_phrase_length_callback);

    screenChildren[SELECT_BIP39_PHRASE_LENGTH_ICON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_icon(&BIP39_ICON);
    screenChildren[SELECT_BIP39_PHRASE_LENGTH_TEXT_INDEX] =
        (nbgl_obj_t*)generic_screen_set_title(
            screenChildren[SELECT_BIP39_PHRASE_LENGTH_ICON_INDEX]);
    ((nbgl_text_area_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_TEXT_INDEX])
        ->text = bip39_length_title();
    // create nb words buttons
    nbgl_objPoolGetArray(BUTTON, ARRAYLEN(bip39_passphraseLength), 0,
                         (nbgl_obj_t**)&screenChildren
                             [SELECT_BIP39_PHRASE_LENGTH_BUTTON_12_INDEX]);
    generic_screen_configure_buttons(
        (nbgl_button_t**)&screenChildren
            [SELECT_BIP39_PHRASE_LENGTH_BUTTON_12_INDEX],
        ARRAYLEN(bip39_passphraseLength));
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_12_INDEX])
        ->text = bip39_passphraseLength[0];
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_18_INDEX])
        ->text = bip39_passphraseLength[1];
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX])
        ->text = bip39_passphraseLength[2];
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX])
        ->borderColor = BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX])
        ->innerColor = BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_BUTTON_24_INDEX])
        ->foregroundColor = WHITE;

    // create back button
    screenChildren[SELECT_BIP39_PHRASE_LENGTH_BACK_BUTTON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_back_button();

    nbgl_screenRedraw();
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
                display_sskr_select_numshares_page();
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
               "verdict_body[] has one row per intention and the backup row "
               "does not say what the check row says on the same tool; a new "
               "intention needs a decision here, not a default row");
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
    static const char* const titles[OUTCOME_NB] = {
        UI_STR_NBGL_RESULT_INVALID_TITLE, UI_STR_NBGL_RESULT_NOMATCH_TITLE,
        UI_STR_NBGL_RESULT_VALID_TITLE};
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
    // all: user_intent is a packed enum, one byte of static RAM, and this is
    // the only array in this file indexed by it. USER_INTENT_NB is itself out
    // of range and is a case label in three switches, so it is a value the
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
        .centeredInfo.text1 = titles[outcome],
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
        display_sskr_select_threshold_page();
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

static void display_generic_review() {
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

static void display_sskr_shares(void) {
    sskr_shares_from_bip39_mnemonic();

    static nbgl_genericContents_t genericContent;
    genericContent.callbackCallNeeded = true;
    genericContent.contentGetterCallback = review_sskr_shares_contentGetter;
    genericContent.nbContents = sskr_sharecount_get();

    nbgl_useCaseGenericReview(&genericContent, UI_STR_NBGL_CLOSE, review_done);
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
        display_sskr_shares();
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

enum __attribute__((packed)) select_bip85_app {
    SELECT_BIP85_APP_ICON_INDEX = 0,
    SELECT_BIP85_APP_TEXT_INDEX,
    SELECT_BIP85_APP_BUTTON_BIP39_INDEX,
    SELECT_BIP85_APP_BUTTON_PWD_BASE64_INDEX,
    SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX,
    SELECT_BIP85_APP_BACK_BUTTON_INDEX,
    SELECT_BIP85_APP_NB_CHILDREN,
};

static const char* bip85_select_app[] = {UI_STR_NBGL_BIP85_APP_BIP39,
                                         UI_STR_NBGL_BIP85_APP_PWD_BASE64,
                                         UI_STR_NBGL_BIP85_APP_PWD_BASE85};
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
        switch (bip85_type_get()) {
            case BIP85_APP_BIP39:
                bip85_app_bip39_gen();
                SPRINTF(headerText, UI_STR_NBGL_BIP85_BIP39_HEADER,
                        bip85_index_get());
                strncpy(reviewText, bip39_mnemonic_get(),
                        bip39_mnemonic_length_get());
                // Ensure null termination
                reviewText[bip39_mnemonic_length_get()] = '\0';
                display_generic_review();
                break;
            case BIP85_APP_PWD_BASE64:
                SPRINTF(headerText, UI_STR_NBGL_BIP85_BASE64_HEADER,
                        bip85_index_get());
                strncpy(reviewText, (const char*)bip85_app_pwd_base64_gen(),
                        bip85_length_get());
                // Ensure null termination
                reviewText[bip85_length_get()] = '\0';
                display_generic_review();
                break;
            case BIP85_APP_PWD_BASE85:
                SPRINTF(headerText, UI_STR_NBGL_BIP85_BASE85_HEADER,
                        bip85_index_get());
                strncpy(reviewText, (const char*)bip85_app_pwd_base85_gen(),
                        bip85_length_get());
                // Ensure null termination
                reviewText[bip85_length_get()] = '\0';
                display_generic_review();
                break;
            default:
                display_bip85_select_app_page();
                break;
        }
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
        display_bip85_select_index_page();
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

static void select_bip85_app_callback(nbgl_obj_t* obj,
                                      nbgl_touchType_t eventType) {
    nbgl_obj_t** screenChildren = nbgl_screenGetElements(0);
    if (eventType != TOUCHED) {
        return;
    }
    io_seproxyhal_play_tune(TUNE_TAP_CASUAL);
    nbgl_layoutRelease(layout);
    if (obj == screenChildren[SELECT_BIP85_APP_BUTTON_BIP39_INDEX]) {
        bip85_type_set(BIP85_APP_BIP39);
        display_bip39_select_phrase_length_page();
    } else if (obj ==
               screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE64_INDEX]) {
        bip85_type_set(BIP85_APP_PWD_BASE64);
        display_bip85_select_password_length_page();
    } else if (obj ==
               screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX]) {
        bip85_type_set(BIP85_APP_PWD_BASE85);
        display_bip85_select_password_length_page();
    } else if (obj == screenChildren[SELECT_BIP85_APP_BACK_BUTTON_INDEX]) {
        display_select_menu_page();
        return;
    } else {
        display_home_page();
    }
}

static void display_bip85_select_app_page(void) {
    nbgl_obj_t** screenChildren;

    // From top to bottom:
    // <return back arrow> + <icon> + <text> + <3 buttons>
    nbgl_screenSet(&screenChildren, SELECT_BIP85_APP_NB_CHILDREN, NULL,
                   (nbgl_touchCallback_t)&select_bip85_app_callback);

    screenChildren[SELECT_BIP85_APP_ICON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_icon(&BIP85_ICON);
    screenChildren[SELECT_BIP39_PHRASE_LENGTH_TEXT_INDEX] =
        (nbgl_obj_t*)generic_screen_set_title(
            screenChildren[SELECT_BIP85_APP_ICON_INDEX]);
    ((nbgl_text_area_t*)screenChildren[SELECT_BIP39_PHRASE_LENGTH_TEXT_INDEX])
        ->text = UI_STR_NBGL_BIP85_SELECT_APP_TITLE;

    // create bip85 app buttons
    nbgl_objPoolGetArray(
        BUTTON, ARRAYLEN(bip85_select_app), 0,
        (nbgl_obj_t**)&screenChildren[SELECT_BIP85_APP_BUTTON_BIP39_INDEX]);
    generic_screen_configure_buttons(
        (nbgl_button_t**)&screenChildren[SELECT_BIP85_APP_BUTTON_BIP39_INDEX],
        ARRAYLEN(bip85_select_app));
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_BIP39_INDEX])
        ->text = bip85_select_app[0];
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE64_INDEX])
        ->text = bip85_select_app[1];
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX])
        ->text = bip85_select_app[2];
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX])
        ->borderColor = BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX])
        ->innerColor = BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_BIP85_APP_BUTTON_PWD_BASE85_INDEX])
        ->foregroundColor = WHITE;

    // create back button
    screenChildren[SELECT_BIP85_APP_BACK_BUTTON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_back_button();

    nbgl_screenRedraw();
}

/*
 * Public function
 */
void ui_idle_init(void) { display_home_page(); }
#endif
