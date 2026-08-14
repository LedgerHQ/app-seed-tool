#!/usr/bin/env python3
"""Speculos regression check: do *generated* SSKR shares actually get
cleared from RAM on the real generate -> view -> dashboard flow?

sskr_shares_from_bip39_mnemonic() (src/nbgl/sskr_shares.c) writes generated
SSKR shares into the same `shares.buffer` used by manual share entry. Its
caller, sskr_shares_check(), has an explicit comment: "Don't clear the
shares just yet as we may need it to generate BIP39 mnemonic" -- the erase
is *deliberately* deferred. This script answers the open question: does the
user, going through the actual generate -> view -> return-to-dashboard
flow, always end up hitting sskr_shares_reset() before the buffer goes out
of scope -- or is there a real exit path that skips it?

Scenario: type and confirm the 12-word mnemonic that matches the device's
own seed (so BIP39 check succeeds with seed_match=true), choose "Generate
SSKR", 3 shares / threshold 2, page through all 3 generated shares, then
tap the review's exit/quit control to return to the dashboard. Read the
real `shares.buffer` over a GDB connection right after generation (should
be non-trivial share data) and right after the exit tap, at whichever
breakpoint(s) actually fire.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_sskr_generated_shares_dashboard_return.py

Exit code 0 and "PASS" if sskr_shares_reset() fires on this path and wipes
shares.buffer. Exit code 1 and "FAIL" if the buffer still contains generated
share data once you're back at the dashboard, OR if sskr_shares_reset()
never fires at all despite genuinely reaching the dashboard -- both are real
findings, not test infrastructure problems.
"""
import json
import os
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rsp_client import RSP, wait_for_gdb
import screens

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ELF_PATH = os.path.join(REPO_ROOT, "build", "flex", "bin", "app.elf")
BUILDER_IMAGE = "ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest"
SPECULOS_IMAGE = "ghcr.io/ledgerhq/speculos:latest"
CONTAINER_NAME = "speculos-sskr-generated-shares-reset-test"
API_PORT = 5004
GDB_PORT = 1237
BASE_URL = f"http://localhost:{API_PORT}"

# Same 128-bit BlockchainCommons test vector already used by
# verify_compare_recovery_phrase_cleanup.py and
# tests/functional/test_bip39_12word.py. Booting Speculos with this as the
# device seed (-s) means the mnemonic typed on screen matches the device's
# own seed, so bip39_mnemonic_check() succeeds with seed_match=true and the
# UI actually offers "Generate SSKR" -- it doesn't otherwise.
MNEMONIC = ("fly mule excess resource treat plunge nose soda reflect adult "
           "ramp planet")
MNEMONIC_WORDS = MNEMONIC.split()

CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

SIGILL = 4
SOCKET_TIMEOUT = 40

# FLEX (480x600) touch-keyboard letter positions, from Ledger's own
# `ragger` package (ragger/firmware/touch/positions.py,
# POSITIONS["LetterOnlyKeyboard"][DeviceType.FLEX]) -- same table already
# used by verify_compare_recovery_phrase_cleanup.py and
# verify_sskr_share_cancel_clears_buffer.py.
FLEX_KEYS = {
    "q": (24, 415), "w": (72, 415), "e": (120, 415), "r": (168, 415),
    "t": (216, 415), "y": (264, 415), "u": (312, 415), "i": (360, 415),
    "o": (408, 415), "p": (456, 415),
    "a": (48, 490), "s": (96, 490), "d": (144, 490), "f": (192, 490),
    "g": (240, 490), "h": (288, 490), "j": (336, 490), "k": (384, 490),
    "l": (432, 490),
    "z": (24, 565), "x": (72, 565), "c": (120, 565), "v": (168, 565),
    "b": (216, 565), "n": (264, 565), "m": (312, 565),
}
SUGGESTION_1 = (140, 300)


# UseCaseChoice.confirm ("Generate SSKR" on the "Generate SSKR Phrase?"
# screen) and the check-result page's CenteredFooter dismiss tap, both from
# the same ragger positions table.
GENERATE_SSKR_CONFIRM = (240, 435)
CHECK_RESULT_DISMISS = (240, 550)

# UseCaseViewDetails FLEX: next/exit, the exact sequence this repo's own
# tests/functional/conftest.py (all_eink_bip39_12word) already proves works
# on this exact 3-share/threshold-2 review screen.
REVIEW_NEXT = (430, 550)
REVIEW_EXIT = (55, 530)


