from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt, UseCaseViewDetails, UseCaseChoice
from ragger.firmware.touch.layouts import CenteredFooter, LetterOnlyKeyboard, Suggestions, ChoiceList
from keypad import Keypad
from genericlayout import GenericLayout

@fixture(scope='session')
def set_seed():
    # Seed taken from https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
    configuration.OPTIONAL.CUSTOM_SEED = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

def all_eink_unsupported_values(backend, device):
    home_page = UseCaseHomeExt(backend, device)
    keyboard = LetterOnlyKeyboard(backend, device)
    suggestion = Suggestions(backend, device)
    check_result = CenteredFooter(backend, device)
    keypad = Keypad(backend, device)
    review = UseCaseViewDetails(backend, device)
    choice = UseCaseChoice(backend, device)
    genericbuttons = GenericLayout(backend, device)

    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("BIP39 Check", 5)
    genericbuttons.choose(1)
    backend.wait_for_text_on_screen("12 words", 5)
    genericbuttons.choose(1)
    backend.wait_for_text_on_screen("Enter word", 5)
    for word in configuration.OPTIONAL.CUSTOM_SEED.split():
        keyboard.write(word[:4])
        suggestion.choose(1)
    backend.wait_for_text_on_screen("Valid Secret", 5)
    backend.wait_for_text_on_screen("Recovery Phrase", 1)
    check_result.tap()
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()

    # Test invalid sharenum values
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("0")
    keypad.enter()
    # Test cannot catch transient (3s) status page fast enough but
    # if unsupported values are entered and app does not crash it
    # will display status and then return to "Generate SSKR" screen
#    backend.wait_for_text_on_screen("between", 3)
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("17")
    keypad.enter()
    # Test cannot catch transient (3s) status page fast enough but
    # if unsupported values are entered and app does not crash it
    # will display status and then return to "Generate SSKR" screen
#    backend.wait_for_text_on_screen("between", 3)
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()

    # Test invalid threshold values
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("3")
    keypad.enter()
    backend.wait_for_text_on_screen("Enter threshold value", 5)
    keypad.write("0")
    keypad.enter()
    # Test cannot catch transient (3s) status page fast enough but
    # if unsupported values are entered and app does not crash it
    # will display status and then return to "Generate SSKR" screen
#    backend.wait_for_text_on_screen("cannot", 3)
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("3")
    keypad.enter()
    backend.wait_for_text_on_screen("Enter threshold value", 5)
    keypad.write("4")
    keypad.enter()
    # Test cannot catch transient (3s) status page fast enough but
    # if unsupported values are entered and app does not crash it
    # will display status and then return to "Generate SSKR" screen
#    backend.wait_for_text_on_screen("cannot", 3)
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("3")
    keypad.enter()
    backend.wait_for_text_on_screen("Enter threshold value", 5)
    keypad.write("1")
    keypad.enter()
    # Test cannot catch transient (3s) status page fast enough but
    # if unsupported values are entered and app does not crash it
    # will display status and then return to "Generate SSKR" screen
#    backend.wait_for_text_on_screen("not supported", 3)
    backend.wait_for_text_on_screen("Generate SSKR", 5)
    choice.confirm()
    backend.wait_for_text_on_screen("Enter number of SSKR shares", 5)
    keypad.write("1")
    keypad.enter()
    backend.wait_for_text_on_screen("Enter threshold value", 5)
    keypad.write("1")
    keypad.enter()
    backend.wait_for_text_on_screen("SSKR Share", 5)
    backend.wait_for_text_on_screen("tuna next keep gyro", 1)
    review.exit()
    backend.wait_for_text_on_screen("Seed Tool", 5)
    home_page.quit()

@mark.use_on_backend("speculos")
def test_sskr_unsupported_values(device, backend, navigator, set_seed):
    if device.type == DeviceType.NANOS:
        skip("Skipping test for Nano S device")
    elif device.type == DeviceType.NANOSP:
        skip("Skipping test for Nano S+ device")
    elif device.type == DeviceType.NANOX:
        skip("Skipping test for Nano X device")
    elif device.type == DeviceType.STAX:
        all_eink_unsupported_values(backend, device)
    elif device.type == DeviceType.FLEX:
        all_eink_unsupported_values(backend, device)
