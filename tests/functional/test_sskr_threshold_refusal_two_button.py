"""
Asking for a 1-of-3 SSKR split, on the two-button devices.

A threshold of 1 over more than one share is refused: each share would on its
own reconstruct the seed, so the split would hand out three copies of the
secret while looking like a split. `sskr_threshold_selector()` in
src/bagl/ux_sskr.c stops there and shows ux_threshold_warn_flow instead of
generating anything.

The refusal had no test that reads the screen. test_sskr_unsupported_values.py
drives the equivalent touch path, but its assertion on the message is
commented out, with the reason given in the file: the touch status page lasts
three seconds and the test cannot catch it in time. What it asserts instead is
that the application comes back to the "Generate SSKR" screen -- which a
build that silently ignored the choice and returned would also satisfy. So on
no device did anything check that the user is told why nothing was generated.

On the two-button devices the refusal is not a transient status page but an
ordinary flow that waits for the user, so the message can simply be read, and
that is what this test does.

The other half of the guarantee is structural and already holds: the threshold
list is built by `sskr_threshold_getter()`, which bounds itself by the share
count the user has just chosen, so a threshold above the number of shares
cannot be offered in the first place and there is no screen to assert about
it.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# Same seed as the other functional tests.
# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


@mark.use_on_backend("speculos")
def test_sskr_threshold_of_one_is_refused(device, backend, navigator, set_seed):
    if device.type == DeviceType.NANOS:
        # Speculos has dropped Nano S; the build is still produced and this
        # test would drive it unchanged if the emulator returns.
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")

    # Generation is only reachable through a phrase the device agrees with,
    # so the phrase below is the one it was seeded with.
    nano.select_in_menu(navigator, "Generate")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, DEVICE_PHRASE)
    nano.wait_for_lines(backend, "BIP39 Phrase", "is correct")

    # The verdict flow carries the entry into generation on its last step.
    nano.choose_in_flow(backend, "Generate")

    nano.choose_in_carousel(backend, "3")
    nano.choose_in_carousel(backend, "1")

    # Both lines, because the flow's second step is another warning screen and
    # the first line alone would not say which one was reached.
    nano.wait_for_lines(backend, "1-of-m shares", "where m > 1")