def die(msg):
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def check_build():
    if not os.path.isfile(ELF_PATH):
        die(
            "build/flex/bin/app.elf not found. Build it first:\n\n"
            '  docker run --rm -v "$(pwd)":/app \\\n'
            f"    {BUILDER_IMAGE} \\\n"
            "    bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'\n"
        )


def docker_run_tool(*args):
    return subprocess.run(
        ["docker", "run", "--rm", "-v", f"{os.path.dirname(ELF_PATH)}:/app",
         BUILDER_IMAGE, *args],
        capture_output=True, text=True, check=True,
    ).stdout


def resolve_symbols():
    """nm -S the built ELF (never hardcode addresses -- they shift on any
    source change to this file). Returns {name: (address, size)}."""
    out = docker_run_tool("nm", "-S", "/app/app.elf")
    wanted = {"sskr_shares_reset", "sskr_shares_from_bip39_mnemonic", "shares"}
    syms = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] in wanted:
            syms[parts[3]] = (int(parts[0], 16), int(parts[1], 16))
        elif len(parts) == 3 and parts[2] in wanted:
            syms[parts[2]] = (int(parts[0], 16), 0)
    missing = wanted - syms.keys()
    if missing:
        die(f"could not resolve symbols in app.elf: {missing} "
            "(did sskr_shares.c change function/variable names?)")
    return syms


def resolve_bss_base():
    out = docker_run_tool("readelf", "-S", "/app/app.elf")
    for line in out.splitlines():
        parts = line.split()
        if ".bss" in parts:
            return int(parts[parts.index(".bss") + 2], 16)
    die("could not find .bss section in app.elf")


def code_runtime_addr(link_addr):
    even = link_addr & ~1
    return CODE_RUNTIME_BASE + (even - CODE_LINK_BASE)


def prepare_patched_speculos_main(scratch_dir):
    patched = os.path.join(scratch_dir, "main.py")
    cid = subprocess.run(["docker", "create", SPECULOS_IMAGE],
                          capture_output=True, text=True, check=True).stdout.strip()
    try:
        subprocess.run(["docker", "cp", f"{cid}:/speculos/speculos/main.py", patched], check=True)
    finally:
        subprocess.run(["docker", "rm", cid], capture_output=True)
    with open(patched) as f:
        content = f.read()
    if "'-singlestep'" not in content:
        die("speculos main.py no longer contains '-singlestep' -- "
            "the image changed, this patch needs re-checking by hand")
    content = content.replace("'-singlestep'", "'-one-insn-per-tb'")
    with open(patched, "w") as f:
        f.write(content)
    return patched


def start_container(patched_main_py):
    subprocess.run(["docker", "rm", "-f", CONTAINER_NAME], capture_output=True)
    subprocess.run([
        "docker", "run", "-d", "--name", CONTAINER_NAME,
        "-p", f"{GDB_PORT}:1234", "-p", f"{API_PORT}:5001",
        "-v", f"{os.path.dirname(ELF_PATH)}:/app",
        "-v", f"{patched_main_py}:/speculos/speculos/main.py",
        SPECULOS_IMAGE, "-m", "flex", "--display", "headless",
        "--api-port", "5001", "-s", MNEMONIC, "-d", "/app/app.elf",
    ], check=True, capture_output=True)


def stop_container():
    subprocess.run(["docker", "rm", "-f", CONTAINER_NAME], capture_output=True)


def tap(x, y, delay=0.1):
    body = json.dumps({"action": "press-and-release", "x": x, "y": y, "delay": delay}).encode()
    req = urllib.request.Request(f"{BASE_URL}/finger", data=body,
                                  headers={"Content-Type": "application/json"}, method="POST")
    urllib.request.urlopen(req).read()
    time.sleep(0.4)


def screen_texts():
    data = json.loads(urllib.request.urlopen(f"{BASE_URL}/events?currentscreenonly=true").read())
    return [e["text"] for e in data["events"]]


