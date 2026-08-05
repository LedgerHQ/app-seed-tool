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

unsigned int tool_type;

static void display_home_page(void);
static void display_check_keyboard_page(void);
static void display_check_result_page(const bool result);
static void display_bip39_select_phrase_length_page(void);
static void display_generic_review(void);
static void display_sskr_select_numshares_page(void);
static void display_sskr_select_threshold_page(void);
static void display_bip85_select_app_page(void);
static void display_bip85_select_index_page(void);

/*
 * Utils
 */
static const char* buttonTexts[NB_MAX_SUGGESTION_BUTTONS] = {0};

static void reset_globals() {
    bip39_mnemonic_reset();
    sskr_shares_reset();
    bip85_app_reset();
    memzero(buttonTexts, sizeof(buttonTexts[0]) * NB_MAX_SUGGESTION_BUTTONS);
    memzero(headerText, sizeof(headerText));
    memzero(reviewText, sizeof(reviewText));
}

static void on_quit(void) { os_sched_exit(-1); }

/*
 * Select tool type, BIP39 or SSKR
 */
enum __attribute__((packed)) select_tool {
    SELECT_TOOL_ICON_INDEX = 0,
    SELECT_TOOL_TEXT_INDEX,
    SELECT_TOOL_BIP39_INDEX,
    SELECT_TOOL_SSKR_INDEX,
    SELECT_TOOL_BIP85_INDEX,
    SELECT_TOOL_BACK_BUTTON_INDEX,
    SELECT_TOOL_NB_CHILDREN
};

static const char* toolType[] = {UI_STR_NBGL_TOOL_BIP39, UI_STR_NBGL_TOOL_SSKR,
                                 UI_STR_NBGL_TOOL_BIP85};
static void select_tool_callback(nbgl_obj_t* obj, nbgl_touchType_t eventType) {
    nbgl_obj_t** screenChildren = nbgl_screenGetElements(0);
    if (eventType != TOUCHED) {
        return;
    }
    io_seproxyhal_play_tune(TUNE_TAP_CASUAL);
    nbgl_layoutRelease(layout);
    if (obj == screenChildren[SELECT_TOOL_BIP39_INDEX]) {
        tool_type = TOOL_TYPE_BIP39;
        display_bip39_select_phrase_length_page();
    } else if (obj == screenChildren[SELECT_TOOL_SSKR_INDEX]) {
        tool_type = TOOL_TYPE_SSKR;
        display_check_keyboard_page();
    } else if (obj == screenChildren[SELECT_TOOL_BIP85_INDEX]) {
        tool_type = TOOL_TYPE_BIP85;
        display_bip85_select_app_page();
    } else if (obj == screenChildren[SELECT_TOOL_BACK_BUTTON_INDEX]) {
        display_home_page();
        return;
    }
}

static void display_select_tool_page(void) {
    nbgl_obj_t** screenChildren;

    // From top to bottom:
    // <return back arrow> + <icon> + <text> + <3 buttons>
    nbgl_screenSet(&screenChildren, SELECT_TOOL_NB_CHILDREN, NULL,
                   (nbgl_touchCallback_t)&select_tool_callback);

    screenChildren[SELECT_TOOL_ICON_INDEX] =
        (nbgl_obj_t*)generic_screen_set_icon(&ICON_APP_HOME);
    screenChildren[SELECT_TOOL_TEXT_INDEX] =
        (nbgl_obj_t*)generic_screen_set_title(
            screenChildren[SELECT_TOOL_ICON_INDEX]);
    ((nbgl_text_area_t*)screenChildren[SELECT_TOOL_TEXT_INDEX])->text =
        UI_STR_NBGL_SELECT_TOOL_TITLE;
    // create nb words buttons
    nbgl_objPoolGetArray(
        BUTTON, ARRAYLEN(toolType), 0,
        (nbgl_obj_t**)&screenChildren[SELECT_TOOL_BIP39_INDEX]);
    generic_screen_configure_buttons(
        (nbgl_button_t**)&screenChildren[SELECT_TOOL_BIP39_INDEX],
        ARRAYLEN(toolType));
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_BIP39_INDEX])->text =
        toolType[0];
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_BIP39_INDEX])->icon =
        &BIP39_ICON_SMALL;
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_SSKR_INDEX])->text =
        toolType[1];
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_SSKR_INDEX])->icon =
        &SSKR_ICON_SMALL;
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_SSKR_INDEX])->borderColor =
        BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_SSKR_INDEX])->innerColor =
        BLACK;
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_SSKR_INDEX])->foregroundColor =
        WHITE;
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_BIP85_INDEX])->text =
        toolType[2];
    ((nbgl_button_t*)screenChildren[SELECT_TOOL_BIP85_INDEX])->icon =
        &SSKR_ICON_SMALL;

    // create back button
    screenChildren[SELECT_TOOL_BACK_BUTTON_INDEX] =
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
 * Select Generate SSKR
 */
