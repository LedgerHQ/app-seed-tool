"""
The two answers this application gives that are not "yes", on the two-button
devices.

Every other two-button test walks a path that succeeds: a phrase that matches,
shares that recombine, a set that generates. The screens that refuse are a
different flow in src/bagl/ux_nano.c -- ux_bip39_invalid_flow and
ux_sskr_invalid_flow -- reached from a different branch of
screen_onboarding_restore_word_validate(), and neither had ever been displayed
under test on these devices.

They are also the screens with the most at stake. Telling a holder their backup
is unreadable when it is fine sends them to re-enter it; telling them it is fine
when it is not sends them away with something that will not open their device.
The two cases below are the ones where the application has to say no.

Both stop before the seed comparison, and for different reasons, which is the
point of covering both:

  - the BIP-39 phrase is twelve real words whose checksum does not close, so
    bolos_ux_bip39_mnemonic_check() refuses it and no seed is ever derived;
  - the SSKR shares carry one substituted ByteWord, so the CRC-32 over the
    frame no longer matches and bolos_ux_sskr_hex_check() refuses the set.

A phrase that is well formed but belongs to another seed is a third answer
again, and is covered for both stacks by test_bip39_seed_match.py.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# The same twelve words with the last one replaced. Every word is in the
# wordlist, so entry accepts each of them and the phrase only falls over at the
# checksum -- which is the branch under test. "planet" -> "zoo" changes the last
# eleven bits, and the four checksum bits no longer match the entropy.
BAD_CHECKSUM_PHRASE = \
    "fly mule excess resource treat plunge nose soda reflect adult ramp zoo"

# The shares test_sskr_128bit.py enters, with one ByteWord of the first share
# substituted: "chef" -> "cost", both in the ByteWords list, so entry accepts it
# and the frame only falls over at its CRC-32.
CORRUPTED_SHARDS = [
    "tuna next keep gyro paid claw able acid able jowl cost drum judo pool "
    "lion keep idle cusp iced rust fact view twin very pose epic whiz jump jury",
    "tuna next keep gyro paid claw able acid acid gray quad kiln wall kept "
    "deli mild epic race fuel dice blue game yank fern bulb gear jade navy cost",
]


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


def _two_button_only(device):
    if device.type == DeviceType.NANOS:
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only")


@mark.use_on_backend("speculos")
def test_bip39_bad_checksum_is_refused(device, backend, navigator, set_seed):
    _two_button_only(device)

    nano.select_in_menu(navigator, "Check BIP39")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, BAD_CHECKSUM_PHRASE)

    # Both lines: "BIP39 Recovery" alone appears on screens that are not this
    # one, and the verdict screens of this application differ by their second
    # line rather than their first.
    nano.wait_for_lines(backend, "BIP39 Recovery", "phrase invalid")


@mark.use_on_backend("speculos")
def test_sskr_bad_crc_is_refused(device, backend, navigator, set_seed):
    _two_button_only(device)

    nano.select_in_menu(navigator, "Check SSKR")
    nano.confirm(backend)

    for shard in CORRUPTED_SHARDS:
        for word in shard.split():
            nano.enter_word(backend, word)

    nano.wait_for_lines(backend, "SSKR Recovery", "phrase invalid")
