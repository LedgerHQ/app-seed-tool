/*******************************************************************************
 *   Ledger Blue - Secure firmware
 *   (c) 2016, 2017 Ledger
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

#include "ux_nano.h"

#if defined(HAVE_BAGL)

#include "common/ui_strings.h"

bolos_ux_context_t G_bolos_ux_context;

void clean_exit(bolos_task_status_t exit_code) {
    memzero(G_bolos_ux_context.words_buffer,
            sizeof(G_bolos_ux_context.words_buffer));
    memzero(G_bolos_ux_context.string_buffer,
            sizeof(G_bolos_ux_context.string_buffer));
    // sizeof, not sskr_words_buffer_length. The length is set to 0 by the
    // entry path without the buffer being erased, so an erase measured on it
    // can erase nothing at all: enter shares, get "not valid", press
    // "Re-enter Shares" and leave, and this ran memzero(buffer, 0) over a
    // buffer still holding every ByteWord that had been typed.
    memzero(G_bolos_ux_context.sskr_words_buffer,
            sizeof(G_bolos_ux_context.sskr_words_buffer));
    G_bolos_ux_context.words_buffer_length = 0;
    G_bolos_ux_context.sskr_words_buffer_length = 0;
    G_bolos_ux_context.sskr_share_index = 0;

    os_sched_exit(exit_code);
}

unsigned short io_timeout(unsigned short last_timeout) {
    UNUSED(last_timeout);
    // infinite timeout
    return 1;
}

void io_seproxyhal_display(const bagl_element_t* element) {
    io_seproxyhal_display_default((bagl_element_t*)element);
}

void bolos_ux_hslider3_init(unsigned int total_count) {
    G_bolos_ux_context.hslider3_total = total_count;
    switch (total_count) {
        case 0:
            G_bolos_ux_context.hslider3_before = BOLOS_UX_HSLIDER3_NONE;
            G_bolos_ux_context.hslider3_current = BOLOS_UX_HSLIDER3_NONE;
            G_bolos_ux_context.hslider3_after = BOLOS_UX_HSLIDER3_NONE;
            break;
        case 1:
            G_bolos_ux_context.hslider3_before = BOLOS_UX_HSLIDER3_NONE;
            G_bolos_ux_context.hslider3_current = 0;
            G_bolos_ux_context.hslider3_after = BOLOS_UX_HSLIDER3_NONE;
            break;
        case 2:
            G_bolos_ux_context.hslider3_before = BOLOS_UX_HSLIDER3_NONE;
            // G_bolos_ux_context.hslider3_before = 1; // full rotate
            G_bolos_ux_context.hslider3_current = 0;
            G_bolos_ux_context.hslider3_after = 1;
            break;
        default:
            G_bolos_ux_context.hslider3_before = total_count - 1;
            G_bolos_ux_context.hslider3_current = 0;
            G_bolos_ux_context.hslider3_after = 1;
            break;
    }
}

void bolos_ux_hslider3_set_current(unsigned int current) {
    // index is reachable ?
    if (G_bolos_ux_context.hslider3_total > current) {
        // reach it
        while (G_bolos_ux_context.hslider3_current != current) {
            bolos_ux_hslider3_next();
        }
    }
}

void bolos_ux_hslider3_next(void) {
    switch (G_bolos_ux_context.hslider3_total) {
        case 0:
        case 1:
            break;
        case 2:
            switch (G_bolos_ux_context.hslider3_current) {
                case 0:
                    G_bolos_ux_context.hslider3_before = 0;
                    G_bolos_ux_context.hslider3_current = 1;
                    G_bolos_ux_context.hslider3_after = BOLOS_UX_HSLIDER3_NONE;
                    break;
                case 1:
                    G_bolos_ux_context.hslider3_before = BOLOS_UX_HSLIDER3_NONE;
                    G_bolos_ux_context.hslider3_current = 0;
                    G_bolos_ux_context.hslider3_after = 1;
                    break;
            }
            break;
        default:
            G_bolos_ux_context.hslider3_before =
                G_bolos_ux_context.hslider3_current;
            G_bolos_ux_context.hslider3_current =
                G_bolos_ux_context.hslider3_after;
            G_bolos_ux_context.hslider3_after =
                (G_bolos_ux_context.hslider3_after + 1) %
                G_bolos_ux_context.hslider3_total;
            break;
    }
}

void bolos_ux_hslider3_previous(void) {
    switch (G_bolos_ux_context.hslider3_total) {
        case 0:
        case 1:
            break;
        case 2:
            switch (G_bolos_ux_context.hslider3_current) {
                case 0:
                    G_bolos_ux_context.hslider3_before = 0;
                    G_bolos_ux_context.hslider3_current = 1;
                    G_bolos_ux_context.hslider3_after = BOLOS_UX_HSLIDER3_NONE;
                    break;
                case 1:
                    G_bolos_ux_context.hslider3_before = BOLOS_UX_HSLIDER3_NONE;
                    G_bolos_ux_context.hslider3_current = 0;
                    G_bolos_ux_context.hslider3_after = 1;
                    break;
            }
            break;
        default:
            G_bolos_ux_context.hslider3_after =
                G_bolos_ux_context.hslider3_current;
            G_bolos_ux_context.hslider3_current =
                G_bolos_ux_context.hslider3_before;
            G_bolos_ux_context.hslider3_before =
                (G_bolos_ux_context.hslider3_before +
                 G_bolos_ux_context.hslider3_total - 1) %
                G_bolos_ux_context.hslider3_total;
            break;
    }
}

UX_STEP_CB(ux_restore_step_1, nn,
           screen_onboarding_restore_word_display_auto_complete();
           , {UI_STR_BAGL_ENTER_LABEL, G_ux.string_buffer});

UX_FLOW(ux_restore_flow, &ux_restore_step_1);

UX_STEP_CB(ux_quit_step, pb, clean_exit(0), {&C_icon_dashboard_x, UI_STR_QUIT});
UX_STEP_VALID(ux_return_step, pb, ui_idle_init(),
              {&C_icon_back_x, UI_STR_BAGL_RETURN_TO_MENU});
UX_STEP_NOCB(ux_invalid_step_2, nn,
             {
                 UI_STR_BAGL_INVALID_ADVICE_L1,
                 UI_STR_BAGL_INVALID_ADVICE_L2,
             });

UX_STEP_NOCB(ux_bip39_invalid_step_1, pbb,
             {&C_icon_crossmark, UI_STR_BAGL_BIP39_PHRASE_TITLE,
              UI_STR_BAGL_BIP39_INVALID_TITLE_L2});
UX_STEP_VALID(ux_bip39_invalid_step_3, pb,
              screen_onboarding_bip39_restore_init();
              , {&C_icon_back_x, UI_STR_BAGL_BIP39_REENTER_PHRASE});

UX_FLOW(ux_bip39_invalid_flow, &ux_bip39_invalid_step_1, &ux_invalid_step_2,
        &ux_bip39_invalid_step_3, &ux_return_step);

UX_STEP_NOCB(ux_bip39_nomatch_step_1, pbb,
             {&C_icon_warning, UI_STR_BAGL_BIP39_PHRASE_TITLE,
              UI_STR_BAGL_BIP39_NOMATCH_TITLE_L2});

// What the mismatch means when the phrase was on its way to being split. The
// title above answers "is this my phrase?"; this answers "can I back it up?",
// which is the question that was actually asked, and the two are not the same
// answer. Its own step for the same reason ux_invalid_step_2 is one: a pbb
// title has two short lines and no room for a sentence.
UX_STEP_NOCB(ux_backup_nomatch_step_2, nn,
             {
                 UI_STR_BAGL_BACKUP_NOMATCH_L1,
                 UI_STR_BAGL_BACKUP_NOMATCH_L2,
             });

UX_FLOW(ux_bip39_check_nomatch_flow, &ux_bip39_nomatch_step_1, &ux_return_step);
UX_FLOW(ux_bip39_backup_nomatch_flow, &ux_bip39_nomatch_step_1,
        &ux_backup_nomatch_step_2, &ux_return_step);

UX_STEP_NOCB(ux_bip39_match_step_1, pbb,
             {&C_icon_validate_14, UI_STR_BAGL_BIP39_PHRASE_TITLE,
              UI_STR_BAGL_BIP39_MATCH_TITLE_L2});
UX_STEP_CB(ux_bip39_recover_step_1, pbb, set_sskr_descriptor_values();
           , {&SSKR_ICON, UI_STR_BAGL_GENERATE_SSKR_L1,
              UI_STR_BAGL_GENERATE_SSKR_L2});

// Checking ends on the verdict; that is what makes it a destination. The step
// offering to generate shares is gone from this flow -- it was the only way
// in before, and it is now an entry of the idle menu, where it says what it
// is. ux_return_step takes its place so that the flow still has a way back to
// that menu rather than only a way out of the application.
UX_FLOW(ux_bip39_check_match_flow, &ux_bip39_match_step_1, &ux_quit_step,
        &ux_return_step);

// Splitting passes through it: verdict, a way out, the press that starts the
// generation, and a way back to the menu.
//
// The first three steps are what this file already had. The fourth is new,
// and it is the same step the check flow above gained: without it the only
// exit from a successful backup verdict is ux_quit_step, which erases but
// closes the application, so someone who changed their mind had to leave
// rather than go back. That was true before this change too; it is worth
// less defending than fixing.
UX_FLOW(ux_bip39_backup_match_flow, &ux_bip39_match_step_1, &ux_quit_step,
        &ux_bip39_recover_step_1, &ux_return_step);

/*
 * The same guarantee the touch stack gets from its tables, stated where a
 * Nano build will see it.
 *
 * src/nbgl/ui.c is not compiled into any Nano target, so its static
 * assertions say nothing here: a fifth intention added for the touch menu
 * breaks that build and leaves this one silent, and the two functions below
 * would send the new value down the check flow through their `else` -- the
 * safe answer, arrived at by accident.
 */
