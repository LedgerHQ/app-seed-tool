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

#include "ui.h"

#include <ux.h>

#if defined(HAVE_BAGL)

#include "common/ui_strings.h"
#include "constants.h"

//////////////////////////////////////////////////////////////////////

void screen_onboarding_bip39_restore_init(void) {
    G_bolos_ux_context.tool_type = TOOL_TYPE_BIP39;
    screen_onboarding_restore_word_init(RESTORE_WORD_ACTION_FIRST_WORD);
}

const char* const number_of_bip39_words_values[] = {
    UI_STR_WORDS_12,
    UI_STR_WORDS_18,
    UI_STR_WORDS_24,
    UI_STR_BAGL_BIP39_LENGTH_BACK,
};

const char* number_of_bip39_words_getter(unsigned int idx) {
    if (idx < ARRAYLEN(number_of_bip39_words_values)) {
        return number_of_bip39_words_values[idx];
    }
    return NULL;
}

void number_of_bip39_words_selector(unsigned int idx) {
    switch (idx) {
        case 0:
            G_bolos_ux_context.bip39_type = BIP39_MNEMONIC_SIZE_12;
            goto word_init;
        case 1:
            G_bolos_ux_context.bip39_type = BIP39_MNEMONIC_SIZE_18;
            goto word_init;
        case 2:
            G_bolos_ux_context.bip39_type = BIP39_MNEMONIC_SIZE_24;
            goto word_init;
        word_init:
            screen_onboarding_bip39_restore_init();
            break;
        default:
            ui_idle_init();
    }
}

#if defined(TARGET_NANOS)
UX_STEP_NOCB(ux_bip39_instruction_step, nn,
             {UI_STR_BAGL_BIP39_LENGTH_TITLE_L1_NANOS,
              UI_STR_BAGL_BIP39_LENGTH_TITLE_L2_NANOS});
#else
UX_STEP_NOCB(ux_bip39_instruction_step, nnn,
             {
                 UI_STR_BAGL_BIP39_LENGTH_TITLE_L1,
                 UI_STR_BAGL_BIP39_LENGTH_TITLE_L2,
                 UI_STR_BAGL_BIP39_LENGTH_TITLE_L3,
             });
#endif

UX_STEP_MENULIST(ux_bip39_menu_step, number_of_bip39_words_getter,
                 number_of_bip39_words_selector);

/*
 * No step explaining why the Phrase is asked for, on purpose: entering it *is*
 * the task the user chose from the menu. app-recovery-check, which does only
 * this, opens the same way. ux_backup_explain_step below exists because that
 * journey was asked for Shares and has to account for the Phrase.
 */
UX_FLOW(ux_bip39_flow, &ux_bip39_instruction_step, &ux_bip39_menu_step);

//////////////////////////////////////////////////////////////////////

/*
 * Splitting a phrase starts by explaining why the phrase is being asked for.
 *
 * Same reason as on the touch stack: compare_recovery_phrase()
 * (src/common/common_seed.c) gets a seed back from the device and never the
 * words, so the words have to come from the person -- and being asked to type
 * twenty-four of them into a device that already holds them is the moment
 * that needs saying. It needs saying more here than there, not less: on a
 * Nano those words are entered one letter at a time with two buttons.
 *
 * nanos shows two lines, which is why the first two are a complete sentence
 * on their own; the taller Nanos add the third.
 */
#if defined(TARGET_NANOS)
UX_STEP_NOCB(ux_backup_explain_step, nn,
             {
                 UI_STR_BAGL_BACKUP_EXPLAIN_L1,
                 UI_STR_BAGL_BACKUP_EXPLAIN_L2,
             });
#else
UX_STEP_NOCB(ux_backup_explain_step, nnn,
             {
                 UI_STR_BAGL_BACKUP_EXPLAIN_L1,
                 UI_STR_BAGL_BACKUP_EXPLAIN_L2,
                 UI_STR_BAGL_BACKUP_EXPLAIN_L3,
             });
#endif

// The length menu and its instruction are the same two steps the check flow
// uses -- the question they ask is the same one, and the intention already
// recorded is what makes the difference later.
UX_FLOW(ux_backup_flow, &ux_backup_explain_step, &ux_bip39_instruction_step,
        &ux_bip39_menu_step);

//////////////////////////////////////////////////////////////////////

void screen_onboarding_sskr_restore_init(void) {
    G_bolos_ux_context.tool_type = TOOL_TYPE_SSKR;
    screen_onboarding_restore_word_init(RESTORE_WORD_ACTION_FIRST_WORD);
}

#if defined(TARGET_NANOS)
UX_STEP_CB(ux_sskr_instruction_step, nn, screen_onboarding_sskr_restore_init(),
           {
               UI_STR_BAGL_SSKR_START_TITLE_L1_NANOS,
               UI_STR_BAGL_SSKR_START_TITLE_L2_NANOS,
           });
