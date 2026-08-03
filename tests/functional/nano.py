"""
Navigation for the two-button devices, driven by what is on the screen.

The BAGL screens this file drives are not a layout variant of the touch ones:
a word is entered one letter at a time through a three-slot carousel, and the
word itself is then picked from a second carousel of candidates. How many
clicks that takes depends on which letters the wordlist still allows after
everything typed so far -- so it depends on the word, on the wordlist, and on
where the carousel happened to start.

Writing that out as a list of `NavInsID.RIGHT_CLICK` is possible, and this
repository already contains five such lists, each a couple of hundred entries
long. They are unreadable, they cannot be reused for another phrase, and a
list transcribed for one screen size does not survive the other: the 128x64
devices have an introductory screen the 128x32 one does not, and the 128x32
one has a "Restart from ..." entry in the word carousel that the 128x64 ones
do not, so the two carousels do not even have the same length.

The helpers below compute the clicks instead, from the screen. Entering a word
becomes `enter_word(backend, "planet")`, which is the same call whatever the
device, the wordlist or the phrase.

## How the screen is read

The carousel occupies one row with three slots at fixed x positions -- the
previous item on the left, the current one in the middle, the next one on the
right -- and the edges of the list simply leave a slot empty. So the current
item is not "the second text on the screen": it is the text whose x puts it in
the middle slot. That is what `_carousel_current()` looks for, and it is the
only geometric assumption in this file.
"""

from time import sleep, time

# The middle slot of the three-slot carousel. Measured on both 128x64 Nano
# devices, where the slots sit at x = 34, 62 and 90 for a one-character item;
# the window is wide enough to hold an item of a few characters centred on the
# same place, and narrow enough never to catch the neighbouring slots.
_CAROUSEL_CURRENT_X = (52, 78)

# The letter carousel row. The title sits above it and the word being typed
# below, so the row alone identifies the letters.
_CAROUSEL_Y = (24, 38)

# Enough clicks to walk any carousel this application shows -- the longest is
# the 26 letters of the alphabet -- with room to spare, and low enough that a
# helper looking for something that is not there fails in seconds.
_MAX_CLICKS = 64


def _events(backend):
    return backend.get_current_screen_content()["events"]


def _texts(backend):
    return [event.get("text", "") for event in _events(backend)]


def _screen(backend):
    """A comparable snapshot of the screen, used to detect that a click did
    something."""
    return tuple((e.get("x"), e.get("y"), e.get("text")) for e in _events(backend))


def _click(backend, button, settle=2.0):
    """Press a button and wait for the screen to settle.

    A click that changes nothing is not an error here -- the end of a carousel
    is one -- so this returns whether the screen changed rather than raising.
    """
    before = _screen(backend)
    {"left": backend.left_click,
     "right": backend.right_click,
     "both": backend.both_click}[button]()
    deadline = time() + settle
    while time() < deadline:
        sleep(0.05)
        if _screen(backend) != before:
            return True
    return False


def _carousel_current(backend):
    """The item currently under selection, or None if the screen has no
    carousel."""
    for event in _events(backend):
        x, y = event.get("x"), event.get("y")
        if x is None or y is None:
            continue
        if _CAROUSEL_CURRENT_X[0] <= x <= _CAROUSEL_CURRENT_X[1] \
                and _CAROUSEL_Y[0] <= y <= _CAROUSEL_Y[1]:
            return event.get("text", "")
    return None


def _title(backend):
    """The topmost line of the screen."""
    events = [e for e in _events(backend) if e.get("y") is not None]
    if not events:
        return ""
    return min(events, key=lambda e: e["y"]).get("text", "")


def select_in_menu(backend, label, backwards=False):
    """Walk a one-item-at-a-time list until `label` is on screen, then choose it.

    Used for the home menu and for every list the flows put in front of a
    choice. `label` is matched as a prefix of any line, so "Check BIP39"
    finds the entry whose second line is "recovery phrase".
    """
    for _ in range(_MAX_CLICKS):
        if any(text.startswith(label) for text in _texts(backend)):
            _click(backend, "both")
            return
        _click(backend, "left" if backwards else "right")
    raise AssertionError(
        f"'{label}' never appeared; last screen was {_texts(backend)}")


def enter_letter(backend, letter):
    """Move the letter carousel onto `letter` and validate it.

    The carousel only offers letters that still lead to a word, and it offers
    them in alphabetical order -- so comparing the wanted letter with the
    current one says which way to walk, and a letter the wordlist does not
    allow makes this fail rather than loop.
    """
    for _ in range(_MAX_CLICKS):
        current = _carousel_current(backend)
        if current is None:
            raise AssertionError(
                f"no letter carousel on screen: {_texts(backend)}")
        if current == letter:
            _click(backend, "both")
            return
        moved = _click(backend, "right" if letter > current else "left")
        if not moved:
            raise AssertionError(
                f"'{letter}' is not offered after the letters already entered; "
                f"the carousel stopped at '{current}'")
    raise AssertionError(f"could not reach letter '{letter}'")


def enter_word(backend, word):
    """Enter one word of a phrase, however many letters that takes.

    The application switches from the letter carousel to the candidate-word
    carousel on its own, as soon as what has been typed is specific enough --
    which is why this types letters until the screen stops being a letter
    screen, rather than typing a fixed prefix length.
    """
    for letter in word:
        if not _title(backend).startswith("Enter"):
            break
        enter_letter(backend, letter)

    if _title(backend).startswith("Enter"):
        raise AssertionError(
            f"'{word}' was typed in full and the word list never opened: "
            f"{_texts(backend)}")

    for _ in range(_MAX_CLICKS):
        if word in _texts(backend):
            _click(backend, "both")
            return
        if not _click(backend, "right"):
            raise AssertionError(
                f"'{word}' is not among the candidates; the list stopped at "
                f"{_texts(backend)}")
    raise AssertionError(f"could not reach candidate '{word}'")


def enter_phrase(backend, phrase):
    """Enter a whole BIP-39 phrase."""
    for word in phrase.split():
        enter_word(backend, word)


def wait_for_lines(backend, *lines, timeout=10.0):
    """Assert that every one of `lines` is on the same screen.

    Two verdict screens of this application differ only by their second line
    -- "is correct" against "doesn't match" -- so asserting the first line
    alone asserts nothing about the verdict.
    """
    deadline = time() + timeout
    while True:
        texts = _texts(backend)
        if all(any(text.startswith(line) for text in texts) for line in lines):
            return
        if time() > deadline:
            raise AssertionError(
                f"expected {list(lines)} on one screen, last saw {texts}")
        sleep(0.1)
