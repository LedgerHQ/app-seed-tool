"""
Generating SSKR shares on the two-button devices.

test_sskr_128bit.py and test_sskr_256bit.py generate shares on the touch
devices and skip on Nano X and Nano S+, so this half of the feature had no
end-to-end coverage there at all -- neither the two menus that pick the share
count and the threshold (src/bagl/ux_sskr.c) nor generate_sskr() behind them.

What that path runs is not UI code. It reaches bolos_ux_bip39_to_sskr_convert()
and through it the Shamir split, its randomness, and the CBOR and ByteWords
encoding of every share. None of that is device-specific, but the arguments it
is called with are: the two-button screens compute them in their own file, from
their own context fields, and a mistake there is invisible to the touch tests.

The shares themselves cannot be asserted: the 16-bit share-set identifier is
drawn at random, so two runs of the same split produce different ByteWords.
What is fixed is everything in front of it -- the CBOR tag #6.40309 (`d9 9d 75`)
and the byte-string header of a 21-byte shard (`0x55`), which is what a 12-word
seed always produces. Those four bytes are "tuna next keep gyro", the same
prefix test_sskr_128bit.py asserts on the touch devices.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

SHARE_COUNT = 3
THRESHOLD = 2


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


@mark.use_on_backend("speculos")
def test_sskr_generate_two_button(device, backend, navigator, set_seed):
    if device.type == DeviceType.NANOS:
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")

    nano.select_in_menu(navigator, "Check BIP39")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, DEVICE_PHRASE)

    # The phrase is the device's, so the verdict is a match -- and only the
    # match flow offers to generate shares (ux_bip39_match_flow,
    # src/bagl/ux_nano.c). Asserting it here is what makes the rest of this
    # test mean "generation from a verified phrase" rather than "generation".
    nano.wait_for_lines(backend, "BIP39 Phrase", "is correct")

    nano.choose_in_flow(backend, "Generate")
    nano.choose_in_carousel(backend, str(SHARE_COUNT))
    nano.choose_in_carousel(backend, str(THRESHOLD))

    # The tag and the byte-string header of a 21-byte shard, which is what a
    # 12-word seed produces. Everything after them is the shard, and the
    # share-set identifier inside it is random.
    nano.wait_for_lines(backend, "tuna next keep gyro")