class MemorySampler:
    """Same entry+LR-return breakpoint pattern as
    verify_bip39_cancel_clears_buffer.py / verify_sskr_share_cancel_clears_buffer.py,
    applied to sskr_shares_from_bip39_mnemonic (generation) and
    sskr_shares_reset (the function whose *whether it fires at all* is the
    actual open question here)."""

    def __init__(self, watch_addrs, shares_offset, read_len):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.watch = watch_addrs  # {runtime_addr: name}
        self.shares_offset = shares_offset
        self.read_len = read_len
        self.snapshots = []  # (name, before_bytes, after_bytes), in hit order
        self._pending_return = None
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        for addr in self.watch:
            self.rsp.insert_bp(addr, kind=2)
        self.rsp.cont()
        self._thread.start()

    def _pump(self):
        while True:
            try:
                reply = self.rsp.wait_stop()
            except socket.timeout:
                continue
            except (ConnectionError, OSError):
                return

            try:
                if self.rsp.stop_signal(reply) == SIGILL:
                    self.rsp.cont_with_signal(SIGILL)
                    continue

                regs = self.rsp.read_regs()
                pc = regs[15] & ~1
                shares_addr = regs[9] + self.shares_offset

                if self._pending_return and pc == self._pending_return[0]:
                    ret_addr, name, before = self._pending_return
                    after = self.rsp.read_mem(shares_addr, self.read_len)
                    self.snapshots.append((name, before, after))
                    self.rsp.remove_bp(ret_addr, kind=2)
                    self._pending_return = None
                    self.rsp.cont()
                    continue

                if pc in self.watch:
                    name = self.watch[pc]
                    ret_addr = regs[14] & ~1
                    before = self.rsp.read_mem(shares_addr, self.read_len)
                    self.rsp.insert_bp(ret_addr, kind=2)
                    self._pending_return = (ret_addr, name, before)
                    self.rsp.remove_bp(pc, kind=2)
                    self.rsp.step()
                    self.rsp.insert_bp(pc, kind=2)
                    self.rsp.cont()
                    continue

                self.rsp.cont()
            except (ConnectionError, OSError):
                return


def type_word(word):
    for letter in word:
        tap(*screens.LETTERS[letter])


def type_and_confirm_word(word):
    """Type a full word and tap the first suggestion. Screen redraws under
    Speculos's -one-insn-per-tb debug mode + SIGILL-passthrough overhead
    can lag tens of seconds behind the actual taps (confirmed in scenario
    1) -- poll patiently rather than assume a short timeout means a
    dropped/stuck key."""
    type_word(word)
    texts = []
    for _ in range(200):
        texts = screen_texts()
        if word in texts:
            break
        time.sleep(0.5)
    else:
        die(f"expected '{word}' suggestion on screen after typing it, got: "
            f"{texts} -- navigation/keyboard-mapping assumption is wrong")
    tap(*screens.SUGGESTION_1)


def long_press_until(x, y, target, timeout_s=240, poll=0.5):
    """Hold the confirm button until the screen it leads to appears.

    The review's confirm is a long press, not a tap: nbgl_useCaseGenericReview()
    ignores a plain tap, which is the point of it standing between the user and
    a secret. The SDK counts the hold on ticker events, and under GDB every one
    of those is slowed by the same factor as everything else -- so a hold of a
    fixed number of seconds is a race this loses on a slow host. Releasing early
    cancels the press, silently, and the run then fails much later with
    "generation was never reached".

    So hold, and watch the screen rather than the clock. Released either way,
    including on timeout, so the app is never left with a finger down.
    """
    def finger(action):
        body = json.dumps({"action": action, "x": x, "y": y}).encode()
        req = urllib.request.Request(f"{BASE_URL}/finger", data=body,
                                     headers={"Content-Type": "application/json"},
                                     method="POST")
        urllib.request.urlopen(req).read()

    finger("press")
    try:
        deadline = time.time() + timeout_s
        texts = []
        while time.time() < deadline:
            texts = screen_texts()
            if any(target in t for t in texts):
                return texts
            time.sleep(poll)
        die(f"long press on {target!r} never took within {timeout_s}s, "
            f"last seen: {texts}")
    finally:
        finger("release")
        time.sleep(1)


def wait_for_text(target, timeout_s=100, poll=0.5):
    deadline = time.time() + timeout_s
    texts = []
    while time.time() < deadline:
        texts = screen_texts()
        if any(target in t for t in texts):
            return texts
        time.sleep(poll)
    die(f"expected screen text containing {target!r} within {timeout_s}s, "
        f"last seen: {texts}")


