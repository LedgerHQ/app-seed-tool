"""Each menu constant opens the entry its name claims.

`choicelist.py` maps the four entries to the rows of the SDK list the menu is
drawn as, counting from the top -- which is how the screen reads and how
ragger's ChoiceList counts. It used to count from the *bottom*, because the
menu was a hand-built stack that grew upwards from the bottom margin, and that
inversion is the kind of thing a suite can get wrong and stay green about.
The inversion is gone; the way to get the mapping wrong is not.

Nothing else in this directory checks it. Every other test waits for a label
to appear *somewhere* on the menu and then touches a coordinate, so a table
shifted by one row would satisfy the wait, open the wrong entry, and fail
several screens later with a message about something else entirely.

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
from ragger.firmware.touch.layouts import ChoiceList
from ragger.firmware.touch.use_cases import UseCaseHomeExt
from choicelist import MENU_CHECK, MENU_BACKUP, MENU_RECOVER, MENU_DERIVE


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = \
        "fly mule excess resource treat plunge nose soda reflect adult ramp planet"


# entry constant -> a line only the screen it opens draws.
#
# Every journey opens on its own explanation, so these are the first pages of
# those four rather than the screens that used to come first. That is the
# property worth asserting here: the menu is a set of four intentions and each
# has to land on the one that names it -- an entry wired to the wrong callback
# would still draw *a* screen.
#
# These are the titles of the four screens the entries open on, and they go
# through ragger's wait_for_text_on_screen(), which re.matches each drawn line
# on its own -- so each has to be a prefix of one drawn line.
DESTINATIONS = [
    (MENU_CHECK, "How long is your"),
    (MENU_BACKUP, "How the backup works"),
    (MENU_RECOVER, "How recovery works"),
    (MENU_DERIVE, "How BIP85 works"),
]


@mark.use_on_backend("speculos")
@mark.parametrize("entry,destination", DESTINATIONS,
                  ids=["check", "backup", "recover", "derive"])
def test_each_entry_opens_what_it_names(device, backend, set_seed, entry, destination):
    if device.type not in (DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P):
        skip("The four-entry menu is the touch stack's")

    home_page = UseCaseHomeExt(backend, device)
    entries = ChoiceList(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    # All four labels on one screen: this is the menu, before anything is
    # touched, and every entry below is drawn.
    backend.wait_for_text_on_screen("Check Recovery Phrase", 5)
    backend.wait_for_text_on_screen("Generate Backup Shares", 1)
    backend.wait_for_text_on_screen("Recover from Backup", 1)
    backend.wait_for_text_on_screen("Derive with BIP85", 1)

    entries.choose(entry)
    backend.wait_for_text_on_screen(destination, 5)