static void select_generate_sskr_choice(bool sskr_gen) {
    nbgl_layoutRelease(layout);
    if (sskr_gen) {
        display_sskr_select_numshares_page();
    } else {
        display_home_page();
    }
}

void display_select_generate_sskr_page(void) {
    nbgl_useCaseChoice(&SSKR_ICON, UI_STR_NBGL_GENERATE_SSKR_TITLE,
                       UI_STR_NBGL_GENERATE_SSKR_DESC,
                       UI_STR_NBGL_GENERATE_SSKR_CONFIRM, UI_STR_NBGL_CANCEL,
                       select_generate_sskr_choice);
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
        if (tool_type == TOOL_TYPE_BIP85) {
            display_bip85_select_app_page();
        } else {
            display_select_tool_page();
        }
        return;
    }
    if (tool_type == TOOL_TYPE_BIP85) {
        display_bip85_select_index_page();
    } else {
        display_check_keyboard_page();
    }
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
        ->text = tool_type == TOOL_TYPE_BIP39
                     ? UI_STR_NBGL_BIP39_LENGTH_TITLE_CHECK
                     : UI_STR_NBGL_BIP39_LENGTH_TITLE_DERIVE;
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
// the biggest word of BIP39 list is 8 char (9 with trailing '\0'), and
// the max number of showed suggestions is NB_MAX_SUGGESTION_BUTTONS
static char wordCandidates[(BIP39_MAX_WORD_LENGTH + 1) *
                           NB_MAX_SUGGESTION_BUTTONS] = {0};

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
            display_select_tool_page();
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
                                .callback = PIC(display_select_tool_page)};

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
    if (tool_type == TOOL_TYPE_BIP39 && bip39_mnemonic_check(&seed_match) &&
        seed_match) {
        display_select_generate_sskr_page();
    } else if (tool_type == TOOL_TYPE_SSKR && sskr_shares_check(&seed_match)) {
        display_select_recover_bip39_page();
    } else {
        reset_globals();
        display_home_page();
    }
}

