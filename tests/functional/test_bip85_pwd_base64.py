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
from choicelist import MENU_DERIVE, BIP85_APP_PWD_BASE64

@fixture(scope='session')
def set_seed():
    # Seed taken from https://github.com/bitcoin/bips/blob/master/bip-0085.mediawiki
    configuration.OPTIONAL.CUSTOM_SEED = "install scatter logic circle pencil average fall shoe quantum disease suspect usage"

def all_eink_bip85_pwd_base64(backend, device):
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
    select_tool.choose(BIP85_APP_PWD_BASE64)
    backend.wait_for_text_on_screen("Enter password length", 5)
    # The keypad names the range this application accepts, on its own line --
    # escaped because wait_for_text_on_screen() matches with re.match(). Base85
    # is 10 to 80; the two are not interchangeable, which is why the title is
    # composed per application rather than declared once.
    backend.wait_for_text_on_screen(r"\(20 - 86\)", 1)
    # A length outside that range comes back to this keypad, not to the
    # application menu: the application chosen is what decides the bounds and
    # was never the mistake.
    keypad.write("5")
    keypad.enter()
    backend.wait_for_text_on_screen("BIP85 password", 5)
    backend.wait_for_text_on_screen("Enter password length", 10)
    keypad.write("2")
    keypad.write("1")
    keypad.enter()
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_INDEX, "Enter index")
    backend.wait_for_text_on_screen(r"Enter index \(0 - 9,999,999\)", 1)
    keypad.write("0")
    keypad.enter()

    # The index no longer derives anything. The review lists the three
    # parameters and the path they combine into -- the path being what makes
    # the result reproducible anywhere else -- and its last page is the
    # warning that stands in front of the secret.
    backend.wait_for_text_on_screen("Path", 5)
    reviews.approve(backend, device, "Derive this secret")

    backend.wait_for_text_on_screen("Base64 Password", 5)
    if device.type == DeviceType.STAX:
        backend.wait_for_text_on_screen("dKLoepugzdVJvdL56o", 1)
        backend.wait_for_text_on_screen("gNV", 1)
    elif device.type == DeviceType.FLEX:
        backend.wait_for_text_on_screen("dKLoepugzdVJvdL56og", 1)
        backend.wait_for_text_on_screen("NV", 1)
    elif device.type == DeviceType.APEX_P:
        backend.wait_for_text_on_screen("dKLoepugzdVJvdL56o", 1)
        backend.wait_for_text_on_screen("gNV", 1)
    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)
    home_page.quit()

@mark.use_on_backend("speculos")
def test_bip85_pwd_base64(device, backend, set_seed):
    if device.type == DeviceType.NANOS:
        skip("Skipping test for Nano S device")
    elif device.type == DeviceType.NANOSP:
        skip("Skipping test for Nano S+ device")
    elif device.type == DeviceType.NANOX:
        skip("Skipping test for Nano X device")
    elif device.type == DeviceType.STAX:
        all_eink_bip85_pwd_base64(backend, device)
    elif device.type == DeviceType.FLEX:
        all_eink_bip85_pwd_base64(backend, device)
    elif device.type == DeviceType.APEX_P:
        all_eink_bip85_pwd_base64(backend, device)
