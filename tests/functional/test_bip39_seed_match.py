"""
The verdict itself: a phrase that matches the device's seed, and one that does
not.

Every functional test in this directory enters the phrase the device was
seeded with, and stops at the screen that says the phrase is well formed. None
of them ever enters a *valid* phrase that belongs to a different seed -- which
is the answer this application exists to give. The distinction is not visible
in the assertion those tests make: on the touch devices both outcomes are
titled "Valid Secret Recovery Phrase" (src/nbgl/ui.c), and only the paragraph
below it changes, from "matches the one present on this Ledger device" to
"doesn't match the one present on this Ledger device". On the two-button
devices they are two separate flows in src/bagl/ux_nano.c, ux_bip39_match_flow
and ux_bip39_nomatch_flow, and only the first was ever reached.

So both cases are asserted here, on both interface stacks, from the same seed
and with the same wordlist -- the point being that the two stacks answer the
same question the same way.

The mismatching phrase is a valid 12-word BIP-39 phrase with a correct
checksum (it is the one tests/unit/tests/bip39.c derives a seed from), so it
gets past the phrase-validity check and reaches the comparison. A phrase with
a bad checksum would stop one screen earlier and prove something else.
"""

from pytest import fixture
from pytest import mark
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt
from ragger.firmware.touch.layouts import LetterOnlyKeyboard, Suggestions
from genericlayout import GenericLayout
import nano

# Same seed as the other functional tests.
# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# Valid, and not the one above.
OTHER_PHRASE = "girl mad pet galaxy egg matter matrix prison refuse sense ordinary nose"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _two_button_check(backend, phrase, first_line, second_line):
    nano.select_in_menu(backend, "Check BIP39")
    nano.select_in_menu(backend, "12 words")
    nano.enter_phrase(backend, phrase)
    nano.wait_for_lines(backend, first_line, second_line)


def _touch_check(backend, device, phrase, verdict):
    home_page = UseCaseHomeExt(backend, device)
    keyboard = LetterOnlyKeyboard(backend, device)
    suggestion = Suggestions(backend, device)
    genericbuttons = GenericLayout(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("BIP39 Check", 5)
    genericbuttons.choose(1)
    backend.wait_for_text_on_screen("12 words", 5)
    genericbuttons.choose(1)
    backend.wait_for_text_on_screen("Enter word", 5)
    for word in phrase.split():
        keyboard.write(word[:4])
        suggestion.choose(1)
    backend.wait_for_text_on_screen("Valid Secret", 5)
    # The line that actually carries the verdict. Asserting the title alone
    # would pass on either outcome.
    backend.wait_for_text_on_screen(verdict, 5)


@mark.use_on_backend("speculos")
def test_bip39_phrase_matches_the_seed(device, backend, set_seed):
    if device.type in (DeviceType.NANOSP, DeviceType.NANOX):
        _two_button_check(backend, DEVICE_PHRASE, "BIP39 Phrase", "is correct")
    else:
        _touch_check(backend, device, DEVICE_PHRASE, "matches")


@mark.use_on_backend("speculos")
def test_bip39_phrase_does_not_match_the_seed(device, backend, set_seed):
    if device.type in (DeviceType.NANOSP, DeviceType.NANOX):
        _two_button_check(backend, OTHER_PHRASE, "BIP39 Phrase", "doesn't match")
    else:
        _touch_check(backend, device, OTHER_PHRASE, "doesn't match")