static void display_check_result_page(const bool result) {
    static const char* possible_results[2][5] = {
        {NULL, UI_STR_NBGL_RESULT_BIP39_INVALID, "",
         UI_STR_NBGL_RESULT_SSKR_INVALID, ""},
        {NULL, UI_STR_NBGL_RESULT_BIP39_NOMATCH, UI_STR_NBGL_RESULT_BIP39_MATCH,
         UI_STR_NBGL_RESULT_SSKR_NOMATCH, UI_STR_NBGL_RESULT_SSKR_MATCH}};
    // Three distinct outcomes -- invalid, doesn't match, matches -- each with
    // its own title and icon, matching what the BAGL flows
    // (ux_bip39_invalid_flow / ux_bip39_nomatch_flow / ux_bip39_match_flow
    // and their SSKR equivalents) already give the user.
    static const char* const titles[3] = {UI_STR_NBGL_RESULT_INVALID_TITLE,
                                          UI_STR_NBGL_RESULT_NOMATCH_TITLE,
                                          UI_STR_NBGL_RESULT_VALID_TITLE};
    static const nbgl_icon_details_t* icons[3] = {
        &DENIED_CIRCLE_ICON, &IMPORTANT_CIRCLE_ICON, &CHECK_CIRCLE_ICON};

    // result is false only when the phrase itself is not well formed, in
    // which case seed_match is never set true; the sum is therefore always
    // 0, 1 or 2, indexing the invalid / nomatch / match outcome respectively.
    const uint8_t outcome = (uint8_t)(result + seed_match);

    // The text index is 1 + tool_type * 2 + seed_match into a five-element row.
    // TOOL_TYPE_BIP39 and TOOL_TYPE_SSKR give 1..4; TOOL_TYPE_BIP85, the third
    // value of that enumeration (constants.h), would give 5 or 6 and read past
    // the row. No path reaches this screen with the BIP85 tool selected -- that
    // flow goes to display_generic_review() and never asks for a verdict -- so
    // this is a bound on an index the screens cannot currently produce, not a
    // fix for one they can.
    const uint8_t text_index =
        (tool_type == TOOL_TYPE_BIP39 || tool_type == TOOL_TYPE_SSKR)
            ? (uint8_t)(1 + (tool_type * 2) + seed_match)
            : 1;

    nbgl_pageInfoDescription_t info = {
        .centeredInfo.icon = icons[outcome],
        .centeredInfo.text1 = titles[outcome],
        .centeredInfo.text2 = possible_results[result][text_index],
        // Advice only on the invalid outcome; NULL elsewhere.
        // LARGE_CASE_GRAY_INFO is required for this to render as its own gray
        // line -- see the comment on UI_STR_NBGL_RESULT_INVALID_ADVICE in
        // ui_strings.h.
        .centeredInfo.text3 = result ? NULL : UI_STR_NBGL_RESULT_INVALID_ADVICE,
        .centeredInfo.style = LARGE_CASE_GRAY_INFO,
        .centeredInfo.offsetY = -16,
        .footerText = UI_STR_NBGL_RESULT_TAP_TO_DISMISS,
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

static void sskr_sharenum_validate(const uint8_t* sharenumentry,
                                   uint8_t length) {
    // Code to validate the entered shares number

    sskr_sharenum_set(0);

    for (uint8_t i = 0; i < length; i++) {
        sskr_sharenum_set(10 * sskr_sharenum_get() + sharenumentry[i] - '0');
    }

    PRINTF("Number of shares entered is '%d'\n", sskr_sharenum_get());

    if (sskr_sharenum_get() > 0 && sskr_sharenum_get() <= 16) {
        display_sskr_select_threshold_page();
    } else {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_NUMSHARES_RANGE_ERROR, false,
                           display_select_generate_sskr_page);
    }
}

void display_sskr_select_numshares_page() {
    // Draw the keypad
    nbgl_useCaseKeypad(
        UI_STR_NBGL_SSKR_NUMSHARES_TITLE, 1, SSKR_MAX_NUMBER_LENGTH, false,
        false, sskr_sharenum_validate, display_select_generate_sskr_page);
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

    if (sskr_threshold_get() < 1) {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_ZERO_ERROR, false,
                           display_select_generate_sskr_page);
    } else if (sskr_threshold_get() > sskr_sharenum_get()) {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_RANGE_ERROR, false,
                           display_select_generate_sskr_page);
    } else if (sskr_threshold_get() == 1 && sskr_sharenum_get() > 1) {
        nbgl_useCaseStatus(UI_STR_NBGL_SSKR_THRESHOLD_ONE_OF_M_ERROR, false,
                           display_select_generate_sskr_page);
    } else {
        display_sskr_shares();
    }
}

void display_sskr_select_threshold_page() {
    // Draw the keypad
    nbgl_useCaseKeypad(
        UI_STR_NBGL_SSKR_THRESHOLD_TITLE, 1, SSKR_MAX_NUMBER_LENGTH, false,
        false, sskr_threshold_validate, display_sskr_select_numshares_page);
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
        nbgl_useCaseStatus(UI_STR_NBGL_BIP85_INDEX_RANGE_ERROR, false,
                           display_bip39_select_phrase_length_page);
    }
}

void display_bip85_select_index_page() {
    // Draw the keypad
    nbgl_useCaseKeypad(UI_STR_NBGL_BIP85_INDEX_TITLE, 1,
                       BIP85_INDEX_MAX_NUMBER_LENGTH, false, false,
                       bip85_index_validate, display_bip85_select_app_page);
}

static void bip85_password_length_validate(const uint8_t* lengthentry,
                                           uint8_t length) {
    // Code to validate BIP85 index

    bip85_length_set(0);

    for (uint8_t i = 0; i < length; i++) {
        bip85_length_set(10 * bip85_length_get() + lengthentry[i] - '0');
    }
    PRINTF("BIP85 password length entered is '%d'\n", bip85_length_get());

    uint8_t password_length_min =
        bip85_type_get() == BIP85_APP_PWD_BASE64 ? 20 : 10;
    uint8_t password_length_max =
        bip85_type_get() == BIP85_APP_PWD_BASE64 ? 86 : 80;
    char message[50] = {0};

    if ((bip85_length_get() >= password_length_min) &&
        (bip85_length_get() <= password_length_max)) {
        display_bip85_select_index_page();
    } else {
        snprintf(message, sizeof(message),
                 UI_STR_NBGL_BIP85_PWD_LENGTH_RANGE_ERROR, password_length_min,
                 password_length_max);
        nbgl_useCaseStatus((const char*)message, false,
                           display_bip85_select_app_page);
    }
}

void display_bip85_select_password_length_page() {
    // Draw the keypad
    nbgl_useCaseKeypad(UI_STR_NBGL_BIP85_PWD_LENGTH_TITLE, 1, 2, false, false,
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
        display_select_tool_page();
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
