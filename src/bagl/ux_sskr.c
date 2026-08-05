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

#include <os_io_seproxyhal.h>

#include "common/sskr/common_sskr.h"
#include "constants.h"
#include "ui.h"

#if defined(HAVE_BAGL)

#include "./ux_sskr_menu.h"
#include "common/ui_strings.h"

bool get_next_data(bool share_step) {
    size_t offset;
    size_t length;

    if (G_bolos_ux_context.sskr_share_index >= 1 &&
        bolos_ux_sskr_share_slice(G_bolos_ux_context.sskr_words_buffer_length,
                                  G_bolos_ux_context.sskr_share_count,
                                  G_bolos_ux_context.sskr_share_index - 1,
                                  &offset, &length)) {
        SPRINTF(G_bolos_ux_context.string_buffer, UI_STR_BAGL_SSKR_SHARE_HEADER,
                G_bolos_ux_context.sskr_share_index,
                G_bolos_ux_context.sskr_share_count);
        memcpy(G_bolos_ux_context.words_buffer,
               G_bolos_ux_context.sskr_words_buffer + offset, length);

        G_bolos_ux_context.sskr_share_index += share_step ? 1 : -1;
        return true;
    }
    G_bolos_ux_context.sskr_share_index =
        (G_bolos_ux_context.sskr_share_index < 1) ? 1
        : (G_bolos_ux_context.sskr_share_index >
           G_bolos_ux_context.sskr_share_count)
            ? G_bolos_ux_context.sskr_share_count
            : G_bolos_ux_context.sskr_share_index;

    return false;
}

void bnnn_paging_edgecase(void) {
    G_ux.flow_stack[G_ux.stack_count - 1].prev_index =
        G_ux.flow_stack[G_ux.stack_count - 1].index - 2;
    G_ux.flow_stack[G_ux.stack_count - 1].index--;
    ux_flow_relayout();
}

void display_next_state(bool is_upper_delimiter) {
    if (is_upper_delimiter) {  // We're called from the upper delimiter.
        if (G_bolos_ux_context.current_state == STATIC_SCREEN) {
            bool dynamic_data = get_next_data(true);
            if (dynamic_data) {
                G_bolos_ux_context.current_state = DYNAMIC_SCREEN;
            }
            ux_flow_next();
        } else {
            bool dynamic_data = get_next_data(false);
            if (dynamic_data) {
                ux_flow_next();
            } else {
                G_bolos_ux_context.current_state = STATIC_SCREEN;
                ux_flow_prev();
            }
        }
    } else {
        if (G_bolos_ux_context.current_state == STATIC_SCREEN) {
            bool dynamic_data = get_next_data(false);
            if (dynamic_data) {
                // We found some data to display so enter in dynamic mode.
                G_bolos_ux_context.current_state = DYNAMIC_SCREEN;
            }
            ux_flow_prev();
        } else {
            bool dynamic_data = get_next_data(true);
            if (dynamic_data) {
                // Similar to `ux_flow_prev()` but updates layout to account for
                // `bnnn_paging`'s weird behaviour.
                bnnn_paging_edgecase();
            } else {
                G_bolos_ux_context.current_state = STATIC_SCREEN;
                ux_flow_next();
            }
        }
    }
}

UX_STEP_INIT(step_upper_delimiter, NULL, NULL, { display_next_state(true); });

UX_STEP_NOCB(step_display_shares, bnnn_paging,
             {
                 .title = G_bolos_ux_context.string_buffer,
                 .text = G_bolos_ux_context.words_buffer,
             });

UX_STEP_INIT(step_lower_delimiter, NULL, NULL, { display_next_state(false); });

UX_STEP_CB(step_sskr_clean_exit, pb, clean_exit(0),
           {&C_icon_dashboard_x, UI_STR_QUIT});

UX_FLOW(dynamic_flow, &step_upper_delimiter, &step_display_shares,
        &step_lower_delimiter, &step_sskr_clean_exit, FLOW_LOOP);