def navigate_generate_view_and_return():
    """Coordinates are for the flex layout only (480x600); see README.md."""
    time.sleep(3)  # let the app finish booting before the first tap
    tap(*screens.HOME_ACTION)  # "Select an action"
    time.sleep(1)
    # Backing up is reached from the menu now, rather than by passing a check
    # and accepting an offer nobody asked for.
    tap(*screens.list_row(screens.MENU_BACKUP))  # "Generate Backup Shares"
    time.sleep(1)

    # Two explanations before the ask, and the difference in how they end is
    # deliberate: the first only leads to more reading, the second leads to an
    # act (see tests/functional/explanations.py).
    wait_for_text("backup", timeout_s=60)
    tap(*screens.CONTINUE_FOOTER)  # "How the backup works"
    time.sleep(1)
    wait_for_text("Phrase", timeout_s=60)
    tap(*screens.BLACK_BUTTON)  # "Why your Phrase?" -> "Enter Recovery Phrase"
    time.sleep(1)

    tap(*screens.list_row(screens.WORDS_12))  # "12 words"
    time.sleep(1)

    for word in MNEMONIC_WORDS:
        type_and_confirm_word(word)

    # bip39_mnemonic_check() should now report a match against the device's
    # own seed (booted with -s using this exact mnemonic) -- confirm the
    # result page really says so before proceeding, rather than assuming.
    # "Valid" alone: the title is "Valid\nRecovery Phrase" since the three
    # verdicts were given separate screens.
    wait_for_text("Valid", timeout_s=150)
    tap(*screens.RESULT_FOOTER)  # "Tap to continue"
    time.sleep(1)

    wait_for_text("Shares", timeout_s=60)
    tap(*screens.CONTINUE_FOOTER)  # "How many Shares?"
    time.sleep(1)

    wait_for_text("SSKR Shares", timeout_s=60)
    for d in "3":
        tap(*screens.KEYPAD[d])
    tap(*screens.KEYPAD["enter"])  # 3 shares
    time.sleep(1)

    wait_for_text("threshold", timeout_s=60)
    tap(*screens.CONTINUE_FOOTER)  # "What is a threshold?"
    time.sleep(1)

    wait_for_text("threshold", timeout_s=60)
    for d in "2":
        tap(*screens.KEYPAD[d])
    tap(*screens.KEYPAD["enter"])  # threshold 2
    time.sleep(1)

    # The review of what was chosen, then the warning and the long press.
    wait_for_text("Threshold", timeout_s=60)
    tap(*screens.REVIEW_NEXT)
    wait_for_text("Anyone who", timeout_s=60)
    long_press_until(*screens.REVIEW_LONG_PRESS, "SSKR Share")

    # Sanity check: this exact 3-share/threshold-2/this-mnemonic combination
    # is already exercised for real by tests/functional/test_bip39_12word.py,
    # which asserts the rendered share text starts with "tuna next keep
    # gyro" -- if that's not what we see, navigation or the mnemonic/seed
    # assumption is wrong, not a finding.
    texts = wait_for_text("SSKR Share", timeout_s=90)
    if not any("tuna next keep gyro" in t for t in texts):
        die("reached the SSKR share review screen but the rendered share "
            f"text doesn't match the known vector -- got: {texts}")

    # Known-good, already-proven-on-this-exact-screen exit sequence, per
    # tests/functional/conftest.py's all_eink_bip39_12word(): page through
    # all 3 shares, then exit. This is a realistic "user finished viewing
    # and returned to the dashboard" path, not a contrived shortcut.
    wait_for_text("SSKR Share", timeout_s=30)
    tap(*screens.SECRET_NEXT)  # share 1 -> 2
    time.sleep(1)
    wait_for_text("SSKR Share", timeout_s=30)
    tap(*screens.SECRET_NEXT)  # share 2 -> 3
    time.sleep(1)
    wait_for_text("SSKR Share", timeout_s=30)
    tap(*screens.SECRET_CLOSE)  # exit review -> review_done() -> back to dashboard
    time.sleep(2)


