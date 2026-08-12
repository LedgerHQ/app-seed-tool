"""The PIN preset of BIP-85's DICE application, end to end.

A PIN here is `DICE(sides = 10, rolls = length)` and nothing else: the digits
on screen are the rolls, in order, untouched. That is what makes this device's
PIN reproducible by any other implementation of the specification, and it is
what the vectors below pin. They come from `bipsea`, run offline against the
same seed this file installs -- the seed BIP-85 itself publishes -- and the
same oracle reproduces the specification's own six-sided vector and the BIP39
derivation the neighbouring test already asserts, so it agrees with this
repository everywhere the two can be compared before being asked for anything
new.

`0934` is the case worth having: a leading zero, which is a digit and not the
absence of one. Anything that ran the rolls through an integer on the way to
the screen would draw "934" and look perfectly healthy doing it.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import (UseCaseHomeExt, UseCaseSubSettings,
                                            UseCaseViewDetails)
from ragger.firmware.touch.layouts import ChoiceList
from keypad import Keypad
import reviews
import explanations
from choicelist import (MENU_DERIVE, BIP85_APP_PIN,
                        DIGITS_4, DIGITS_6, DIGITS_8)

# Seed taken from https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki
BIP85_SEED = ("install scatter logic circle pencil average fall shoe "
              "quantum disease suspect usage")

# PIN(length, index) for that seed, from
#   bipsea derive -a dice -s 10 -n <length> -i <index> -x <xprv>
# Regenerate these if the seed above ever changes: they say nothing about the
# code, only about this seed.
PIN_6_INDEX_0 = "039262"
PIN_4_INDEX_1 = "0934"
PIN_8_INDEX_3 = "61615716"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = BIP85_SEED


def _touch_only(device):
    if device.type not in (DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P):
        skip("BIP85 has no screen on the two-button stack")


def _walk_to_pin_length(backend, device):
    """Home -> Derive -> the BIP85 explanation -> PIN -> the length screen."""
    home_page = UseCaseHomeExt(backend, device)
    buttons = ChoiceList(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Derive with BIP85", 5)
    buttons.choose(MENU_DERIVE)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_BIP85, "Which BIP85")
    ChoiceList(backend, device).choose(BIP85_APP_PIN)
    # Three buttons rather than the keypad the password length uses: a PIN is
    # one of three lengths here, which is a choice and not an entry.
    backend.wait_for_text_on_screen("How many digits", 5)


def _walk_to_review(backend, device, digits, index):
    """...and on through the length, the index explanation and the index."""
    buttons = ChoiceList(backend, device)
    keypad = Keypad(backend, device)

    _walk_to_pin_length(backend, device)
    buttons.choose(digits)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_INDEX, "Enter index")
    keypad.write(index)
    keypad.enter()
    # The review is the only screen that shows the path, and the path is what
    # makes the PIN reproducible anywhere else.
    backend.wait_for_text_on_screen("Path", 5)


def _derive(backend, device, digits, index):
    _walk_to_review(backend, device, digits, index)
    reviews.approve(backend, device, "Derive this secret")


def all_eink_bip85_pin(backend, device):
    review = UseCaseViewDetails(backend, device)

    _derive(backend, device, DIGITS_6, "0")

    # The label carries the derivation path under the header, so the two are
    # read -- and copied -- together. Both are asserted here: a PIN without
    # its path cannot be derived again, which is the whole reason to derive
    # one rather than write one down.
    backend.wait_for_text_on_screen(PIN_6_INDEX_0, 5)
    reviews.assert_on_screen(backend, "PIN (Index #0)")
    reviews.assert_on_screen(backend, "m/83696968'/89101'/10'/6'/0'")

    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_bip85_pin(device, backend, set_seed):
    _touch_only(device)
    all_eink_bip85_pin(backend, device)


@mark.use_on_backend("speculos")
def test_bip85_pin_keeps_a_leading_zero(device, backend, set_seed):
    """Four digits at index 1, which the oracle derives as 0934.

    The one case a wrong implementation passes every other test with: a PIN
    read as a number loses its leading zero, and "934" on a four-digit screen
    looks like a PIN rather than like a defect.
    """
    _touch_only(device)
    review = UseCaseViewDetails(backend, device)

    _derive(backend, device, DIGITS_4, "1")

    backend.wait_for_text_on_screen(PIN_4_INDEX_1, 5)
    reviews.assert_on_screen(backend, "PIN (Index #1)")
    reviews.assert_on_screen(backend, "m/83696968'/89101'/10'/4'/1'")

    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_bip85_pin_eight_digits(device, backend, set_seed):
    """The longest PIN, at an index that is not the first one.

    The roll count and the index sit in adjacent path components, so a flow
    that swapped them would still produce eight perfectly good digits.
    """
    _touch_only(device)
    review = UseCaseViewDetails(backend, device)

    _derive(backend, device, DIGITS_8, "3")

    backend.wait_for_text_on_screen(PIN_8_INDEX_3, 5)
    reviews.assert_on_screen(backend, "PIN (Index #3)")
    reviews.assert_on_screen(backend, "m/83696968'/89101'/10'/8'/3'")

    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_the_review_names_the_pin_and_its_length(device, backend, set_seed):
    """What the review says before anything is derived.

    The parameters and the path they combine into, on one page: this is where
    the user decides, and it is the only screen in the flow that shows all of
    them together. "6 digits" rather than a bare "6", because the same row
    carries characters and words for the other applications.
    """
    _touch_only(device)

    _walk_to_review(backend, device, DIGITS_6, "0")

    reviews.assert_on_screen(backend, "PIN")
    reviews.assert_on_screen(backend, "6 digits")
    # The path wraps on Flex, where it is drawn as two lines, so it is read
    # off the screen with the wrapping removed rather than as one drawn line.
    reviews.assert_on_screen(backend, "m/83696968'/89101'/10'/6'/0'")

    reviews.reject(backend, device)
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_refusing_the_pin_review_derives_nothing(device, backend, set_seed):
    """The refusal, asserted by the destination rather than by an absence.

    A screen that failed to draw for an unrelated reason would satisfy "the
    PIN is not on screen". Arriving home is only possible through the branch
    that erases and goes home, so it is what says the digits were never
    derived -- bip85_generate_and_display() is reached from the approving
    branch alone.
    """
    _touch_only(device)

    _walk_to_review(backend, device, DIGITS_6, "0")
    reviews.reject(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 5)
    # And nothing of the journey survived it: reset_globals() runs on the way
    # home, so the home screen carries none of what the flow collected.
    reviews.assert_not_on_screen(backend, PIN_6_INDEX_0)


@mark.use_on_backend("speculos")
def test_leaving_the_length_screen_goes_back_to_the_applications(
        device, backend, set_seed):
    """Back from the length screen returns to the list it was reached from.

    Not home, and not to the menu: the application chosen is what this screen
    asks a length for, and it was not the mistake. Same reasoning as the
    password length's own back button.
    """
    _touch_only(device)

    _walk_to_pin_length(backend, device)
    # The SDK's header arrow, which is now the only back arrow in the
    # application: the length screen is a list like the one it returns to, and
    # both carry the arrow lib_nbgl puts in a BACK_BUTTON_HEADER_HEIGHT
    # header. There is no longer a second, hand-placed square to confuse it
    # with.
    UseCaseSubSettings(backend, device).exit()

    backend.wait_for_text_on_screen("Which BIP85", 5)


@mark.use_on_backend("speculos")
def test_leaving_the_index_keypad_goes_back_to_the_applications(
        device, backend, set_seed):
    """And back from the index keypad, which is the shared screen of the flow.

    Its back button is the one nbgl_useCaseKeypad() draws, wired to
    display_bip85_select_app_page() for all four applications.
    """
    _touch_only(device)
    buttons = ChoiceList(backend, device)

    _walk_to_pin_length(backend, device)
    buttons.choose(DIGITS_6)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_INDEX, "Enter index")
    # The arrow nbgl_useCaseKeypad() draws, which is the same header arrow the
    # two lists before it carry.
    UseCaseSubSettings(backend, device).exit()

    backend.wait_for_text_on_screen("Which BIP85", 5)


@mark.use_on_backend("speculos")
def test_closing_the_pin_leaves_nothing_behind(device, backend, set_seed):
    """Closing the result, and then walking the flow again from the top.

    review_done() reaches display_home_page() and so reset_globals(), which
    erases the buffers the PIN was composed in -- headerText, reviewText and
    the derivation path. What a test can see of that is the second journey:
    it starts at the application list with nothing carried over, and the
    length screen asks its question again rather than remembering the answer.
    """
    _touch_only(device)
    review = UseCaseViewDetails(backend, device)

    _derive(backend, device, DIGITS_6, "0")
    backend.wait_for_text_on_screen(PIN_6_INDEX_0, 5)

    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)

    _walk_to_pin_length(backend, device)
    reviews.assert_not_on_screen(backend, PIN_6_INDEX_0)
