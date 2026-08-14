import logging
from ledgered.devices import DeviceType
from ragger.firmware.touch.positions import POSITIONS, Position
from ragger.firmware.touch.element import Element

KEYPAD_POSITIONS = {
    "Keypad": {
        DeviceType.STAX: {
            # first line
            "1": Position(67, 308),
            "2": Position(200, 308),
            "3": Position(333, 308),
            # second line
            "4": Position(67, 412),
            "5": Position(200, 412),
            "6": Position(333, 412),
            # third line
            "7": Position(67, 516),
            "8": Position(200, 516),
            "9": Position(333, 516),
            # fourth and last line
            "delete": Position(67, 620),
            "0": Position(200, 620),
            "enter": Position(333, 620),
        },
        DeviceType.FLEX: {
            # first line
            "1": Position(80, 292),
            "2": Position(240, 292),
            "3": Position(400, 292),
            # second line
            "4": Position(80, 380),
            "5": Position(240, 380),
            "6": Position(400, 380),
            # third line
            "7": Position(80, 468),
            "8": Position(240, 468),
            "9": Position(400, 468),
            # fourth and last line
            "delete": Position(80, 556),
            "0": Position(240, 556),
            "enter": Position(400, 556),
        },
        DeviceType.APEX_P: {
            # first line
            "1": Position(50, 190),
            "2": Position(150, 190),
            "3": Position(250, 190),
            # second line
            "4": Position(50, 250),
            "5": Position(150, 250),
            "6": Position(250, 250),
            # third line
            "7": Position(50, 310),
            "8": Position(150, 310),
            "9": Position(250, 310),
            # fourth and last line
            "delete": Position(50, 370),
            "0": Position(150, 370),
            "enter": Position(250, 370),
        }
    }
}

POSITIONS.update(KEYPAD_POSITIONS)

class Keypad(Element):

    def write(self, digits: str):
        for digit in digits:
            logging.info("Writing digit '%s', position '%s'", digit, self.positions[digit])
            self.client.finger_touch(*self.positions[digit])

    def delete(self):
        self.client.finger_touch(*self.positions["delete"])

    def enter(self):
        self.client.finger_touch(*self.positions["enter"])
