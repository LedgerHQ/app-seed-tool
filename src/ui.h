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

#include <os.h>

#if defined(HAVE_BAGL)

#include "bagl/ux_nano.h"

#endif  // defined(HAVE_BAGL)

#if defined(TARGET_STAX)
#define ICON_APP_HOME    C_icon_seed_64px
#define BIP39_ICON       C_icon_bip39_64px
#define SSKR_ICON        C_icon_sskr_64px
#define BIP85_ICON       C_icon_bip85_64px
#define BIP39_ICON_SMALL C_icon_bip39_32px
#define SSKR_ICON_SMALL  C_icon_sskr_32px
#define BIP85_ICON_SMALL C_icon_bip85_32px
#define PRIVACY_ICON     C_Privacy_32px
#define INFO_ICON        C_Info_32px
#define CHECKED_ICON     C_Check_Circle_32px
#elif defined(TARGET_FLEX)
#define ICON_APP_HOME    C_icon_seed_64px
#define BIP39_ICON       C_icon_bip39_64px
#define SSKR_ICON        C_icon_sskr_64px
#define BIP85_ICON       C_icon_bip85_64px
#define BIP39_ICON_SMALL C_icon_bip39_40px
#define SSKR_ICON_SMALL  C_icon_sskr_40px
#define BIP85_ICON_SMALL C_icon_bip85_40px
#define PRIVACY_ICON     C_Privacy_40px
#define INFO_ICON        C_Info_40px
#define CHECKED_ICON     C_Check_Circle_40px
#elif defined(TARGET_APEX)
#define ICON_APP_HOME    C_icon_seed_48px
#define BIP39_ICON       C_icon_bip39_32px
#define SSKR_ICON        C_icon_sskr_32px
#define BIP85_ICON       C_icon_bip85_32px
#define BIP39_ICON_SMALL C_icon_bip39_24px
#define SSKR_ICON_SMALL  C_icon_sskr_24px
#define BIP85_ICON_SMALL C_icon_bip85_24px
/*
 * The SDK glyphs the explanation rows use, sized alongside the *_SMALL family
 * above.
 *
 * A previous version of this comment claimed lib_nbgl had no check mark at row
 * size, "C_Check_Circle only at 48px and 64px". That was false, and it was
 * written from a single compiler error rather than from the glyph tables: what
 * is 48/64-only is the SDK's size-agnostic CHECK_CIRCLE_ICON macro. The symbols
 * themselves are generated per target by Makefile.glyphs -- 32px for Stax, 40px
 * for Flex, 24px for Apex -- and each appears in that target's own
 * build/<target>/gen_src/glyphs.h. The lesson is the same one PIC() taught this
 * file: a compiler error on one target is not a fact about the SDK.
 */
#define PRIVACY_ICON     C_Privacy_24px
#define INFO_ICON        C_Info_24px
#define CHECKED_ICON     C_Check_Circle_24px
#endif

// All devices
void ui_idle_init(void);
