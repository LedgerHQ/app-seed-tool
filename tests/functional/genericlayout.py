import logging
from ledgered.devices import DeviceType
from ragger.firmware.touch.positions import POSITIONS, Position, STAX_X_CENTER, FLEX_X_CENTER, APEX_P_X_CENTER
from ragger.firmware.touch.element import Element

# Positions are counted from the *bottom* of the screen, because that is the
# order generic_screen_configure_buttons() (src/nbgl/layout_generic_screen.c)
# lays the buttons out in: child i sits (BUTTON_DIAMETER + 8) * i above the
# bottom margin, so child 0 is the lowest one on screen and the last one read.
#
# The fourth entry is new with the four-intention menu. Its coordinate is the
# same arithmetic the three above already follow -- one button pitch higher --
# and it was checked against what Speculos actually draws rather than assumed:
# the buttons report y=153/217/281/345 on apex_p, 218/314/410/506 on flex.
GENERIC_LAYOUT_POSITIONS = {
    "GenericLayout": {
        DeviceType.STAX: {
            # Up to 4 choices in a list, lowest first
            1: Position(STAX_X_CENTER, 620),
            2: Position(STAX_X_CENTER, 520),
            3: Position(STAX_X_CENTER, 420),
            4: Position(STAX_X_CENTER, 340)
        },
        DeviceType.FLEX: {
            # Up to 4 choices in a list, lowest first
            1: Position(FLEX_X_CENTER, 540),
            2: Position(FLEX_X_CENTER, 430),
            3: Position(FLEX_X_CENTER, 320),
            4: Position(FLEX_X_CENTER, 236)
        },
        DeviceType.APEX_P: {
            # Up to 4 choices in a list, lowest first
            1: Position(APEX_P_X_CENTER, 350),
            2: Position(APEX_P_X_CENTER, 290),
            3: Position(APEX_P_X_CENTER, 230),
            4: Position(APEX_P_X_CENTER, 164)
        }
    }
}

POSITIONS.update(GENERIC_LAYOUT_POSITIONS)

# The four entries of the menu, in the order they are read from the top, and
# the position each one occupies counting from the bottom.
#
# Written out rather than computed, and named rather than numbered. The screen
# and the layout disagree about which end of the list comes first, so a bare
# choose(2) on this screen is a claim about the layout that nothing in the
# test would check; MENU_BACKUP at least names the entry it means.
#
# Naming it does not verify it. A caller waits for the label to be somewhere
# on screen and then touches a coordinate, so a table shifted by one row would
# satisfy the wait, touch the wrong entry, and fail several screens later with
# an unrelated message. What holds the mapping is
# test_menu_positions.py::test_each_entry_opens_what_it_names, which touches
# each constant and asserts the screen it arrives on.
MENU_CHECK = 4
MENU_BACKUP = 3
MENU_RECOVER = 2
MENU_DERIVE = 1


class GenericLayout(Element):

    def choose(self, index: int):
        """Touch the index-th button counting up from the bottom of the list."""
        assert 1 <= index <= 4, "Choice index must be in [1, 4]"
        self.client.finger_touch(*self.positions[index])
