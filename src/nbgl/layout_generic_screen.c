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

#include "glyphs.h"

#if defined(SCREEN_SIZE_WALLET)

#include <nbgl_obj.h>

#define UPPER_MARGIN 4
#define ICON_X 0
#define ICON_Y 148

nbgl_image_t* generic_screen_set_icon(const nbgl_icon_details_t* icon) {
    nbgl_image_t* image = (nbgl_image_t*)nbgl_objPoolGet(IMAGE, 0);
    image->foregroundColor = BLACK;
    image->buffer = icon;
    image->obj.area.bpp = NBGL_BPP_1;
#if defined(TARGET_STAX)
    uint8_t divide = 1;
#elif defined(TARGET_FLEX) || defined(TARGET_APEX_P)
    uint8_t divide = 2;
#endif
    image->obj.alignmentMarginX = ICON_X / divide;
    image->obj.alignmentMarginY = ICON_Y / divide;
    image->obj.alignment = TOP_MIDDLE;
    image->obj.alignTo = NULL;
    return image;
}

nbgl_text_area_t* generic_screen_set_title(nbgl_obj_t* align_to) {
    nbgl_text_area_t* textArea =
        (nbgl_text_area_t*)nbgl_objPoolGet(TEXT_AREA, 0);
    textArea->textColor = BLACK;
    textArea->text = "";
    textArea->textAlignment = CENTER;
    textArea->fontId = LARGE_MEDIUM_FONT;
    textArea->obj.area.width = SCREEN_WIDTH - 2 * BORDER_MARGIN;
    textArea->obj.area.height =
        nbgl_getTextHeight(textArea->fontId, textArea->text);
    textArea->style = NO_STYLE;
    textArea->obj.alignment = BOTTOM_MIDDLE;
    textArea->obj.alignTo = align_to;
    textArea->obj.alignmentMarginX = 0;
    textArea->obj.alignmentMarginY = BORDER_MARGIN;
    return textArea;
}

/*
 * A title for a screen that has no icon to hang it from.
 *
 * generic_screen_set_title() above aligns BOTTOM_MIDDLE to the icon it is
 * given; with no icon there is nothing to align to, and BOTTOM_MIDDLE against
 * a NULL alignTo is the bottom of the screen -- under the buttons. This one
 * hangs from the top instead, clear of the back button, whose height the SDK
 * names (BACK_BUTTON_HEADER_HEIGHT: 88 on Stax, 96 on Flex, 60 on Apex).
 *
 * `wrapping` is left unset here, as generic_screen_set_title() above also
 * leaves it. That is a decision and not an oversight, and it is why every
 * title in this file carries a hand-placed "\n": with the field clear the
 * text area breaks on characters, so a title that overflows is cut mid-word;
 * setting it would break on words instead. Placed breaks are kept because
 * these titles are short and their two halves are chosen, not discovered --
 * but a caller that reads "the area wraps on characters" should know it is
 * reading this line, not a property of the widget.
 */
nbgl_text_area_t* generic_screen_set_top_title(void) {
    nbgl_text_area_t* textArea =
        (nbgl_text_area_t*)nbgl_objPoolGet(TEXT_AREA, 0);
    textArea->textColor = BLACK;
    textArea->text = "";
    textArea->textAlignment = CENTER;
    textArea->fontId = LARGE_MEDIUM_FONT;
    textArea->obj.area.width = SCREEN_WIDTH - 2 * BORDER_MARGIN;
    textArea->obj.area.height =
        nbgl_getTextHeight(textArea->fontId, textArea->text);
    textArea->style = NO_STYLE;
    textArea->obj.alignment = TOP_MIDDLE;
    textArea->obj.alignTo = NULL;
    textArea->obj.alignmentMarginX = 0;
    textArea->obj.alignmentMarginY = BACK_BUTTON_HEADER_HEIGHT;
    return textArea;
}

void generic_screen_configure_buttons(nbgl_button_t** buttons,
                                      const size_t size) {
    nbgl_button_t* button;
    for (size_t i = 0; i < size; i++) {
        button = buttons[i];
        button->innerColor = WHITE;
        button->borderColor = LIGHT_GRAY;
        button->foregroundColor = BLACK;
        button->obj.area.width = SCREEN_WIDTH - 2 * BORDER_MARGIN;
        button->obj.area.height = BUTTON_DIAMETER;
        button->radius = BUTTON_RADIUS;
        button->fontId = SMALL_BOLD_1BPP_FONT;
        button->icon = NULL;
        button->obj.alignmentMarginX = 0;
        button->obj.alignmentMarginY =
            (button->obj.area.height + 8) * i + BORDER_MARGIN;
        button->obj.alignment = BOTTOM_MIDDLE;
        button->obj.alignTo = NULL;
        button->obj.touchMask = (1 << TOUCHED);
    }
}

nbgl_button_t* generic_screen_set_back_button() {
    nbgl_button_t* button = (nbgl_button_t*)nbgl_objPoolGet(BUTTON, 0);
    button->innerColor = WHITE;
    button->borderColor = WHITE;
    button->foregroundColor = BLACK;
    button->obj.area.width = BUTTON_DIAMETER;
    button->obj.area.height = BUTTON_DIAMETER;
    button->radius = BUTTON_RADIUS;
    button->text = NULL;
    button->icon = &LEFT_ARROW_ICON;
    button->obj.alignmentMarginX = 0;
    button->obj.alignmentMarginY = UPPER_MARGIN;
    button->obj.alignment = TOP_LEFT;
    button->obj.alignTo = NULL;
    button->obj.touchMask = (1 << TOUCHED);
    return button;
}

#endif
