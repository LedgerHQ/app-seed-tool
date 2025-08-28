import logging
from ledgered.devices import DeviceType
from ragger.firmware.touch.positions import POSITIONS, Position, STAX_X_CENTER, FLEX_X_CENTER
from ragger.firmware.touch.element import Element

GENERIC_LAYOUT_POSITIONS = {
    "GenericLayout": {
        DeviceType.STAX: {
            # Up to 3 choices in a list
            1: Position(STAX_X_CENTER, 620),
            2: Position(STAX_X_CENTER, 520),
            3: Position(STAX_X_CENTER, 420)
        },
        DeviceType.FLEX: {
            # Up to 3 choices in a list
            1: Position(FLEX_X_CENTER, 540),
            2: Position(FLEX_X_CENTER, 430),
            3: Position(FLEX_X_CENTER, 320)
        }
    }
}

POSITIONS.update(GENERIC_LAYOUT_POSITIONS)

class GenericLayout(Element):

    def choose(self, index: int):
        assert 1 <= index <= 3, "Choice index must be in [1, 3]"
        self.client.finger_touch(*self.positions[index])
