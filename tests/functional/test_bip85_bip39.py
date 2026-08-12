from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt, UseCaseViewDetails
from ragger.firmware.touch.layouts import ChoiceList
from keypad import Keypad
import reviews
import explanations
from choicelist import MENU_DERIVE, BIP85_APP_BIP39, WORDS_12

@fixture(scope='session')
def set_seed():
    # Seed taken from https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki
    configuration.OPTIONAL.CUSTOM_SEED = "install scatter logic circle pencil average fall shoe quantum disease suspect usage"

def all_eink_bip85_bip39(backend, device):
    home_page = UseCaseHomeExt(backend, device)
    select_tool = ChoiceList(backend, device)
    keypad = Keypad(backend, device)
    review = UseCaseViewDetails(backend, device)
    genericbuttons = ChoiceList(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Derive with BIP85", 5)
    genericbuttons.choose(MENU_DERIVE)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_BIP85, "Which BIP85")
    select_tool.choose(BIP85_APP_BIP39)
    backend.wait_for_text_on_screen("Length of BIP39", 5)
    genericbuttons.choose(WORDS_12)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_INDEX, "Enter index")
    keypad.write("0")
    keypad.enter()

    # The index no longer derives anything. The review lists the three
    # parameters and the path they combine into -- the path being what makes
    # the result reproducible anywhere else -- and its last page is the
    # warning that stands in front of the secret.
    backend.wait_for_text_on_screen("Path", 5)
    reviews.approve(backend, device, "Derive this secret")

    backend.wait_for_text_on_screen("BIP39 Phrase", 5)
    if device.type == DeviceType.STAX:
        backend.wait_for_text_on_screen("girl mad pet galaxy", 1)
        backend.wait_for_text_on_screen("egg matter matrix", 1)
        backend.wait_for_text_on_screen("prison refuse sense", 1)
        backend.wait_for_text_on_screen("ordinary nose", 1)
    elif device.type == DeviceType.FLEX:
        backend.wait_for_text_on_screen("girl mad pet galaxy egg", 1)
        backend.wait_for_text_on_screen("matter matrix prison", 1)
        backend.wait_for_text_on_screen("refuse sense ordinary", 1)
        backend.wait_for_text_on_screen("nose", 1)
    elif device.type == DeviceType.APEX_P:
        backend.wait_for_text_on_screen("girl mad pet galaxy", 1)
        backend.wait_for_text_on_screen("egg matter matrix", 1)
        backend.wait_for_text_on_screen("prison refuse sense", 1)
        backend.wait_for_text_on_screen("ordinary nose", 1)
    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)
    home_page.quit()

@mark.use_on_backend("speculos")
def test_bip85_bip39(device, backend, set_seed):
    if device.type == DeviceType.NANOS:
        skip("Skipping test for Nano S device")
    elif device.type == DeviceType.NANOSP:
        skip("Skipping test for Nano S+ device")
    elif device.type == DeviceType.NANOX:
        skip("Skipping test for Nano X device")
    elif device.type == DeviceType.STAX:
        all_eink_bip85_bip39(backend, device)
    elif device.type == DeviceType.FLEX:
        all_eink_bip85_bip39(backend, device)
    elif device.type == DeviceType.APEX_P:
        all_eink_bip85_bip39(backend, device)
