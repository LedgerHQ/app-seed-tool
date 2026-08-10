"""
The same three claims as test_backup_from_menu.py, on the two-button devices.

The Nano menu gained the same intentions the touch menu did, minus BIP-85,
which has no BAGL screen on any Nano. Everything the touch file says about
why applies here; what differs is the shape, and the shape is what has to be
tested separately:

  * there is no verdict "footer" to read. A BAGL flow is a list of steps, so
    what says the verdict continues rather than ends is whether any step of
    it generates anything: ux_bip39_backup_match_flow ends on the step that
    does, ux_bip39_check_match_flow ends on a way back to the menu;
  * a mismatch in the backup flow gets an extra step rather than a longer
    sentence: a pbb title has two short lines and no room for one.

nano.choose_in_flow() walks a flow to the step showing a label and raises if
no step does. That is what the two negative assertions below use: they are not
"the label was not on the first screen", they are "the label is on no step of
this flow", which is the claim.
"""

from pytest import fixture
from pytest import mark
from pytest import raises
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# Valid, correct checksum, and not the device's -- the same phrase
# test_bip39_seed_match.py uses for the mismatch case.
OTHER_PHRASE = "girl mad pet galaxy egg matter matrix prison refuse sense ordinary nose"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _two_button_only(device):
    if device.type == DeviceType.NANOS:
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")


@mark.use_on_backend("speculos")
def test_backup_explains_why_the_phrase_is_asked_for(device, backend, navigator, set_seed):
    _two_button_only(device)

    # The step this change adds to the Nano flow, between the menu and the
    # length list. Its first two lines are a whole sentence because a nanos nn
    # step shows only two -- and this device shows three, so the third is
    # there as well.
    nano.select_in_menu(navigator, "Generate")
    nano.wait_for_lines(backend, "This Ledger cannot", "read back its Phrase.")

    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, DEVICE_PHRASE)
    nano.wait_for_lines(backend, "Your Phrase", "is correct")

    # And the verdict carries on rather than stopping: the generation step is
    # on this flow. test_sskr_generate_two_button.py takes it from here and
    # checks what comes out of it.
    nano.choose_in_flow(backend, "Set up")
    nano.wait_for_lines(backend, "Select number", "of SSKR Shares")


@mark.use_on_backend("speculos")
def test_backup_stops_on_a_phrase_this_device_cannot_recover(device, backend, navigator, set_seed):
    _two_button_only(device)

    nano.select_in_menu(navigator, "Generate")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, OTHER_PHRASE)

    # The verdict title is the one the check flow already gave. What the
    # backup flow adds is the step after it, which answers the question that
    # was actually asked: not "is this my phrase" but "can I back it up".
    nano.wait_for_lines(backend, "Your Phrase", "doesn't match")
    nano.choose_in_flow(backend, "It would not restore")
    nano.wait_for_lines(backend, "It would not restore", "this Ledger")


@mark.use_on_backend("speculos")
def test_a_mismatch_never_reaches_the_generation_step(device, backend, navigator, set_seed):
    _two_button_only(device)

    nano.select_in_menu(navigator, "Generate")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, OTHER_PHRASE)
    nano.wait_for_lines(backend, "Your Phrase", "doesn't match")

    # No step of this flow generates anything. The entry point into the backup
    # flow is new; the requirement that the phrase be the device's before a
    # single share exists is not, and this is what says so on this stack.
    with raises(AssertionError):
        nano.choose_in_flow(backend, "Set up")


@mark.use_on_backend("speculos")
def test_checking_a_phrase_does_not_lead_to_shares(device, backend, navigator, set_seed):
    _two_button_only(device)

    nano.select_in_menu(navigator, "Check Phrase")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, DEVICE_PHRASE)
    nano.wait_for_lines(backend, "Your Phrase", "is correct")

    # The same phrase, the same verdict, and no offer behind it any more. The
    # generation step used to be the third step of this flow and was the only
    # way to reach it; it is an entry of the idle menu now, where it says what
    # it is.
    with raises(AssertionError):
        nano.choose_in_flow(backend, "Set up")
