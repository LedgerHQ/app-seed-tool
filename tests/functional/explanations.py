"""Driving the screens that explain a journey before it asks anything.

Six of them, seven screens: a bold title over rows that each carry their own
icon, built from the layout API -- `display_explanation_page()` in
src/nbgl/ui.c. Two facts side by side read as two facts; folded into one
centered paragraph the second is the tail of a sentence about something else.

**Two controls, and the difference is the point.** A screen that only carries
on into more reading has no button at all: the whole content is tappable and a
grey "Continue" names the gesture (nbgl_layoutDescription_t::tapActionText).
A screen that leads to an act -- entering a Phrase, entering Shares -- keeps
the black button, because that is what this application uses for an act with a
consequence.

**Why only the button screens are checked for overflow.** The two controls fail
differently, and that was measured rather than assumed. A row too tall for a
screen ending on a black button is drawn *behind* the pill, with every text
event still reported at full height and full text -- silent, and the reason
reviews.assert_body_clears_button() reads the pill's top edge out of the
screenshot pixels. A row too tall for a screen ending on the grey text is not
silent at all: the layout asks to draw outside the screen and Speculos'
hal_draw_rect() assertion kills the run, so any test that so much as visits the
screen already fails. Adding a check there would only duplicate it -- and an
event-based one cannot work anyway, because Speculos keeps reporting the events
of the screen before, so an explanation reached from a keypad still lists that
keypad's digits at the bottom of the screen.

**Why these helpers touch a label rather than call a use-case method.** ragger's
UseCaseChoice and ChoiceList drive components these screens are not -- an
explanation is nbgl_layoutGet() with a left content, not a choice and not a
list -- and the screen-change notification their helpers wait on does not
arrive in time. So the control's own drawn rectangle is touched, and the destination
is polled for -- the application only processes a touch when it receives a
tick, so reading straight after a tap returns the page that was already there.
"""
import time

import reviews

# The title of each screen. Matched against the whole screen with the wrapping
# removed, so a title that wraps is still one string here.
# There is no EXPLAIN_CHECK: that journey goes straight from the menu to the
# phrase length, as app-recovery-check does.
EXPLAIN_BACKUP = "How the backup works"
EXPLAIN_NUMBERS = "How many Shares"
EXPLAIN_THRESHOLD = "What is a threshold"
EXPLAIN_RECOVER = "How recovery works"
EXPLAIN_BIP85 = "How BIP85 works"
EXPLAIN_INDEX = "What is an index"

# The button that carries on. Every screen but the last of the Backup journey
# says "Continue"; that one names the act it leads to.
CONTINUE = "Continue"
ENTER_PHRASE = "Enter Recovery Phrase"

# No explanation is longer than this, and a walk that does not find its button
# within it has gone somewhere else.
_MAX_PAGES = 4

_POLL_TRIES = 60
_POLL_SLEEP = 0.2

# How long to give a press to land on its destination before concluding that it
# landed on the next screen of a chain instead. A redraw is fast; this only has
# to outlast one.
_ARRIVAL_TRIES = 25


def _texts(backend):
    return [event["text"] for event in
            backend.get_current_screen_content()["events"]]


def _flat(backend):
    """The screen's whole text, wrapping removed.

    Matching a prefix against one drawn line looks precise and is brittle: the
    line breaks where the renderer decides, so "Your Recovery Phrase will"
    fails on a screen that reads exactly that, because the first line stopped
    at "Your Recovery Phrase ". Joining the events and collapsing whitespace
    matches the sentence the reader sees rather than the shape it was drawn in.
    """
    return " ".join(" ".join(_texts(backend)).split())


def _on_screen(backend, label):
    return " ".join(label.split()) in _flat(backend)


def _touch(backend, label):
    for event in backend.get_current_screen_content()["events"]:
        if event["text"].startswith(label):
            backend.finger_touch(event["x"] + event["w"] // 2,
                                 event["y"] + event["h"] // 2)
            return
    raise AssertionError(
        f"{label!r} is not on screen; saw {_texts(backend)}")


def _wait_for(backend, expected):
    for _ in range(_POLL_TRIES):
        if _on_screen(backend, expected):
            return
        time.sleep(_POLL_SLEEP)
    raise AssertionError(f"{expected!r} never came up; saw {_flat(backend)!r}")


def pass_explanation(backend, device, title, then, button=CONTINUE,
                     action=False):
    """Read an explanation, walk its pages, and press the button that ends it.

    `title` is the first line of the *first* page and `then` the first line of
    the screen this leads to. Asserting both is what makes this a navigation
    step rather than a blind tap: a screen that failed to appear would
    otherwise leave the next touch landing on whatever was underneath.

    Both are matched against the screen's whole text with the wrapping
    removed, not against a single drawn line and not with re.match as
    wait_for_text_on_screen does -- so they take the sentence as read, never an
    escaped pattern and never a guess about where a line breaks.

    `action` says the last screen ends on a black button rather than on the
    grey tap-to-continue, which is what decides how its layout is checked.

    The walk presses on until `then` is what came up, which is how a one-screen
    and a two-screen explanation are driven by the same call: the caller says
    where it starts and where it ends, not how many screens are in between.
    Every screen it passes is checked for overflow, not only the last.

    Arrival is what ends the walk, deliberately, and not the sight of the final
    button. Six of the seven explanations end on "Continue", which is also what
    every screen before the last says -- so a walk that stopped at the button's
    label would leave the first screen of a chain believing it was the last.
    """
    # Polled, not waited on: wait_for_text_on_screen() waits for a screen
    # *change*, and an explanation reached from a keypad has often already been
    # drawn by the time this is called.
    _wait_for(backend, title)
    for _ in range(_MAX_PAGES):
        label = button if _on_screen(backend, button) else CONTINUE
        # Which check applies is decided by which control is drawn, and that
        # cannot be read off the label alone: the screen that leads to entering
        # Shares carries a black button that also says "Continue". `action`
        # names the screen that ends on a button, and only its own label is the
        # button -- everything before it is the grey text.
        if action and label == button:
            reviews.assert_body_clears_button(backend, label)
        before = _texts(backend)
        _touch(backend, label)
        # Poll for the destination, not for the change. A screen that has begun
        # to be redrawn is already "different" while its title is not yet
        # drawn, and reading once at that moment made the walk loop round and
        # check a half-drawn keypad's layout against its own "Continue".
        for _ in range(_ARRIVAL_TRIES):
            if _on_screen(backend, then):
                return
            time.sleep(_POLL_SLEEP)
        if _texts(backend) == before:
            raise AssertionError(
                f"pressing {label!r} changed nothing; still on {_flat(backend)!r}")
    raise AssertionError(
        f"{then!r} was not reached in {_MAX_PAGES} screens from {title!r}; "
        f"last saw {_flat(backend)!r}")
