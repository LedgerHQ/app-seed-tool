/*******************************************************************************
 *   Ledger Seed Tool application
 *   (c) 2016-2025 Ledger SAS
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

#if defined(SCREEN_SIZE_WALLET)

nbgl_image_t *generic_screen_set_icon(const nbgl_icon_details_t *icon);
nbgl_text_area_t *generic_screen_set_title(nbgl_obj_t *align_to);
void generic_screen_configure_buttons(nbgl_button_t **buttons, const size_t size);
nbgl_button_t *generic_screen_set_back_button();

#endif
