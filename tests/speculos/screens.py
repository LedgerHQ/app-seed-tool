"""Where to touch, on flex, for the screens these scripts walk through.

Every script in this directory drives the application by coordinate, because
none of them goes through Ragger -- the GDB/breakpoint machinery has nothing to
do with Ragger's navigator, and mixing the two would add coupling for no
benefit (see README.md, "Adapting to stax/apex").

Driving by coordinate has a cost, and this file is the answer to it. Each of
the six scripts used to carry its own copy of these numbers, so when the menus
changed the six went stale at once and nothing noticed: these scripts are run
by hand, never in CI. One copy means the next change to a screen is one edit
here, and a wrong row shows up in every script at the same time rather than in
whichever one someone happens to run.

flex only (480x600). See README.md for what it takes to add stax or apex.
"""

# --- Lists ---------------------------------------------------------------
#
# The four screens that ask "which one?" are one component: a BARS_LIST drawn
# by nbgl_useCaseGenericConfiguration(), through display_choice_list() in
# src/nbgl/ui.c. Its rows are a fixed pitch from the top of the list, which is
# what makes a row number enough to touch one -- and why these scripts no
# longer count from the bottom, as they did when three of these screens were
# hand-built stacks of nbgl_button_t.
#
# Measured off the separators of a rendered list: they fall at y = 95, 187,
# 279, 371 and 463, so a row is 92 high and the first one is centred at 141.
LIST_FIRST_ROW_Y = 141
LIST_ROW_PITCH = 92


def list_row(n):
    """Centre of the n-th row of a choice list, counted from the top, 1-based.

    The row *numbers* are the ones named in tests/functional/choicelist.py --
    same screens, same order, same source (the entry arrays in src/nbgl/ui.c).
    Kept as numbers rather than labels because these scripts touch a point;
    what holds the mapping from number to entry is
    test_menu_positions.py::test_each_entry_opens_what_it_names, which touches
    each one and asserts the screen it arrives on.
    """
    return (240, LIST_FIRST_ROW_Y + LIST_ROW_PITCH * (n - 1))


# The menu of intentions, in the order select_menu_entries[] draws them.
MENU_CHECK = 1
MENU_BACKUP = 2
MENU_RECOVER = 3
MENU_DERIVE = 4

# The BIP-85 secrets, in the order of `enum bip85_app_type`.
BIP85_APP_BIP39 = 1
BIP85_APP_PWD_BASE64 = 2
BIP85_APP_PWD_BASE85 = 3
BIP85_APP_PIN = 4

# Phrase lengths and PIN lengths, ascending, as both lists draw them.
WORDS_12 = 1
WORDS_18 = 2
WORDS_24 = 3

DIGITS_4 = 1
DIGITS_6 = 2
DIGITS_8 = 3

# --- Fixed points --------------------------------------------------------

# The home screen's action button.
HOME_ACTION = (240, 435)

# The SDK's header back arrow. Not a square this application places itself any
# more -- generic_screen_set_back_button() is gone, and this is the band
# UseCaseSubSettings(...).exit() drives.
BACK = (48, 48)

# An explanation screen ends one of two ways, and the difference is deliberate
# (see tests/functional/explanations.py). A screen that only leads to more
# reading has no button: the whole content is tappable and a grey "Continue"
# names the gesture. A screen that leads to an act -- entering a Phrase,
# entering Shares -- keeps the black button.
CONTINUE_FOOTER = (240, 550)
BLACK_BUTTON = (240, 530)

# Result screens: "Tap to dismiss" / "Tap to continue".
RESULT_FOOTER = (240, 550)

# The review before generating or deriving. Two pages: the values, then the
# warning and the long-press button.
REVIEW_NEXT = (430, 550)
REVIEW_CANCEL = (95, 550)
REVIEW_LONG_PRESS = (404, 428)
REVIEW_LONG_PRESS_SECONDS = 3.2

# The paged display of a generated or derived secret.
SECRET_CLOSE = (95, 550)
SECRET_NEXT = (430, 550)

# --- Keyboards -----------------------------------------------------------

# POSITIONS["LetterOnlyKeyboard"][DeviceType.FLEX] in Ledger's own ragger
# package (ragger/firmware/touch/positions.py).
LETTERS = {
    "q": (24, 415), "w": (72, 415), "e": (120, 415), "r": (168, 415),
    "t": (216, 415), "y": (264, 415), "u": (312, 415), "i": (360, 415),
    "o": (408, 415), "p": (456, 415),
    "a": (48, 490), "s": (96, 490), "d": (144, 490), "f": (192, 490),
    "g": (240, 490), "h": (288, 490), "j": (336, 490), "k": (384, 490),
    "l": (432, 490),
    "z": (24, 565), "x": (72, 565), "c": (120, 565), "v": (168, 565),
    "b": (216, 565), "n": (264, 565), "m": (312, 565),
}
SUGGESTION_1 = (140, 300)

# KEYPAD_POSITIONS["Keypad"][DeviceType.FLEX] in tests/functional/keypad.py,
# which is the one Ragger-side helper in this repository that still carries a
# coordinate table.
KEYPAD = {
    "1": (80, 292), "2": (240, 292), "3": (400, 292),
    "4": (80, 380), "5": (240, 380), "6": (400, 380),
    "7": (80, 468), "8": (240, 468), "9": (400, 468),
    "delete": (80, 556), "0": (240, 556), "enter": (400, 556),
}
