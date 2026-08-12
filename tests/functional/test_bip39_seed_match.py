"""
The verdict itself: a phrase that matches the device's seed, one that is
well formed but does not, and one that is not well formed at all.

Every other functional test in this directory that reaches this screen enters
the phrase the device was seeded with, and stops once it says the phrase is
well formed. None of them ever enters a *valid* phrase that belongs to a
different seed, or one that is not valid at all -- both of which are answers
this application exists to give.

Until this file's most recent revision, the touch stack could not tell the
first two of those apart from the assertions alone: both were titled
"Valid / Recovery Phrase" (src/nbgl/ui.c), and only the paragraph below
it changed, from "matches the one present on this Ledger device" to "doesn't
match the one present on this Ledger device". NBGL now gives all three
outcomes their own title (UI_STR_NBGL_RESULT_NOMATCH_TITLE in ui_strings.h),
matching what the two-button devices already did with three separate flows in
src/bagl/ux_nano.c -- ux_bip39_invalid_flow, ux_bip39_nomatch_flow,
ux_bip39_match_flow. The invalid screen also gains a line of advice, "Check
length, order and spelling", that only the two-button screens carried before.

So all three cases are asserted here, on both interface stacks, from the same
seed and with the same wordlist -- the point being that the two stacks answer
the same question the same way.

The mismatching phrase is a valid 12-word BIP-39 phrase with a correct
checksum (it is the one tests/unit/tests/bip39.c derives a seed from), so it
gets past the phrase-validity check and reaches the comparison. The invalid
phrase is the device phrase with its last word replaced by one that is still
in the wordlist but breaks the checksum ("planet" -> "zoo" changes the last
eleven bits, and the four checksum bits no longer match the entropy), so it is
refused one screen earlier and never reaches the comparison at all -- the same
phrase and the same reasoning as test_two_button_refusals.py's
test_bip39_bad_checksum_is_refused(), which covers the two-button side of this
same outcome.
"""

from pytest import fixture
from pytest import mark
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt
from ragger.firmware.touch.layouts import ChoiceList, LetterOnlyKeyboard, Suggestions
from choicelist import MENU_CHECK, WORDS_12
import explanations
import nano

# Same seed as the other functional tests.
# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# Valid, and not the one above.
OTHER_PHRASE = "girl mad pet galaxy egg matter matrix prison refuse sense ordinary nose"

# The device phrase with its last word replaced -- see test_two_button_refusals.py's
# BAD_CHECKSUM_PHRASE, which this mirrors so both stacks are tested against the
# same invalid input.
BAD_CHECKSUM_PHRASE = \
    "fly mule excess resource treat plunge nose soda reflect adult ramp zoo"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _two_button_check(backend, navigator, phrase, first_line, second_line):
    nano.select_in_menu(navigator, "Check Phrase")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, phrase)
    nano.wait_for_lines(backend, first_line, second_line)


def _touch_check(backend, device, phrase, title, verdict):
    home_page = UseCaseHomeExt(backend, device)
    keyboard = LetterOnlyKeyboard(backend, device)
    suggestion = Suggestions(backend, device)
    genericbuttons = ChoiceList(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Check Recovery Phrase", 5)
    genericbuttons.choose(MENU_CHECK)
    # Straight to the length choice: this journey has no explanation in front
    # of it, because entering the Phrase is the thing the entry asked for.
    backend.wait_for_text_on_screen("12 words", 5)
    genericbuttons.choose(WORDS_12)
    backend.wait_for_text_on_screen("Enter word", 5)
    for word in phrase.split():
        keyboard.write(word[:4])
        suggestion.choose(1)
    # The title now distinguishes all three outcomes on its own. The body
    # line is asserted too, since it is what a user actually reads to act on
    # the verdict, and asserting the title alone would still leave the
    # nomatch/match pair distinguishable only by that line before this fix.
    #
    # wait_for_text_on_screen() matches with re.match against each rendered
    # line's own text individually (ragger's SpeculosBackend), which anchors
    # at that line's start -- not a substring search over the whole screen.
    # `verdict` must therefore be a prefix of the specific line that carries
    # it, not any fragment of the sentence.
    backend.wait_for_text_on_screen(title, 5)
    backend.wait_for_text_on_screen(verdict, 5)


@mark.use_on_backend("speculos")
def test_bip39_phrase_matches_the_seed(device, backend, navigator, set_seed):
    if device.type in (DeviceType.NANOSP, DeviceType.NANOX):
        _two_button_check(backend, navigator, DEVICE_PHRASE,
                          "Your Phrase", "is correct")
    else:
        _touch_check(backend, device, DEVICE_PHRASE, "Valid", "matches")


@mark.use_on_backend("speculos")
def test_bip39_phrase_does_not_match_the_seed(device, backend, navigator, set_seed):
    if device.type in (DeviceType.NANOSP, DeviceType.NANOX):
        _two_button_check(backend, navigator, OTHER_PHRASE,
                          "Your Phrase", "doesn't match")
    else:
        _touch_check(backend, device, OTHER_PHRASE, "Mismatched", "doesn't match")


@mark.use_on_backend("speculos")
def test_bip39_phrase_is_invalid(device, backend, navigator, set_seed):
    if device.type in (DeviceType.NANOSP, DeviceType.NANOX):
        _two_button_check(backend, navigator, BAD_CHECKSUM_PHRASE,
                          "Your Phrase", "is not valid")
    else:
        # The verdict line has no explicit break before its second half (unlike
        # the match/nomatch strings' "...you have entered\nmatches..."), so it
        # renders as one event and must be matched from its start -- see
        # _touch_check()'s comment on wait_for_text_on_screen()'s anchoring.
        _touch_check(backend, device, BAD_CHECKSUM_PHRASE, "Invalid",
                    "is not valid")
        # The advice line: new on this stack (UI_STR_NBGL_RESULT_INVALID_ADVICE),
        # already present on the two-button screens asserted above via
        # ux_invalid_step_2 in src/bagl/ux_nano.c.
        backend.wait_for_text_on_screen("Check length", 5)
