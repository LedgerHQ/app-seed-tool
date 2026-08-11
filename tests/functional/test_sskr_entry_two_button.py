"""
Entering SSKR shares on the two-button devices.

test_sskr_128bit.py and test_sskr_256bit.py enter the same shares on the touch
devices and skip on Nano X and Nano S+, so the two-button share-entry path --
src/bagl/nanox_enter_phrase.c and src/bagl/nanos_enter_phrase.c, which is a
different implementation of the same screen, not a different rendering of one
-- had no end-to-end coverage at all.

That path is where the shape of a share is worked out while it is being typed:
the fourth ByteWord of the first share carries the CBOR byte-string header, and
`bolos_ux_sskr_entry_header_update()` reads how many words the share holds out
of it. The entry loop then runs to that count, for this share and for the ones
after it. Nothing else on the device decides when a share is complete.

The shares below are the ones test_sskr_128bit.py enters, for the same seed:
two 2-of-3 shards of the 128-bit Blockchain Commons test vector. Any valid pair
would do -- what is being checked is that the two-button screens accept them
and reach the same verdict the touch screens do.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# Two of the three shards of a 2-of-3 split of that seed, as
# test_sskr_128bit.py enters them.
SHARDS = [
    "tuna next keep gyro paid claw able acid able jowl chef drum judo pool "
    "lion keep idle cusp iced rust fact view twin very pose epic whiz jump jury",
    "tuna next keep gyro paid claw able acid acid gray quad kiln wall kept "
    "deli mild epic race fuel dice blue game yank fern bulb gear jade navy cost",
]


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


@mark.use_on_backend("speculos")
def test_sskr_entry_two_button(device, backend, navigator, set_seed):
    if device.type == DeviceType.NANOS:
        # Speculos has dropped Nano S; the build is still produced and this
        # test would drive it unchanged if the emulator returns.
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")

    nano.select_in_menu(navigator, "Recover")
    nano.open_sskr_entry(backend)

    for shard in SHARDS:
        for word in shard.split():
            nano.enter_word(backend, word)

    # The verdict, both lines. The first one alone is shared with the screen
    # that says the phrase does not match, so asserting it on its own would
    # assert nothing about the answer -- the same reason
    # test_bip39_seed_match.py checks two lines.
    nano.wait_for_lines(backend, "SSKR Shares", "are correct")
