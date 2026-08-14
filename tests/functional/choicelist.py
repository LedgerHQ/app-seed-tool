"""Naming the entries of the screens that ask "which one?".

Four screens ask it -- the menu of intentions, the BIP-39 phrase length, the
PIN length, and the list of BIP-85 secrets -- and all four are the same
component: a BARS_LIST drawn by nbgl_useCaseGenericConfiguration(), through
display_choice_list() in src/nbgl/ui.c. One component, one driver: ragger's
own ChoiceList, whose positions count from the *top* of the screen, as the
list itself reads.

There is nothing here but names. Three of these screens used to be hand-built
stacks of nbgl_button_t, which is why this file used to carry a table of touch
coordinates counted from the *bottom* -- the stack grew upwards from the
bottom margin, so its first entry was the last line on screen. Nothing counts
backwards any more, and nothing here has to say where a row is: ragger knows.
The back arrow is gone from here too; it is the SDK's header arrow now, driven
by UseCaseSubSettings(...).exit(), and not a square this application places
itself.

Naming an entry still does not verify it. A caller waits for a label to be
somewhere on screen and then touches a coordinate, so a constant shifted by
one row would satisfy the wait, touch the wrong entry, and fail several
screens later with an unrelated message. What holds the mapping is
test_menu_positions.py::test_each_entry_opens_what_it_names, which touches
each constant and asserts the screen it arrives on.
"""

# The four intentions, in the order the menu draws them -- which is the order
# of select_menu_entries[] in src/nbgl/ui.c, and the order they are read in.
MENU_CHECK = 1
MENU_BACKUP = 2
MENU_RECOVER = 3
MENU_DERIVE = 4

# The four secrets, in the order of `enum bip85_app_type` in
# src/nbgl/bip85_app.h -- which is the order the screen lists them in and the
# order the review reads its Application row from.
BIP85_APP_BIP39 = 1
BIP85_APP_PWD_BASE64 = 2
BIP85_APP_PWD_BASE85 = 3
BIP85_APP_PIN = 4

# The three phrase lengths and the three PIN lengths, ascending, which is the
# order both lists draw them in. These two happen to have kept the numbers
# they had as button stacks: the stack was written 12, 18, 24 and drawn
# bottom-up, so the shortest was already row 1 counting from the bottom -- and
# is row 1 counting from the top now. The names are here so that no caller has
# to know that coincidence held.
WORDS_12 = 1
WORDS_18 = 2
WORDS_24 = 3

DIGITS_4 = 1
DIGITS_6 = 2
DIGITS_8 = 3
