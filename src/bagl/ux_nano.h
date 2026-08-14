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

#include <ux.h>
#include "ui.h"
#include "../common/common.h"
// user_intent_e, stored in the context below and shared with the touch stack
#include "constants.h"

#if defined(HAVE_BAGL)

typedef const bagl_element_t *(*keyboard_callback_t)(unsigned int event, unsigned int value);

void bolos_ux_hslider3_init(unsigned int total_count);
void bolos_ux_hslider3_set_current(unsigned int current);
void bolos_ux_hslider3_next(void);
void bolos_ux_hslider3_previous(void);

// Layout of G_ux.string_buffer while a word is being entered, as
// screen_onboarding_restore_word_init() describes it: display scratch at 0,
// the stem entered so far at 16, the candidate next letters at 32.
//
// The stem therefore has RESTORE_STEM_MAX_LENGTH characters before its
// terminator would land on the first candidate. What keeps it inside that
// today is the wordlist -- the longest BIP-39 word is BIP39_MAX_WORD_LENGTH
// characters, a ByteWord is SSKR_BYTEWORD_LENGTH, and the keyboard only offers
// letters that extend a real prefix. That is a bound computed in another file,
// for another purpose, that these writes happen to benefit from; the names
// below are the buffer's own.
#define RESTORE_STEM_OFFSET       16
#define RESTORE_CANDIDATES_OFFSET 32
#define RESTORE_STEM_MAX_LENGTH   (RESTORE_CANDIDATES_OFFSET - RESTORE_STEM_OFFSET - 1)

// all screens
void screen_onboarding_bip39_restore_init(void);
void screen_onboarding_sskr_restore_init(void);
void screen_onboarding_restore_word_init(unsigned int action);
void screen_onboarding_restore_word_display_auto_complete(void);

// bolos ux context (not mandatory if redesigning a bolos ux)
typedef struct bolos_ux_context {
    // 12, 18 or 24 word BIP39. Written by the 12/18/24 menu and by nothing
    // else: the SSKR entry path used to store a share length here too, which
    // made the field mean two things and left the bound on words_buffer
    // depending on which screen had been visited last.
    unsigned int bip39_type;

    // How many ByteWords one SSKR share holds, taken from the CBOR header of
    // the first share as it is typed.
    //
    // Zero until the fourth word of that share has been entered, which is the
    // earliest bolos_ux_sskr_entry_header_update() can know it -- and zero is
    // a safe "not yet": the entry loop compares it against a step count that
    // has already been incremented, so it never matches before the header has
    // been read. Every share of a set has the same shape, so it is read once
    // and kept for the shares that follow.
    unsigned int sskr_share_word_count;

    // Type of tool we are using (BIP39 or SSKR)
    unsigned int tool_type;

    // Which entry of the idle menu the user picked. Not the same thing as the
    // tool: checking a phrase and splitting one into shares both enter a
    // BIP-39 phrase, so both are TOOL_TYPE_BIP39, and only this tells the
    // verdict flow which of the two it is ending. Written by the idle menu
    // and by ui_idle_init(), read by ux_bip39_match_display() and
    // ux_bip39_nomatch_display() in src/bagl/ux_nano.c.
    user_intent_e user_intent;

    // State of the dynamic display
    unsigned int current_state;

#ifdef HAVE_ELECTRUM
    unsigned int onboarding_algorithm;
#endif  // HAVE_ELECTRUM

    unsigned int onboarding_step;
    unsigned int onboarding_index;
    unsigned int onboarding_words_checked;

    unsigned int words_buffer_length;

    // 128 of words (215 => hashed to 64, or 128) + HMAC_LENGTH*2 = 256
#define WORDS_BUFFER_MAX_SIZE_B 257
    char words_buffer[WORDS_BUFFER_MAX_SIZE_B];

    // after an int to make sure it's aligned
#define BOLOS_APP_ICON_SIZE_B (9 + 32)
    char string_buffer[MAX(
        64,
        sizeof(bagl_icon_details_t) + BOLOS_APP_ICON_SIZE_B - 1)];  // to store the seed wholly

#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
    // label line for common PIN and common keyboard screen (displayed over the entry)
    const char *common_label;
#endif                      // defined(TARGET_NANOX) || defined(TARGET_NANOS2)
    char pin_digit_buffer;  // digit to be displayed

    // slider management / menu list management
    unsigned int hslider3_before;
    unsigned int hslider3_current;
    unsigned int hslider3_after;
    unsigned int hslider3_total;

    keyboard_callback_t keyboard_callback;

    // for CheckSeed app only
    uint8_t processing;

#if defined(TARGET_NANOS)
    // 7 shares * 229 chars per share (46 SSKR Bytewords)
#define SSKR_WORDS_BUFFER_MAX_SIZE_B 1603
#else
    // 16 shares * 229 chars per share (46 SSKR Bytewords)
#define SSKR_WORDS_BUFFER_MAX_SIZE_B 3664
#endif
    uint8_t sskr_share_count;
    uint8_t sskr_share_index;
    unsigned int sskr_group_descriptor[1][2];
    unsigned int sskr_words_buffer_length;
    char sskr_words_buffer[SSKR_WORDS_BUFFER_MAX_SIZE_B];
} bolos_ux_context_t;

extern bolos_ux_context_t G_bolos_ux_context;

// update before, current, after index for horizontal slider with 3 positions
// slider distinguish handling from the data, to be more generic :)
#define BOLOS_UX_HSLIDER3_NONE (-1UL)

void screen_common_keyboard_init(unsigned int stack_slot,
                                 unsigned int current_element,
                                 unsigned int nb_elements,
                                 keyboard_callback_t callback);

void set_sskr_descriptor_values(void);
void recover_bip39(void);

// The BIP-39 verdict, in whichever form the intention calls for. Two flows
// per outcome, and the choice between them lives next to the flows in
// src/bagl/ux_nano.c rather than at the call sites: nanos_enter_phrase.c and
// nanox_enter_phrase.c both reach the verdict, by different routes, and a
// copy of this decision in each is a third thing to keep in step.
void ux_bip39_match_display(void);
void ux_bip39_nomatch_display(void);

#include "common/bip39/common_bip39.h"
#include "common/sskr/common_sskr.h"

void clean_exit(bolos_task_status_t exit_code);

#if defined(TARGET_NANOS)
#define BIP39_ICON                         C_icon_bip39_16px
#define SSKR_ICON                          C_icon_sskr_16px
#define PROCESSING_COMPLETE                0
#define PROCESSING_COMPARE_RECOVERY_PHRASE 1
#define PROCESSING_GENERATE_SSKR           2

extern const bagl_element_t screen_onboarding_word_list_elements[9];
void compare_recovery_phrase_and_display_result(void);
void generate_sskr(void);
void screen_processing_init(void);
#else
#define BIP39_ICON C_icon_bip39_14px
#define SSKR_ICON  C_icon_sskr_14px

// to be included into all flow that needs to go back to the dashboard
extern const ux_flow_step_t ux_ob_goto_dashboard_step;
#endif  // defined(TARGET_NANOS)

#endif  // defined(HAVE_BAGL)