void generate_sskr(void) {
#if defined(TARGET_NANOS)
    G_bolos_ux_context.processing = PROCESSING_COMPLETE;
    io_seproxyhal_general_status();
#endif

    PRINTF("SSKR threshold selected: %d\n",
           G_bolos_ux_context.sskr_group_descriptor[0][0]);
    PRINTF("SSKR share count selected: %d\n",
           G_bolos_ux_context.sskr_group_descriptor[0][1]);

    G_bolos_ux_context.sskr_share_count = 0;
    G_bolos_ux_context.sskr_words_buffer_length = 0;

    bolos_ux_bip39_to_sskr_convert(
        (unsigned char*)G_bolos_ux_context.words_buffer,
        G_bolos_ux_context.words_buffer_length, G_bolos_ux_context.bip39_type,
        G_bolos_ux_context.sskr_group_descriptor[0],
        &G_bolos_ux_context.sskr_share_count,
        (unsigned char*)G_bolos_ux_context.sskr_words_buffer,
        &G_bolos_ux_context.sskr_words_buffer_length);

    if (G_bolos_ux_context.sskr_share_count > 0) {
        PRINTF("SSKR share_count from generate_sskr(): %d\n",
               G_bolos_ux_context.sskr_share_count);
        for (uint8_t share = 0; share < G_bolos_ux_context.sskr_share_count;
             share++) {
            // A length, never the share itself. Any threshold of these
            // reconstructs the seed, and on the Nano targets PRINTF leaves
            // over SEPROXYHAL, which is whatever host the device is plugged
            // into. The same shape as sskr_shares_from_bip39_mnemonic()
            // (nbgl/sskr_shares.c), with %u rather than %zu because the two
            // operands here are an unsigned int and a uint8_t, not size_t.
            PRINTF("SSKR share %d:\n", share + 1);
            PRINTF("(%u bytes)\n", G_bolos_ux_context.sskr_words_buffer_length /
                                       G_bolos_ux_context.sskr_share_count);
        }
    }
    G_bolos_ux_context.sskr_share_index = 1;
    ux_flow_init(0, dynamic_flow, NULL);
}

UX_STEP_NOCB(ux_threshold_warn_step_1, pnn,
             {
                 &C_icon_warning,
                 UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L1,
                 UI_STR_BAGL_SSKR_ONE_OF_M_WARN_L2,
             });

UX_STEP_NOCB(ux_threshold_warn_step_2, pbb,
             {
                 &C_icon_warning,
                 UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L1,
                 UI_STR_BAGL_SSKR_ONE_OF_M_NOT_L2,
             });

UX_FLOW(ux_threshold_warn_flow, &ux_threshold_warn_step_1,
        &ux_threshold_warn_step_2, &step_sskr_clean_exit);

const char* sskr_threshold_getter(unsigned int idx) {
    // Bounded by the share count the user just chose, which is what keeps a
    // threshold from ever exceeding the number of shares it would be taken
    // out of.
    return sskr_descriptor_label(
        idx, G_bolos_ux_context.sskr_group_descriptor[0][1]);
}

void sskr_threshold_selector(unsigned int idx) {
    G_bolos_ux_context.sskr_group_descriptor[0][0] = idx + 1;

    if (G_bolos_ux_context.sskr_group_descriptor[0][0] == 1 &&
        G_bolos_ux_context.sskr_group_descriptor[0][1] > 1) {
        ux_flow_init(0, ux_threshold_warn_flow, NULL);
    } else {
#if defined(TARGET_NANOS)
        // Display processing warning to user
        screen_processing_init();
        G_bolos_ux_context.processing = PROCESSING_GENERATE_SSKR;
#else
        generate_sskr();
#endif
    }
}

UX_STEP_NOCB(ux_threshold_instruction_step, nn,
             {UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L1,
              UI_STR_BAGL_SSKR_THRESHOLD_TITLE_L2});

UX_STEP_MENULIST(ux_threshold_menu_step, sskr_threshold_getter,
                 sskr_threshold_selector);

UX_FLOW(ux_threshold_flow, &ux_threshold_instruction_step,
        &ux_threshold_menu_step);

const char* sskr_shares_number_getter(unsigned int idx) {
    return sskr_descriptor_label(idx, sskr_descriptor_count());
}

void sskr_shares_number_selector(unsigned int idx) {
    G_bolos_ux_context.sskr_group_descriptor[0][1] = idx + 1;
    ux_flow_init(0, ux_threshold_flow, NULL);
}

UX_STEP_NOCB(ux_shares_number_instruction_step, nn,
             {UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L1,
              UI_STR_BAGL_SSKR_NUMSHARES_TITLE_L2});

UX_STEP_MENULIST(ux_shares_number_menu_step, sskr_shares_number_getter,
                 sskr_shares_number_selector);

UX_FLOW(ux_shares_number_flow, &ux_shares_number_instruction_step,
        &ux_shares_number_menu_step);

void set_sskr_descriptor_values(void) {
    ux_flow_init(0, ux_shares_number_flow, NULL);
}

#endif  // defined(HAVE_BAGL)
