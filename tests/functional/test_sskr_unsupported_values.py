from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
from ragger.firmware.touch.use_cases import UseCaseHomeExt, UseCaseViewDetails, UseCaseChoice
from ragger.firmware.touch.layouts import CenteredFooter, LetterOnlyKeyboard, Suggestions, ChoiceList
from keypad import Keypad
import reviews
import explanations
from choicelist import MENU_BACKUP, WORDS_12

# The threshold keypad names the values it accepts. Its upper bound is the
# share count entered on the screen before it, so this string is also the
# evidence that the count survived a refusal. Its lower bound is 2, not 1:
# a threshold of 1 over three shares is the 1-of-m case the app refuses, and
# a title reading (1 - 3) would be promising it.
#
# Escaped, because wait_for_text_on_screen() passes its argument to re.match():
# unescaped parentheses are a group, and "(2 - 3)" would match a screen reading
# "Enter threshold value 2 - 3", which is not what the device draws.
THRESHOLD_TITLE_3_SHARES = r"Enter threshold value \(2 - 3\)"

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
    genericbuttons = ChoiceList(backend, device)

    # Reached from the menu entry, not from a verdict that happened to offer
    # it. The verdict is still crossed on the way -- the phrase has to be the
    # device's before it can be split -- but it is now a step of a flow the
    # user asked for rather than the only door into it.
    backend.wait_for_text_on_screen("Seed Tool", 10)
    home_page.action()
    backend.wait_for_text_on_screen("Generate Backup Shares", 5)
    genericbuttons.choose(MENU_BACKUP)
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_BACKUP, "12 words",
        button=explanations.ENTER_PHRASE, action=True)
    genericbuttons.choose(WORDS_12)
    backend.wait_for_text_on_screen("Enter word", 5)
    for word in configuration.OPTIONAL.CUSTOM_SEED.split():
        keyboard.write(word[:4])
        suggestion.choose(1)
    backend.wait_for_text_on_screen("Valid", 5)
    backend.wait_for_text_on_screen("Recovery Phrase", 1)
    check_result.tap()

    # An out-of-range share count is refused and asked for again, on the keypad
    # that asked for it. Each refusal is waited for on its own: the status page
    # does not carry the keypad title, so reaching the title afterwards is a
    # screen change and not the screen we were already on.
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_NUMBERS,
        "Enter number of SSKR Shares")
    # The range on its own line, matched whole. This title carries a
    # hand-placed line break for the same reason the password one does, and a
    # layout that wrapped inside the range would draw "to generate (1 - " and
    # "16)" as two events, so this assertion is what keeps the break where it
    # was put. The other three ranges are matched whole for the same reason.
    backend.wait_for_text_on_screen(r"to generate \(1 - 16\)", 1)
    keypad.write("0")
    keypad.enter()
    backend.wait_for_text_on_screen("Number of SSKR", 5)
    backend.wait_for_text_on_screen("Enter number of SSKR Shares", 10)
    keypad.write("17")
    keypad.enter()
    backend.wait_for_text_on_screen("Number of SSKR", 5)
    backend.wait_for_text_on_screen("Enter number of SSKR Shares", 10)

    # From here the share count is entered exactly once, and the rest of the
    # test never types it again. Everything below depends on it still being 3.
    keypad.write("3")
    keypad.enter()
    # The threshold explanation stands between the two keypads on the first
    # pass, and only there: the three refusals below come back to the keypad
    # itself, which is what the rest of this test asserts.
    # Not THRESHOLD_TITLE_3_SHARES: that constant is escaped for
    # wait_for_text_on_screen(), which matches with re.match, while
    # pass_explanation() compares with str.startswith. The line below is
    # asserted whole two statements further down anyway.
    explanations.pass_explanation(
        backend, device, explanations.EXPLAIN_THRESHOLD,
        "Enter threshold value")
    backend.wait_for_text_on_screen(THRESHOLD_TITLE_3_SHARES, 5)

    # Each of the three threshold refusals comes back to the threshold keypad,
    # still announcing (1 - 3).
    keypad.write("0")
    keypad.enter()
    backend.wait_for_text_on_screen("Threshold value", 5)
    backend.wait_for_text_on_screen(THRESHOLD_TITLE_3_SHARES, 10)

    keypad.write("4")
    keypad.enter()
    backend.wait_for_text_on_screen("Threshold value", 5)
    backend.wait_for_text_on_screen(THRESHOLD_TITLE_3_SHARES, 10)

    keypad.write("1")
    keypad.enter()
    backend.wait_for_text_on_screen("A threshold of 1", 5)
    backend.wait_for_text_on_screen(THRESHOLD_TITLE_3_SHARES, 10)

    # And a threshold this one does accept generates a 2-of-3 straight away.
    # This is what the three refusals above cost before: each of them returned
    # to the head of the flow, so reaching this point meant typing the share
    # count three more times. The share label naming 3 is the proof that the
    # count entered once is the count that was used.
    keypad.write("2")
    keypad.enter()

    # The review now stands between the accepted threshold and the shares, and
    # it is where the count entered once becomes visible as a number rather
    # than only as a share label: "3" against the "2" just typed.
    backend.wait_for_text_on_screen("Number of Shares", 5)
    reviews.approve(backend, device, "Generate Backup Shares")

    backend.wait_for_text_on_screen("SSKR Share 1 of 3", 5)
    backend.wait_for_text_on_screen("tuna next keep gyro", 1)
    review.exit()
    reviews.close_shares(backend, device)
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
    elif device.type == DeviceType.APEX_P:
        all_eink_unsupported_values(backend, device)
