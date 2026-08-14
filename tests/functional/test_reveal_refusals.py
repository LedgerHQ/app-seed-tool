"""Refusing, at each of the three places this application reveals or produces.

The other tests in this directory walk these flows to their end, which says
that accepting works. This one says the other half, and it is the half worth
more: that refusing produces nothing.

**How absence is asserted.** Not by looking for the absence of a text -- a
screen that failed to draw for an unrelated reason would pass that. By the
destination: after a refusal the application must be on its home page, and
arriving there is only possible if the code took the branch that erases and
went home rather than the branch that draws. `wait_for_text_on_screen("Seed
Tool")` is therefore the assertion, and the shares, the rebuilt phrase and the
derived secret are what it proves were not drawn.

**Why each refusal is one gesture.** nbgl_useCaseGenericReview() calls the
reject callback directly, so a rejected review calls back at once -- there is
none of the hardcoded "Reject operation?" confirmation that
nbgl_useCaseReview() puts in front of a rejection. Every "Cancel" and every
"Back to safety" below reaches display_home_page(), and so reset_globals(),
in a single press.

The fourth case is not a refusal of a reveal but of a destruction: the shares'
close confirmation, whose "Back" has to return to the shares *without*
regenerating them. bolos_ux_bip39_to_sskr_convert() memzeroes the mnemonic it
read, so a second generation would split an erased buffer -- which is why the
display and the generation are separate functions, and why this asserts the
same share is still readable afterwards.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt, UseCaseChoice, UseCaseViewDetails
from ragger.firmware.touch.layouts import (CenteredFooter, ChoiceList,
                                           LetterOnlyKeyboard, Suggestions)
from keypad import Keypad
import reviews
import explanations
from choicelist import (MENU_BACKUP, MENU_RECOVER, MENU_DERIVE,
                        BIP85_APP_BIP39, WORDS_12)

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

SHARDS = [
    "tuna next keep gyro paid claw able acid able jowl chef drum judo pool lion keep idle cusp iced rust fact view twin very pose epic whiz jump jury",
    "tuna next keep gyro paid claw able acid acid gray quad kiln wall kept deli mild epic race fuel dice blue game yank fern bulb gear jade navy cost",
]


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _touch_only(device):
    if device.type not in (DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P):
        skip("The refusals of the two-button stack are covered by the "
             "two-button tests; these screens are the touch stack's")


def _enter(backend, device, words):
    keyboard = LetterOnlyKeyboard(backend, device)
    suggestion = Suggestions(backend, device)
    for word in words.split():
        keyboard.write(word[:4])
        suggestion.choose(1)


def _walk_to_sskr_review(backend, device):
    """Menu -> backup -> the device's own phrase -> share count -> threshold."""
    home_page = UseCaseHomeExt(backend, device)
    buttons = ChoiceList(backend, device)
    choice = UseCaseChoice(backend, device)
    check_result = CenteredFooter(backend, device)
    keypad = Keypad(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Generate Backup Shares", 5)
    buttons.choose(MENU_BACKUP)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_BACKUP, "12 words",
        button=explanations.ENTER_PHRASE, action=True)
    buttons.choose(WORDS_12)
    backend.wait_for_text_on_screen("Enter word", 5)
    _enter(backend, device, DEVICE_PHRASE)
    backend.wait_for_text_on_screen("Valid", 5)
    check_result.tap()
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_NUMBERS,
        "Enter number of SSKR Shares")
    keypad.write("3")
    keypad.enter()
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_THRESHOLD,
        "Enter threshold value")
    keypad.write("2")
    keypad.enter()
    backend.wait_for_text_on_screen("Words to write", 5)


@mark.use_on_backend("speculos")
def test_refusing_the_backup_review_generates_nothing(device, backend, set_seed):
    _touch_only(device)

    _walk_to_sskr_review(backend, device)

    # The review is up and nothing has been generated yet: this is the last
    # point at which refusing costs nothing.
    reviews.reject(backend, device)

    # Home, in one gesture. If the rejection had fallen through to
    # generate_and_display_sskr_shares() the share label would be here instead,
    # and this wait would time out on it.
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_refusing_the_reveal_warning_shows_no_shares(device, backend, set_seed):
    _touch_only(device)

    _walk_to_sskr_review(backend, device)

    # The warning is the review's own last page, so reaching it is walking the
    # review rather than accepting it -- and refusing from there is refusing
    # the reveal, one page after refusing the parameters.
    review = UseCaseViewDetails(backend, device)
    review.next()
    # The warning's own sentence, not the button's label: inside the long-press
    # widget the label is left-aligned beside the round button and wraps, so
    # "Generate Backup Shares" is never one drawn line.
    backend.wait_for_text_on_screen("Anyone who collects", 5)
    reviews.reject(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_going_back_to_safety_shows_no_rebuilt_phrase(device, backend, set_seed):
    _touch_only(device)
    home_page = UseCaseHomeExt(backend, device)
    buttons = ChoiceList(backend, device)
    check_result = CenteredFooter(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Recover from Backup", 5)
    buttons.choose(MENU_RECOVER)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_RECOVER, "Enter Share 1 Word 1",
        action=True)
    for shard in SHARDS:
        _enter(backend, device, shard)
    backend.wait_for_text_on_screen("Valid", 5)
    check_result.tap()

    # The one warning that is still a screen of its own: this path has no
    # review for it to be the last page of.
    reviews.decline_warning(backend, device, "A Recovery Phrase")

    # The phrase the shards rebuild is in RAM at this point, and going home is
    # what erases it. Arriving here is what says it was never drawn.
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_refusing_the_derivation_review_derives_nothing(device, backend, set_seed):
    _touch_only(device)
    home_page = UseCaseHomeExt(backend, device)
    buttons = ChoiceList(backend, device)
    keypad = Keypad(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Derive with BIP85", 5)
    buttons.choose(MENU_DERIVE)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_BIP85, "Which BIP85")
    # Both of the next two screens are the same SDK list, counted from the
    # top, so one driver walks them.
    ChoiceList(backend, device).choose(BIP85_APP_BIP39)
    backend.wait_for_text_on_screen("Length of BIP39", 5)
    buttons.choose(WORDS_12)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_INDEX, "Enter index")
    keypad.write("0")
    keypad.enter()

    # The path is on screen, so the review has been built -- and refusing it
    # still derives nothing: bip85_generate_and_display() is only reached from
    # the approving branch.
    backend.wait_for_text_on_screen("Path", 5)
    reviews.reject(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_going_back_keeps_the_shares_readable(device, backend, set_seed):
    _touch_only(device)
    review = UseCaseViewDetails(backend, device)

    _walk_to_sskr_review(backend, device)
    reviews.approve(backend, device, "Generate Backup Shares")
    backend.wait_for_text_on_screen("SSKR Share 1 of 3", 5)
    backend.wait_for_text_on_screen("tuna next keep gyro", 1)

    # Leaving asks first, because leaving destroys.
    review.exit()
    reviews.keep_shares(backend, device)

    # And "Back" returns to the shares themselves, not to a regenerated set:
    # the phrase they were split from was memzeroed by the conversion, so a
    # second generation would produce nothing to show. The same first share
    # being readable is what says the set survived the detour.
    backend.wait_for_text_on_screen("SSKR Share 1 of 3", 5)
    backend.wait_for_text_on_screen("tuna next keep gyro", 1)