_Static_assert(USER_INTENT_NB == 4,
               "the two functions below choose a flow on the intention, and "
               "everything they do not name takes the check flow; a new "
               "intention needs a decision here, not an else branch");

void ux_bip39_match_display(void) {
    if (G_bolos_ux_context.user_intent == USER_INTENT_BACKUP) {
        ux_flow_init(0, ux_bip39_backup_match_flow, NULL);
    } else {
        ux_flow_init(0, ux_bip39_check_match_flow, NULL);
    }
}

void ux_bip39_nomatch_display(void) {
    if (G_bolos_ux_context.user_intent == USER_INTENT_BACKUP) {
        ux_flow_init(0, ux_bip39_backup_nomatch_flow, NULL);
    } else {
        ux_flow_init(0, ux_bip39_check_nomatch_flow, NULL);
    }
}

UX_STEP_NOCB(ux_sskr_invalid_step_1, pbb,
             {&C_icon_crossmark, UI_STR_BAGL_SSKR_SHARES_TITLE,
              UI_STR_BAGL_SSKR_INVALID_TITLE_L2});
UX_STEP_VALID(ux_sskr_invalid_step_3, pb, screen_onboarding_sskr_restore_init();
              , {&C_icon_back_x, UI_STR_BAGL_SSKR_REENTER_SHARES});

