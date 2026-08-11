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

#if defined(HAVE_BAGL)

#include "common/ui_strings.h"

UX_STEP_NOCB(step_display_bip39, bnnn_paging,
             {
                 .title = UI_STR_BAGL_BIP39_PHRASE_TITLE,
                 .text = G_bolos_ux_context.words_buffer,
             });

UX_STEP_CB(step_bip39_clean_exit, pb, clean_exit(0),
           {&C_icon_dashboard_x, UI_STR_QUIT});

UX_FLOW(display_bip39_flow, &step_display_bip39, &step_bip39_clean_exit,
        FLOW_LOOP);

void recover_bip39(void) {
    // BIP39 phrase should already be in G_bolos_ux_context.words_buffer so just
    // need to display it
    ux_flow_init(0, display_bip39_flow, NULL);
}

#endif  // defined(HAVE_BAGL)