#else
UX_STEP_CB(ux_sskr_instruction_step, nnn,
           G_bolos_ux_context.tool_type = TOOL_TYPE_SSKR;
           screen_onboarding_restore_word_init(RESTORE_WORD_ACTION_FIRST_WORD);
           , {
                 UI_STR_BAGL_SSKR_START_TITLE_L1,
                 UI_STR_BAGL_SSKR_START_TITLE_L2,
                 UI_STR_BAGL_SSKR_START_TITLE_L3,
             });
#endif

#if defined(TARGET_NANOS)
UX_STEP_NOCB(ux_recover_explain_step, nn,
             {
                 UI_STR_BAGL_RECOVER_EXPLAIN_L1,
                 UI_STR_BAGL_RECOVER_EXPLAIN_L2,
             });
#else
UX_STEP_NOCB(ux_recover_explain_step, nnn,
             {
                 UI_STR_BAGL_RECOVER_EXPLAIN_L1,
                 UI_STR_BAGL_RECOVER_EXPLAIN_L2,
                 UI_STR_BAGL_RECOVER_EXPLAIN_L3,
             });
#endif

UX_FLOW(ux_sskr_flow, &ux_recover_explain_step, &ux_sskr_instruction_step);

//////////////////////////////////////////////////////////////////////

/*
 * The idle menu: one entry per intention, as on the touch stack, minus the
 * one that has nowhere to go.
 *
 * Three, not four. BIP-85 has no BAGL screen on any Nano, so a fourth entry
 * would lead nowhere; the two stacks do not have the same menu and this is
 * where they differ.
 *
 * Each entry records what the user came to do before starting the flow. That
 * is the whole reason the field exists: the first two both end up entering a
 * BIP-39 phrase and are both TOOL_TYPE_BIP39, and without it the verdict
 * cannot tell which of the two it is answering.
 *
 * The icon names the format the entry produces rather than the one it
 * consumes, which is what keeps the last two apart: splitting arrives at SSKR
 * shares, restoring arrives at a BIP-39 phrase.
 */
UX_STEP_VALID(ux_idle_flow_1_step, pbb,
              G_bolos_ux_context.user_intent = USER_INTENT_CHECK;
              ux_flow_init(0, ux_bip39_flow, NULL),
              {
                  &BIP39_ICON,
                  UI_STR_BAGL_IDLE_CHECK_L1,
                  UI_STR_BAGL_IDLE_CHECK_L2,
              });
UX_STEP_VALID(ux_idle_flow_2_step, pbb,
              G_bolos_ux_context.user_intent = USER_INTENT_BACKUP;
              ux_flow_init(0, ux_backup_flow, NULL),
              {
                  &SSKR_ICON,
                  UI_STR_BAGL_IDLE_BACKUP_L1,
                  UI_STR_BAGL_IDLE_BACKUP_L2,
              });
UX_STEP_VALID(ux_idle_flow_3_step, pbb,
              G_bolos_ux_context.user_intent = USER_INTENT_RECOVER;
              ux_flow_init(0, ux_sskr_flow, NULL),
              {
                  &BIP39_ICON,
                  UI_STR_BAGL_IDLE_RECOVER_L1,
                  UI_STR_BAGL_IDLE_RECOVER_L2,
              });

UX_STEP_NOCB(ux_idle_flow_4_step, bn,
             {
                 UI_STR_VERSION_LABEL,
                 APPVERSION,
             });
UX_STEP_VALID(ux_idle_flow_5_step, pb, os_sched_exit(-1),
              {
                  &C_icon_dashboard_x,
                  UI_STR_QUIT,
              });
UX_FLOW(ux_idle_flow, &ux_idle_flow_1_step, &ux_idle_flow_2_step,
        &ux_idle_flow_3_step, &ux_idle_flow_4_step, &ux_idle_flow_5_step);

void ui_idle_init(void) {
    memzero(G_bolos_ux_context.words_buffer,
            sizeof(G_bolos_ux_context.words_buffer));
    memzero(G_bolos_ux_context.string_buffer,
            sizeof(G_bolos_ux_context.string_buffer));
    memzero(G_bolos_ux_context.sskr_words_buffer,
            G_bolos_ux_context.sskr_words_buffer_length);
    G_bolos_ux_context.words_buffer_length = 0;
    G_bolos_ux_context.sskr_words_buffer_length = 0;
    G_bolos_ux_context.sskr_share_index = 0;
    // Back to the first entry, so that no flow started from this menu reads
    // what the previous one left behind. Every entry writes it again on the
    // way out, so this only matters for the value read before any entry has
    // been picked.
    G_bolos_ux_context.user_intent = USER_INTENT_CHECK;

    // reserve a display stack slot if none yet
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }
    ux_flow_init(0, ux_idle_flow, NULL);
}

#endif