UX_FLOW(ux_sskr_invalid_flow, &ux_sskr_invalid_step_1, &ux_invalid_step_2,
        &ux_sskr_invalid_step_3, &ux_return_step);

UX_STEP_NOCB(ux_sskr_nomatch_step_1, pbb,
             {&C_icon_warning, UI_STR_BAGL_SSKR_SHARES_TITLE,
              UI_STR_BAGL_SSKR_NOMATCH_TITLE_L2});

UX_STEP_NOCB(ux_sskr_match_step_1, pbb,
             {&C_icon_validate_14, UI_STR_BAGL_SSKR_SHARES_TITLE,
              UI_STR_BAGL_SSKR_MATCH_TITLE_L2});

UX_STEP_CB(ux_sskr_recover_step_1, pbb, recover_bip39();
           , {&BIP39_ICON, UI_STR_BAGL_RECOVER_BIP39_L1,
              UI_STR_BAGL_RECOVER_BIP39_L2});

/*
 * What stands in front of the rebuilt phrase, on both verdicts.
 *
 * Three steps, and the third is the reason the other two are not enough. This
 * flow reveals what the entered shares rebuild whether or not it matches the
 * seed this Ledger holds: ux_sskr_nomatch_flow reaches ux_sskr_recover_step_1
 * exactly as ux_sskr_match_flow does, and the touch stack gates the same
 * screen on sskr_shares_check() rather than on seed_match.
 *
 * That is deliberate, and it is what a backup is for -- rebuilding your phrase
 * from your own shares onto a spare or replacement device is precisely the
 * case where the device does not already hold it. Requiring a match would
 * remove the feature on the day it is needed. What was missing was any screen
 * saying so, on either stack. UI_STR_BAGL_RECOVER_WARN_L5/L6 say it.
 *
 * Placed after ux_quit_step so that the way out is read before the warning
 * rather than after it, and before ux_sskr_recover_step_1 so that no ordering
 * of presses reaches the reveal without passing them.
 */
UX_STEP_NOCB(ux_sskr_reveal_warn_step_1, pbb,
             {&C_icon_warning, UI_STR_BAGL_RECOVER_WARN_L1,
              UI_STR_BAGL_RECOVER_WARN_L2});

UX_STEP_NOCB(ux_sskr_reveal_warn_step_2, nn,
             {
                 UI_STR_BAGL_SCREEN_PRIVACY_L1,
                 UI_STR_BAGL_SCREEN_PRIVACY_L2,
             });

UX_STEP_NOCB(ux_sskr_reveal_warn_step_3, nn,
             {
                 UI_STR_BAGL_RECOVER_WARN_L5,
                 UI_STR_BAGL_RECOVER_WARN_L6,
             });

UX_FLOW(ux_sskr_nomatch_flow, &ux_sskr_nomatch_step_1, &ux_quit_step,
        &ux_sskr_reveal_warn_step_1, &ux_sskr_reveal_warn_step_2,
        &ux_sskr_reveal_warn_step_3, &ux_sskr_recover_step_1);

UX_FLOW(ux_sskr_match_flow, &ux_sskr_match_step_1, &ux_quit_step,
        &ux_sskr_reveal_warn_step_1, &ux_sskr_reveal_warn_step_2,
        &ux_sskr_reveal_warn_step_3, &ux_sskr_recover_step_1);
#endif  // defined(HAVE_BAGL)
