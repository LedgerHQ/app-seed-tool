"""
Entering the same SSKR share twice, on the two-button devices.

This covers the outcome of the share-entry screen that sits between the two
obvious ones. Shares that reconstruct the onboarded seed are entered by
test_sskr_entry_two_button.py, and shares whose CRC is wrong are refused in
test_two_button_refusals.py before anything is combined. What had no
end-to-end test on any device is the case in between -- shares that are
individually well-formed and still cannot be combined.

The distinction is the point. `bolos_ux_sskr_hex_check()` accepts a share pair
made of the same share twice, and does so deliberately: the shares carry a
valid CBOR header, identical cross-share metadata and genuine CRC-32s, so
there is nothing in the frame to reject. That acceptance is pinned by
tests/unit/tests/sskr_duplicate_member_index.c, which also pins the refusal
one layer down -- sskr_combine_shards() answers
SSKR_ERROR_DUPLICATE_MEMBER_INDEX and bolos_ux_sskr_combine() answers 0.

What no test pinned is the screen the user is then shown. In
src/bagl/nanox_enter_phrase.c the answer runs through `reconstructed`, a flag
distinct from the verdict itself:

    if (!reconstructed)   -> ux_sskr_invalid_flow   "SSKR Recovery"/"phrase invalid"
    else if (match)       -> ux_sskr_match_flow     "SSKR Phrase"/"is correct"
    else                  -> ux_sskr_nomatch_flow   "SSKR Phrase"/"doesn't match"

Collapsing the first branch into the third is the failure this test exists to
catch, and it is a plausible one -- combination failed, so the reconstructed
seed does not match, and reporting a mismatch looks locally reasonable. It
would be the wrong answer: "doesn't match" tells the user their shares belong
to some other seed, sending them to look for the wrong shares, when what
actually happened is that they entered one share twice and need a second,
different share. Both branches refuse; only one of them says something true.

The two lines are both asserted because the two screens differ on both of
them, so either line alone would leave the other branch passing.
"""

from pytest import fixture
from pytest import mark
from pytest import skip
from ledgered.devices import DeviceType
from ragger.conftest import configuration
import nano

# https://github.com/BlockchainCommons/crypto-commons/blob/master/Docs/sskr-test-vector.md#128-bit-seed
DEVICE_PHRASE = "fly mule excess resource treat plunge nose soda reflect adult ramp planet"

# The first shard of the 2-of-3 split of that seed used by
# test_sskr_128bit.py and test_sskr_entry_two_button.py. Entered twice below.
# Its member-threshold nibble is what tells the device to ask for two shares,
# so the entry loop runs to completion exactly as it would for a genuine pair.
SHARD = ("tuna next keep gyro paid claw able acid able jowl chef drum judo pool "
         "lion keep idle cusp iced rust fact view twin very pose epic whiz jump jury")


@fixture(scope='session')
def set_seed():
    configuration.OPTIONAL.CUSTOM_SEED = DEVICE_PHRASE


@mark.use_on_backend("speculos")
def test_sskr_duplicate_share_is_refused_as_invalid(device, backend, navigator,
                                                    set_seed):
    if device.type == DeviceType.NANOS:
        # Speculos has dropped Nano S; the build is still produced and this
        # test would drive it unchanged if the emulator returns.
        skip("Nano S is not emulated by the current Speculos")
    if device.type not in (DeviceType.NANOX, DeviceType.NANOSP):
        skip("Two-button devices only; the touch path is covered elsewhere")

    nano.select_in_menu(navigator, "Check SSKR")
    nano.confirm(backend)

    for _ in range(2):
        for word in SHARD.split():
            nano.enter_word(backend, word)

    nano.wait_for_lines(backend, "SSKR Recovery", "phrase invalid")
