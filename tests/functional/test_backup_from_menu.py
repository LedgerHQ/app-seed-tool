"""
Generating a backup, asked for at the menu.

This is the path the four-intention menu adds, and no test in this directory
could reach it before, because it did not exist: share generation was offered
only by check_result_callback() (src/nbgl/ui.c), and only when the tool was
BIP39, the phrase was well formed, and it matched the device. Someone who came
to back up a phrase had to check it first, succeed, and then accept an offer
they had not asked for -- and nothing in the menu said any of that was there.

Three claims, and the second and third are what make the first mean something.

  * the flow is reachable from the menu and gets as far as the share count;
  * a phrase that is well formed but is *not* this device's stops at the
    verdict and never reaches the share count -- the entry point is new, the
    guard in front of the shares is not, and this is what says so;
  * checking a phrase no longer leads there at all. That was the only door
    before; it is now a destination, and the offer that used to sit behind it
    is gone.

The second and third also pin the wording apart. The same screen, reached
from two different entries with the same correct phrase, says two different
things -- "matches the one present on this Ledger device" when the question
was whether it matches, "This is the Recovery Phrase on this Ledger device.
It can be split into shares." when the question was whether it can be backed
up -- and the footer follows: one screen ends, the other continues. Asserting
only one of the two would pass on a build that had lost the distinction.

Text is matched with re.match against each rendered line on its own (ragger's
SpeculosBackend), so every string below is a prefix of one drawn line, not a
fragment of a sentence spanning two. Where a line was chosen it was read off
Speculos rather than guessed: the three touch devices wrap these bodies at
exactly the same words.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt, UseCaseChoice
from ragger.firmware.touch.layouts import CenteredFooter, ChoiceList, LetterOnlyKeyboard, Suggestions
from choicelist import MENU_BACKUP, MENU_CHECK, WORDS_12
import explanations

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# Valid, correct checksum, and not the device's -- the same phrase
# test_bip39_seed_match.py uses for the mismatch case.
OTHER_PHRASE = "girl mad pet galaxy egg matter matrix prison refuse sense ordinary nose"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _touch_only(device):
    if device.type not in (DeviceType.STAX, DeviceType.FLEX, DeviceType.APEX_P):
        skip("The four-intention menu is the touch stack's; the Nano menu is "
             "covered by the two-button tests")


def _enter(backend, device, phrase):
    keyboard = LetterOnlyKeyboard(backend, device)
    suggestion = Suggestions(backend, device)
    for word in phrase.split():
        keyboard.write(word[:4])
        suggestion.choose(1)


def _walk_to_verdict(backend, device, entry, phrase):
    """Menu -> the chosen entry -> a 12-word phrase -> the verdict screen."""
    home_page = UseCaseHomeExt(backend, device)
    genericbuttons = ChoiceList(backend, device)
    choice = UseCaseChoice(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Check Recovery Phrase", 5)
    backend.wait_for_text_on_screen("Generate Backup Shares", 1)
    genericbuttons.choose(entry)

    # Only one of the two journeys opens on an explanation, and the difference
    # is the point: Backup was asked for Shares and has to account for wanting
    # twenty-four words, where Check was asked for exactly this. Asserting a
    # body row rather than only the title is what stops an empty screen from
    # passing, and the row is matched from the start of one drawn line, so it
    # stops where the wrapping does.
    if entry == MENU_BACKUP:
        backend.wait_for_text_on_screen("Your Phrase is split into", 1)
        explanations.pass_explanation(
            backend, device, explanations.EXPLAIN_BACKUP, "12 words",
            button=explanations.ENTER_PHRASE, action=True)
    else:
        backend.wait_for_text_on_screen("How long is your", 5)

    genericbuttons.choose(WORDS_12)
    backend.wait_for_text_on_screen("Enter word", 5)
    _enter(backend, device, phrase)


@mark.use_on_backend("speculos")
def test_backup_reaches_the_share_count_from_the_menu(device, backend, set_seed):
    _touch_only(device)
    check_result = CenteredFooter(backend, device)

    _walk_to_verdict(backend, device, MENU_BACKUP, DEVICE_PHRASE)

    # A verdict that continues, and says so. "Tap to continue" is not
    # decoration: the same screen in the check flow reads "Tap to dismiss",
    # and test_checking_a_phrase_does_not_lead_to_shares() below asserts that.
    backend.wait_for_text_on_screen("Valid", 5)
    backend.wait_for_text_on_screen("This is the Recovery Phrase", 1)
    backend.wait_for_text_on_screen("It can be split into Shares.", 1)
    backend.wait_for_text_on_screen("Tap to continue", 1)
    check_result.tap()

    # Reached without a single screen having offered anything: the user asked
    # for this at the menu five screens ago.
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_NUMBERS,
        "Enter number of SSKR Shares")


@mark.use_on_backend("speculos")
def test_backup_stops_on_a_phrase_this_device_cannot_recover(device, backend, set_seed):
    _touch_only(device)
    check_result = CenteredFooter(backend, device)

    _walk_to_verdict(backend, device, MENU_BACKUP, OTHER_PHRASE)

    # Well formed, so it got past the checksum, and not this device's, so the
    # comparison refused it. The sentence is the backup flow's own: "doesn't
    # match the one present on this Ledger device" answers a question this
    # user did not ask.
    backend.wait_for_text_on_screen("Mismatched", 5)
    backend.wait_for_text_on_screen("You would be backing up", 1)
    backend.wait_for_text_on_screen("a Phrase this Ledger", 1)
    backend.wait_for_text_on_screen("cannot recover.", 1)
    # A failure ends the flow, so it is dismissed and not continued.
    backend.wait_for_text_on_screen("Tap to dismiss", 1)
    check_result.tap()

    # And it ends at the home page rather than at the share count. This is the
    # assertion that makes the new entry point safe to have: arriving here is
    # only possible if check_result_callback() took neither of the two
    # branches that lead to a share, and reaching "Seed Tool" is what proves
    # the keypad was not drawn instead.
    backend.wait_for_text_on_screen("Seed Tool", 5)


@mark.use_on_backend("speculos")
def test_checking_a_phrase_does_not_lead_to_shares(device, backend, set_seed):
    _touch_only(device)
    check_result = CenteredFooter(backend, device)

    _walk_to_verdict(backend, device, MENU_CHECK, DEVICE_PHRASE)

    # Same phrase, same device, same screen as the first test -- and a
    # different answer, because a different question was asked.
    backend.wait_for_text_on_screen("Valid", 5)
    backend.wait_for_text_on_screen("The Phrase you have entered", 1)
    backend.wait_for_text_on_screen("matches the one present", 1)
    backend.wait_for_text_on_screen("Tap to dismiss", 1)
    check_result.tap()

    # Home, not the "Generate SSKR Phrase?" offer this used to open. Backing
    # up is still one tap away, from the menu, where it says what it is.
    backend.wait_for_text_on_screen("Seed Tool", 5)
