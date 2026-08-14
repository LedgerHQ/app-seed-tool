"""Driving the screens this application now puts in front of anything it
reveals or produces.

Three shapes, and they are navigated differently:

  * a **review** (nbgl_useCaseGenericReview) is one or more pages of
    tag/value pairs followed by a page carrying a single long-press button. The pages are
    walked with the review's own "tap" area; the last one is recognised by the
    button's text, and pressed by touching that text's own rectangle rather
    than a position from a table -- the button sits at a different height on
    each of the three touch devices, and reading it off the screen is what
    keeps this file from carrying a fourth copy of those coordinates;

  * a **warning** (nbgl_useCaseChoice) is a single page with two buttons, which
    is exactly what ragger's UseCaseChoice drives;

  * the **close confirmation** on the shares is the same component, and is
    given its own function only because "Close" destroys the shares and "Back"
    does not: a test that means one should not be able to reach the other by
    editing a string.

Every helper asserts the screen it expects before acting on it. A helper that
touched blind would turn a screen that failed to appear into a touch landing
on whatever was underneath, which is the failure mode hardest to read from a
campaign log.
"""

from ragger.firmware.touch.use_cases import UseCaseChoice, UseCaseReview

# A review has at most three pages in this application: the BIP85 one, whose
# four rows fit one page, plus its finish page -- and the SSKR one, which is
# the same shape. Six is room for the layout changing under us without this
# turning into an infinite walk.
_MAX_REVIEW_PAGES = 6

# Long enough to clear the SDK's long-press threshold with room to spare.
_LONG_PRESS_SECONDS = 3.0


def _events(backend):
    return backend.get_current_screen_content()["events"]


def _texts(backend):
    return [event["text"] for event in _events(backend)]


def _on_screen(backend, text):
    return any(drawn.startswith(text) for drawn in _texts(backend))


def screen_text(backend):
    """The screen's text with the renderer's line breaks removed.

    Joined with nothing between the events, deliberately. A tag/value pair
    wraps where the renderer decides and on characters, not on words, so a
    derivation path arrives as two events -- Flex draws
    "m/83696968'/89101'/10" and "'/6'/0'" -- and a value asserted against one
    drawn line would be an assertion about the font. Joining with a space
    instead would put one inside the path.

    Use this rather than wait_for_text_on_screen() for anything containing a
    parenthesis: that helper matches with re.match(), so "PIN (Index #0)"
    asks for a group and matches "PIN Index #0".
    """
    return "".join(_texts(backend))


def assert_on_screen(backend, text):
    joined = screen_text(backend)
    assert text in joined, f"{text!r} is not on screen; saw {joined!r}"


def assert_not_on_screen(backend, text):
    joined = screen_text(backend)
    assert text not in joined, f"{text!r} is still on screen; saw {joined!r}"


def assert_body_clears_button(backend, button_text):
    """Fail if the button is drawn over the end of the description.

    nbgl_useCaseChoice() lays its description out from the top and puts the
    buttons at a fixed height. It does not shorten, scroll or paginate the text
    to make room, so a description one line too long is *overprinted* by the
    button: the last line keeps its pixels down to where the black pill starts
    and loses the rest. On Flex that is how "...not necessarily the one present
    on this Ledger device." lost its final line.

    Nothing in the screen's event list can see it. Every text event is still
    reported, at its full height, with its full text -- so every assertion on
    the wording passes, and so does a comparison against the button's own
    rectangle, because that rectangle is the *label* and the label sits well
    inside the pill that does the overprinting. The shape is not an event.

    So the pill's top edge is read from the image: scanning up from the label,
    the first row that stops being mostly dark is where it begins. Any text
    reaching past that row is text the user cannot finish reading.
    """
    from io import BytesIO
    from PIL import Image

    events = _events(backend)
    button = next((e for e in events if e["text"].startswith(button_text)), None)
    assert button is not None, (
        f"{button_text!r} is not on screen; saw {_texts(backend)}")

    image = Image.open(BytesIO(backend._client.get_screenshot())).convert("L")
    width, _ = image.size
    pixels = image.load()
    sampled = range(0, width, 2)

    def mostly_dark(y):
        dark = sum(1 for x in sampled if pixels[x, y] < 96)
        return dark > 0.6 * len(list(sampled))

    pill_top = 0
    for y in range(button["y"], -1, -1):
        if not mostly_dark(y):
            pill_top = y + 1
            break

    # Only text rows are candidates. A keypad reports each of its keys with the
    # rectangle of the whole key area -- 352px tall on Flex -- so on a screen
    # this was called on by mistake every key would look like text buried under
    # the button. No row of an explanation or a warning is near that tall.
    max_text_row_height = 100
    overprinted = [e for e in events
                   if e is not button and e["h"] <= max_text_row_height
                   and e["y"] + e["h"] > pill_top and e["y"] < pill_top]
    assert not overprinted, (
        "the {!r} button starts at y={} and is drawn over text that ends "
        "below it -- the user cannot read the end of: {}".format(
            button_text, pill_top,
            [(e["text"], e["y"], e["h"]) for e in overprinted]))



