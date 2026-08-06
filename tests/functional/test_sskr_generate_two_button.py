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
Their shape can be, and it is what a split has to get right. Each share is
read back off the display and held to four things:

  * three shares come out when three were asked for;
  * each is 29 ByteWords -- the CBOR tag #6.40309 (`d9 9d 75`), the byte-string
    header of a 21-byte shard (`0x55`), 5 metadata bytes and 16 of share value
    inside it, then a 4-byte CRC-32. Those first four bytes are "tuna next keep
    gyro", the same prefix test_sskr_128bit.py asserts on the touch devices;
  * all three carry the same first eight words, so they belong to one share
    set -- shares of different sets would never combine;
  * their ninth words are "able", "acid" and "also", ByteWords for 0x00, 0x01
    and 0x02. That byte is the reserved nibble followed by the member index
    (BCR-2020-011), so this says both that the reserved nibble is zero and
    that the three shares are numbered rather than being three copies of one
    share -- which is the failure that would make the split worthless while
    still looking like a split.

This flow ends on a step that quits the application, so the shares cannot be
fed back into "Check SSKR" without a second run of the emulator, and no
round-trip is attempted here.
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
SHARE_LENGTH = 29


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


@mark.use_on_backend("speculos")
def test_sskr_generate_two_button(device, backend, navigator, set_seed):
    if device.type == DeviceType.NANOS:
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")

    nano.select_in_menu(navigator, "Generate")
    nano.select_in_menu(navigator, "12 words")
    nano.enter_phrase(backend, DEVICE_PHRASE)

    # The phrase is the device's, so the verdict is a match -- and only the
    # match flow carries on to the generation
    # (ux_bip39_backup_match_flow, src/bagl/ux_nano.c). Asserting it here is
    # what makes the rest of this test mean "generation from a verified
    # phrase" rather than "generation".
    nano.wait_for_lines(backend, "BIP39 Phrase", "is correct")

    nano.choose_in_flow(backend, "Generate")
    nano.choose_in_carousel(backend, str(SHARE_COUNT))
    nano.choose_in_carousel(backend, str(THRESHOLD))

    shares = nano.collect_shares(backend)

    assert len(shares) == SHARE_COUNT, \
        f"asked for {SHARE_COUNT} shares, {len(shares)} were displayed"

    for index, words in enumerate(shares, start=1):
        assert len(words) == SHARE_LENGTH, \
            f"share #{index} is {len(words)} words, expected {SHARE_LENGTH}"
        assert words[:4] == ["tuna", "next", "keep", "gyro"], \
            f"share #{index} does not open on the SSKR tag: {words[:4]}"

    assert len({tuple(words[:8]) for words in shares}) == 1, \
        "the shares do not carry a common share-set header"

    assert [words[8] for words in shares] == ["able", "acid", "also"], \
        f"unexpected member-index bytes: {[words[8] for words in shares]}"