def main():
    check_build()
    print("Resolving symbols from build/flex/bin/app.elf ...")
    syms = resolve_symbols()
    bss_base = resolve_bss_base()
    shares_addr, shares_size = syms["shares"]
    shares_offset = shares_addr - bss_base
    read_len = 64
    watch = {
        code_runtime_addr(syms["sskr_shares_from_bip39_mnemonic"][0]):
            "sskr_shares_from_bip39_mnemonic",
        code_runtime_addr(syms["sskr_shares_reset"][0]): "sskr_shares_reset",
    }
    print(f"  shares buffer: offset R9+0x{shares_offset:x} (struct size {shares_size} bytes)")
    for addr, name in watch.items():
        print(f"  breakpoint: 0x{addr:x} ({name})")

    sampler = None
    with tempfile.TemporaryDirectory() as scratch:
        print("Patching Speculos debug image (singlestep -> one-insn-per-tb) ...")
        patched_main = prepare_patched_speculos_main(scratch)

        print(f"Starting Speculos (device seed: \"{MNEMONIC}\") ...")
        start_container(patched_main)
        try:
            if not wait_for_gdb(GDB_PORT):
                die(f"Speculos GDB stub on port {GDB_PORT} did not "
                    "become ready in time")
            sampler = MemorySampler(watch, shares_offset, read_len)
            sampler.start()
            print("Navigating: home -> Generate Backup Shares -> two "
                  "explanations -> 12 words -> type+confirm mnemonic -> "
                  "3 shares, threshold 2 -> review -> long press -> "
                  "page through 3 shares -> exit to dashboard ...")
            print("(this is slow -- every guest syscall round-trips through this "
                  "script while GDB is attached; a full run of the 12-word entry "
                  "alone can take 15-30+ minutes, see README.md)")
            navigate_generate_view_and_return()
            # give sskr_shares_reset (if it fires at all) time to be caught
            for _ in range(60):
                names = [n for n, _, _ in sampler.snapshots]
                if "sskr_shares_reset" in names:
                    break
                time.sleep(0.5)
        finally:
            stop_container()

    if sampler is None or not sampler.snapshots:
        die("no breakpoint ever fired -- navigation didn't reach the expected "
            "code paths (did the UI flow change?)")

    print(f"\nCaptured {len(sampler.snapshots)} snapshots:")
    gen_hit = None
    reset_hit = None
    for name, before, after in sampler.snapshots:
        print(f"  {name}: before={before.hex()} after={after.hex()}")
        if name == "sskr_shares_from_bip39_mnemonic" and gen_hit is None:
            gen_hit = (before, after)
        if name == "sskr_shares_reset" and reset_hit is None:
            reset_hit = (before, after)

    if gen_hit is None:
        die("sskr_shares_from_bip39_mnemonic was never hit -- share "
            "generation was never reached")
    gen_after = gen_hit[1]
    nz = sum(1 for b in gen_after if b)
    print(f"\n  sskr_shares_from_bip39_mnemonic returned with "
          f"{nz}/{read_len} nonzero bytes in shares.buffer")
    if nz == 0:
        die("shares.buffer reads all-zero right after "
            "sskr_shares_from_bip39_mnemonic() returns -- generation didn't "
            "actually produce data, this is a test-setup problem, not the "
            "bug under test")
    print("  confirmed: real generated share data is present in RAM right "
          "after generation (as expected -- this is what makes the next "
          "check meaningful)")

    if reset_hit is None:
        print("\nFAIL: sskr_shares_reset() was never hit, despite the "
              "navigation completing its full page-through-and-exit "
              "sequence on the SSKR share review screen. That means the "
              "generated share data captured above is exactly what's left "
              "resident in RAM on this return-to-dashboard path -- a real "
              "gap, not a test infrastructure problem.")
        print(f"  shares.buffer right after generation: {gen_after.hex()}")
        sys.exit(1)

    reset_after = reset_hit[1]
    reset_nz = sum(1 for b in reset_after if b)
    if reset_nz != 0:
        print(f"\nFAIL: sskr_shares_reset() fired, but shares.buffer still "
              f"has {reset_nz}/{read_len} nonzero bytes after it returns:")
        print(f"  {reset_after.hex()}")
        sys.exit(1)

    print("\nPASS: sskr_shares_reset() fired on the page-through-and-exit "
          "path and shares.buffer reads all-zero afterward -- the deferred "
          "erase documented in sskr_shares_check() does get executed on "
          "this real user flow")
    sys.exit(0)


if __name__ == "__main__":
    main()