def approve(backend, device, finish_text):
    """Walk a review to its last page and press the button that ends it.

    That last page is the warning: it carries the alert icon, the sentence
    about what is going to be drawn, and this button. There is no separate
    warning screen after it, which is why this function is the whole of
    "accept what the review offered".

    `finish_text` is the button's label, and is also what identifies the last
    page -- the pages before it carry tag/value pairs and no button.

    That button is held, not tapped, and UseCaseReview.confirm() is what holds
    it: the INFO_LONG_PRESS content nbgl_useCaseGenericReview() draws really
    does require the hold, where the nbgl_useCaseStaticReviewLight() this
    replaced drew a button labelled as a long press but confirmed on a simple
    tap. ragger holds the widget's own position for 3s, which is why this no
    longer touches the label's rectangle.

    The label is matched against the page's whole text rather than against a
    single drawn line: inside the long-press widget it is left-aligned beside
    the round button and wraps, so "Derive this secret" arrives as two events
    and no one of them starts with it.

    The wait after each tap is not decoration. get_current_screen_content()
    reads whatever Speculos last drew, and the application only processes a
    touch when it receives a tick, so reading straight after a tap returns the
    page that was already there and the walk stalls on it.
    """
    review = UseCaseReview(backend, device)
    wanted = " ".join(finish_text.split())
    for _ in range(_MAX_REVIEW_PAGES):
        if wanted in " ".join(" ".join(_texts(backend)).split()):
            review.confirm()
            return
        review.tap()
        backend.wait_for_screen_change()
    raise AssertionError(
        f"{finish_text!r} was not reached in {_MAX_REVIEW_PAGES} review pages; "
        f"last saw {_texts(backend)}")


def reject(backend, device):
    """Refuse a review.

    One gesture, and it lands on the home page: nbgl_useCaseGenericReview()
    calls the reject callback directly, so there is none of the "Reject
    operation?" confirmation that nbgl_useCaseReview() puts in front of a
    rejection. The footer reads "Cancel" rather than "Reject" because this use
    case, unlike the one it replaced, honours the text it is given.
    """
    UseCaseReview(backend, device).reject()


def accept_warning(backend, device, title, timeout=5):
    """Read a standalone reveal warning and continue past it.

    Only one path still has one: the SSKR reconstitution, which has no review
    for the warning to be the last page of. The generation and derivation
    flows carry theirs on the review's own final page -- see approve().
    """
    backend.wait_for_text_on_screen(title, timeout)
    assert_body_clears_button(backend, "Continue anyway")
    UseCaseChoice(backend, device).confirm()


def decline_warning(backend, device, title, timeout=5):
    """Read a reveal warning and go back to safety.

    Declining reaches display_home_page() and so reset_globals(): this is the
    gesture a test asserts *nothing* was drawn after.
    """
    backend.wait_for_text_on_screen(title, timeout)
    assert_body_clears_button(backend, "Continue anyway")
    UseCaseChoice(backend, device).reject()


# The confirmation's title wraps -- Flex draws "Close and erase " and
# "all 3 Shares?" as two events -- so what is waited on has to be a prefix of
# one drawn line rather than of the sentence.
_CLOSE_TITLE = "Close and erase"


def close_shares(backend, device, timeout=5):
    """Answer the shares' close confirmation with "Yes, close", which destroys them."""
    backend.wait_for_text_on_screen(_CLOSE_TITLE, timeout)
    UseCaseChoice(backend, device).confirm()


def keep_shares(backend, device, timeout=5):
    """Answer it with "Go back to Shares", which returns them without regenerating."""
    backend.wait_for_text_on_screen(_CLOSE_TITLE, timeout)
    UseCaseChoice(backend, device).reject()
