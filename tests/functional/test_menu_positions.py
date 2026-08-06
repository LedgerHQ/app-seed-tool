"""
Each menu constant opens the entry its name claims.

`genericlayout.py` maps the four entries to touch coordinates counted from the
*bottom* of the screen, because `generic_screen_configure_buttons()`
(src/nbgl/layout_generic_screen.c) stacks its buttons upwards: child 0 is the
lowest one drawn and the last one read. That inversion is the kind of thing a
suite can get wrong and stay green about.

Nothing else in this directory checks the mapping. Every other test waits for
a label to appear *somewhere* on the menu and then touches a coordinate, so a
table shifted by one row would satisfy the wait, open the wrong entry, and
fail several screens later with a message about something else entirely.

This asserts the only thing that settles it: the screen each coordinate
actually arrives on. The four destinations are distinguishable from each other
by their first line, which is what makes the assertions two-directional --
MENU_BACKUP landing on the length screen would fail, not merely fail to prove.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt
from genericlayout import GenericLayout, MENU_CHECK, MENU_BACKUP, MENU_RECOVER, MENU_DERIVE


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = \
        "fly mule excess resource treat plunge nose soda reflect adult ramp planet"


# entry constant -> a line only the screen it opens draws
DESTINATIONS = [
    (MENU_CHECK, "How long is your"),
    (MENU_BACKUP, "Enter your recovery"),
    (MENU_RECOVER, "Enter Share 1 Word 1"),
    (MENU_DERIVE, "Which BIP85"),
]


@mark.use_on_backend("speculos")
@mark.parametrize("entry,destination", DESTINATIONS,
                  ids=["check", "backup", "recover", "derive"])
def test_each_entry_opens_what_it_names(device, backend, set_seed, entry, destination):
    if device.type not in (DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P):
        skip("The four-entry menu is the touch stack's")

    home_page = UseCaseHomeExt(backend, device)
    genericbuttons = GenericLayout(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    # All four labels on one screen: this is the menu, before anything is
    # touched, and every entry below is drawn.
    backend.wait_for_text_on_screen("Check recovery phrase", 5)
    backend.wait_for_text_on_screen("Generate backup shares", 1)
    backend.wait_for_text_on_screen("Recover from backup", 1)
    backend.wait_for_text_on_screen("Derive with BIP85", 1)

    genericbuttons.choose(entry)
    backend.wait_for_text_on_screen(destination, 5)
