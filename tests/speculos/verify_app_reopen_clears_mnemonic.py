#!/usr/bin/env python3
"""Speculos regression check: closing the app and starting a brand-new
process must not leave a previous session's typed mnemonic content behind
in RAM.

Unlike the other five scripts in this directory, this one is not chasing an
application bug. `git grep -n "nvm_write\\|nvm_erase" -- src/` finds nothing:
this app never writes to NVM/flash, so every secret buffer -- the `mnemonic`
global watched here included (src/nbgl/bip39_mnemonic.c) -- lives exclusively
in RAM (globals/.bss or stack locals). The only way a secret could survive an
app close-then-reopen is if BOLOS does not actually reset that RAM when
(re)loading the app process -- a platform guarantee, not something this
app's own code controls. A `PASS` here confirms that guarantee holds for this
build; a `FAIL` would be a significant finding about the platform or the test
infrastructure itself, not a one-line application fix.

Scenario:
  1. Start a first Speculos container. On "Check BIP39", type and confirm
     all 12 words of a known-valid test mnemonic (reusing the vector and
     per-word typing helper from verify_compare_recovery_phrase_cleanup.py),
     filling the real `mnemonic` global.
  2. Read that buffer at the return of bip39_mnemonic_check() -- the point
     furthest into the flow where the buffer is still guaranteed to hold
     the full typed phrase (see "Why docker stop/rm, not a real quit tap"
     below) -- as a sanity check that the secret was genuinely present
     before the app goes away.
  3. Tear down that first container's process entirely.
  4. Start a second, completely independent Speculos container from the
     same app.elf (no shared volumes/state with the first).
  5. Before any simulated user interaction, read the same global in this
     fresh process (its R9-relative runtime address is re-resolved from
     scratch -- never assumed identical to the first session) and confirm
     none of the first session's typed words appear in it.

Why docker stop/rm, not a real quit tap: on_quit() (src/nbgl/ui.c,
os_sched_exit(-1)) is trivially reachable -- confirmed empirically, it is a
"Quit app" footer link directly on the home screen (nbgl_useCaseHomeAndSettings)
-- but reaching it from the post-confirmation state requires first navigating
back to the dashboard, and display_home_page() unconditionally calls
reset_globals() (-> bip39_mnemonic_reset()) before it ever draws that screen.
Doing that would wipe the buffer with this app's own defense-in-depth code
before the platform-level guarantee under test is ever exercised -- exactly
the thing checks 1/4/5 already verify, and it would make a PASS here
ambiguous (clean because of BOLOS, or clean because of application code that
had nothing to do with it?). Tearing the container down directly, right after
the sanity-check read, keeps the buffer populated with real content up to the
moment the first process actually disappears.

Symmetrically, the second container's read point is not "as soon as GDB can
attach" but the entry of ui_idle_init() / display_home_page() -- the first
UI code this fresh process ever runs, called once at boot. That is deliberately
*before* its own reset_globals() call (the first statement in the function
body): reading any later would let this process's own init code launder away
whatever the loader actually left behind, which is exactly the thing this
script needs to observe untouched.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_app_reopen_clears_mnemonic.py

Exit code 0 and "PASS" if none of the first session's typed words appear in
the second (fresh) process's buffer. Exit code 1 and "FAIL" otherwise -- see
this script's header comment in the repo for what to do in that case (not a
routine one-line fix).

See README.md in this directory for the Speculos/GDB gotchas this script
relies on (shared with the other five scripts).
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
from rsp_client import RSP

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ELF_PATH = os.path.join(REPO_ROOT, "build", "flex", "bin", "app.elf")
BUILDER_IMAGE = "ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest"
SPECULOS_IMAGE = "ghcr.io/ledgerhq/speculos:latest"
CONTAINER_NAME_1 = "speculos-app-reopen-test-1"
CONTAINER_NAME_2 = "speculos-app-reopen-test-2"
API_PORT = 5006
GDB_PORT = 1239
BASE_URL = f"http://localhost:{API_PORT}"

# Same 128-bit BlockchainCommons test vector already used by
# verify_compare_recovery_phrase_cleanup.py / tests/functional/test_bip39_12word.py
# -- a known-valid checksum matters here: bip39_mnemonic_check() calls
# bip39_mnemonic_reset() itself on a *failed* checksum, before this script
# would get a chance to read anything meaningful.
MNEMONIC = ("fly mule excess resource treat plunge nose soda reflect adult "
           "ramp planet")
MNEMONIC_WORDS = MNEMONIC.split()

# Link-time code base for the flex target vs. the address speculos's
# launcher actually maps it at -- see README.md gotcha #4.
CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

SIGILL = 4
SOCKET_TIMEOUT = 40

# FLEX (480x600) touch-keyboard letter positions -- ragger/firmware/touch/positions.py,
# POSITIONS["LetterOnlyKeyboard"][DeviceType.FLEX] -- same source as the other scripts.
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
    """nm -S the built ELF -- never hardcode addresses, they shift on any
    source change. `bip39_mnemonic_check` and `ui_idle_init` are both real
    (non-inlined) symbols: the former is called via the keyboard dispatcher's
    if-branch, the latter is registered as the app's UX idle callback --
    confirmed present in this build via `nm`, not assumed."""
    out = docker_run_tool("nm", "-S", "/app/app.elf")
    wanted = {"mnemonic", "bip39_mnemonic_check", "ui_idle_init"}
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
            "(did bip39_mnemonic.c/ui.c rename these?)")
    return syms


def resolve_bss_base():
    out = docker_run_tool("readelf", "-S", "/app/app.elf")
    for line in out.splitlines():
        parts = line.split()
        if ".bss" in parts:
            return int(parts[parts.index(".bss") + 2], 16)
    die("could not find .bss section in app.elf")


def code_runtime_addr(link_addr):
    """Translate a linked code address to where speculos's launcher actually
    maps it -- see README.md gotcha #4. `link_addr` may carry the Thumb bit
    (bit 0), stripped here since it's not part of the real address."""
    even = link_addr & ~1
    return CODE_RUNTIME_BASE + (even - CODE_LINK_BASE)


def prepare_patched_speculos_main(scratch_dir):
    """See README.md gotcha #1: the published debug image's `-singlestep`
    flag no longer exists in the bundled QEMU; patch it to
    `-one-insn-per-tb`."""
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


def start_container(name, patched_main_py):
    subprocess.run(["docker", "rm", "-f", name], capture_output=True)
    subprocess.run([
        "docker", "run", "-d", "--name", name,
        "-p", f"{GDB_PORT}:1234", "-p", f"{API_PORT}:5001",
        "-v", f"{os.path.dirname(ELF_PATH)}:/app",
        "-v", f"{patched_main_py}:/speculos/speculos/main.py",
        SPECULOS_IMAGE, "-m", "flex", "--display", "headless",
        "--api-port", "5001", "-d", "/app/app.elf",
    ], check=True, capture_output=True)


def stop_container(name):
    subprocess.run(["docker", "rm", "-f", name], capture_output=True)


def tap(x, y, delay=0.1):
    body = json.dumps({"action": "press-and-release", "x": x, "y": y, "delay": delay}).encode()
    req = urllib.request.Request(f"{BASE_URL}/finger", data=body,
                                  headers={"Content-Type": "application/json"}, method="POST")
    urllib.request.urlopen(req).read()
    time.sleep(0.4)


def screen_texts():
    data = json.loads(urllib.request.urlopen(f"{BASE_URL}/events?currentscreenonly=true").read())
    return [e["text"] for e in data["events"]]


class CheckReturnSampler:
    """First container: runs the app under GDB (SIGILL passthrough, see
    README.md gotcha #2), and takes a synchronous snapshot of the `mnemonic`
    buffer at the entry and return of bip39_mnemonic_check() -- same
    entry+LR-return pattern as verify_bip39_cancel_clears_buffer.py's
    MemorySampler, narrowed to a single watched function.

    The return point is the sanity-check read this script needs: by then
    compare_recovery_phrase() has already run, but bip39_mnemonic_check()
    deliberately does *not* reset the buffer on a valid-checksum path ("Don't
    clear the mnemonic just yet as we may need it to generate BIP39
    mnemonic" -- bip39_mnemonic.c), so the full typed phrase is still there.
    """

    def __init__(self, watch_addr, mnemonic_offset, mnemonic_len):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.watch_addr = watch_addr
        self.mnemonic_offset = mnemonic_offset
        self.mnemonic_len = mnemonic_len
        self.entry = None
        self.after_return = None
        self._pending_return = None  # (ret_addr, before_bytes)
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        self.rsp.insert_bp(self.watch_addr, kind=2)
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
                mnemonic_addr = regs[9] + self.mnemonic_offset

                if self._pending_return and pc == self._pending_return[0]:
                    ret_addr, before = self._pending_return
                    after = self.rsp.read_mem(mnemonic_addr, self.mnemonic_len)
                    self.after_return = after
                    self.rsp.remove_bp(ret_addr, kind=2)
                    self._pending_return = None
                    self.rsp.cont()
                    continue

                if pc == self.watch_addr and self.entry is None:
                    self.entry = self.rsp.read_mem(mnemonic_addr, self.mnemonic_len)
                    ret_addr = regs[14] & ~1
                    self.rsp.insert_bp(ret_addr, kind=2)
                    self._pending_return = (ret_addr, self.entry)
                    # step over the breakpoint we're sitting on before
                    # resuming, or we'd hit it again immediately
                    self.rsp.remove_bp(pc, kind=2)
                    self.rsp.step()
                    self.rsp.insert_bp(pc, kind=2)
                    self.rsp.cont()
                    continue

                self.rsp.cont()
            except (ConnectionError, OSError):
                return


class FreshBootSampler:
    """Second container: a single breakpoint at ui_idle_init()'s entry, the
    very first UI code a fresh process runs at boot -- deliberately *before*
    display_home_page()'s own reset_globals() call, so this captures whatever
    the loader actually left in .bss, untouched by this app's own init code.
    No taps, no other interaction: this fires purely from the normal boot
    sequence."""

    def __init__(self, watch_addr, mnemonic_offset, mnemonic_len):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.watch_addr = watch_addr
        self.mnemonic_offset = mnemonic_offset
        self.mnemonic_len = mnemonic_len
        self.r9 = None
        self.snapshot = None
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        self.rsp.insert_bp(self.watch_addr, kind=2)
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

                if pc == self.watch_addr and self.snapshot is None:
                    self.r9 = regs[9]
                    mnemonic_addr = self.r9 + self.mnemonic_offset
                    self.snapshot = self.rsp.read_mem(mnemonic_addr, self.mnemonic_len)
                    return  # got what we need, nothing left to pump

                self.rsp.cont()
            except (ConnectionError, OSError):
                return


def type_and_confirm_word(word):
    """Type a BIP39 word in full and tap the first suggestion -- see
    verify_compare_recovery_phrase_cleanup.py for why the poll is patient
    (screen redraws under GDB + SIGILL-passthrough overhead can lag many
    tens of seconds behind the actual taps)."""
    for letter in word:
        tap(*FLEX_KEYS[letter])
    texts = []
    for _ in range(200):
        texts = screen_texts()
        if word in texts:
            break
        time.sleep(0.5)
    else:
        die(f"expected '{word}' suggestion on screen after typing it, got: "
            f"{texts} -- navigation/keyboard-mapping assumption is wrong")
    tap(*SUGGESTION_1)


def navigate_and_confirm_mnemonic():
    """Coordinates are for the flex layout only (480x600); see README.md
    for how these were found and how to adapt to stax/apex."""
    time.sleep(3)  # let the app finish booting before the first tap
    tap(371, 436)  # home screen: "Select Tool"
    time.sleep(1)
    tap(358, 524)  # "BIP39 Check"
    time.sleep(1)
    tap(387, 524)  # "12 words"
    time.sleep(1)

    for word in MNEMONIC_WORDS:
        type_and_confirm_word(word)


def nz(b):
    return sum(1 for x in b if x)


def main():
    check_build()
    print("Resolving symbols from build/flex/bin/app.elf ...")
    syms = resolve_symbols()
    bss_base = resolve_bss_base()
    mnemonic_addr, mnemonic_len = syms["mnemonic"]
    mnemonic_offset = mnemonic_addr - bss_base
    check_addr = code_runtime_addr(syms["bip39_mnemonic_check"][0])
    idle_addr = code_runtime_addr(syms["ui_idle_init"][0])
    print(f"  mnemonic buffer: offset R9+0x{mnemonic_offset:x}, {mnemonic_len} bytes")
    print(f"  breakpoint (container 1): 0x{check_addr:x} (bip39_mnemonic_check)")
    print(f"  breakpoint (container 2): 0x{idle_addr:x} (ui_idle_init)")

    # --- Container 1: fill the buffer, read it, then tear the process down ---
    sampler1 = None
    with tempfile.TemporaryDirectory() as scratch:
        print("\nPatching Speculos debug image (singlestep -> one-insn-per-tb) ...")
        patched_main = prepare_patched_speculos_main(scratch)

        print(f"Starting first Speculos container ({CONTAINER_NAME_1}) ...")
        start_container(CONTAINER_NAME_1, patched_main)
        try:
            time.sleep(2)
            sampler1 = CheckReturnSampler(check_addr, mnemonic_offset, mnemonic_len)
            sampler1.start()
            print("Navigating: home -> BIP39 Check -> 12 words -> type+confirm "
                  "all 12 words of the test mnemonic ...")
            print("(this is slow -- every guest syscall round-trips through this "
                  "script while GDB is attached; a full run takes a few minutes)")
            navigate_and_confirm_mnemonic()
            for _ in range(150):
                if sampler1.after_return is not None:
                    break
                time.sleep(0.5)
        finally:
            print("Tearing down the first container (simulates the app closing) ...")
            stop_container(CONTAINER_NAME_1)

    if sampler1.entry is None:
        die("bip39_mnemonic_check was never hit -- navigation didn't reach "
            "the expected code path (did the UI flow change?)")
    if MNEMONIC.encode() not in sampler1.entry:
        die("bip39_mnemonic_check fired without the full typed phrase present "
            "in the mnemonic buffer -- test setup is broken, this isn't the "
            f"scenario under test:\n  {sampler1.entry.hex()}")
    if sampler1.after_return is None:
        die("bip39_mnemonic_check's entry breakpoint fired but its return "
            "never did -- it didn't return normally (LEDGER_ASSERT somewhere "
            "downstream?)")

    print(f"\nContainer 1 -- mnemonic buffer at bip39_mnemonic_check() return "
          f"(nonzero={nz(sampler1.after_return)}/{mnemonic_len}):")
    print(f"  {sampler1.after_return.hex()}")
    if MNEMONIC.encode() not in sampler1.after_return:
        die("the typed phrase is not present at bip39_mnemonic_check's return "
            "-- test setup is broken (unexpected early clear?), this isn't "
            "the scenario under test")
    print("\n  confirmed: the full typed phrase is present in RAM right "
          "before the first process is torn down (as expected -- this is "
          "what makes the next check meaningful)")

    # --- Container 2: brand-new process, read before any interaction ---
    sampler2 = None
    with tempfile.TemporaryDirectory() as scratch:
        patched_main = prepare_patched_speculos_main(scratch)
        print(f"\nStarting second, independent Speculos container "
              f"({CONTAINER_NAME_2}) from the same app.elf, no shared "
              "volumes/state with the first ...")
        start_container(CONTAINER_NAME_2, patched_main)
        try:
            time.sleep(2)
            sampler2 = FreshBootSampler(idle_addr, mnemonic_offset, mnemonic_len)
            sampler2.start()
            print("Waiting for the fresh process to reach ui_idle_init() "
                  "(no taps sent -- this fires from the normal boot sequence "
                  "alone) ...")
            for _ in range(150):
                if sampler2.snapshot is not None:
                    break
                time.sleep(0.5)
        finally:
            stop_container(CONTAINER_NAME_2)

    if sampler2.snapshot is None:
        die("ui_idle_init was never hit in the second container -- it never "
            "reached its normal boot sequence (Speculos/container problem, "
            "not the scenario under test)")

    print(f"\nContainer 2 -- mnemonic buffer at ui_idle_init() entry, before "
          f"any interaction (R9=0x{sampler2.r9:x}, nonzero="
          f"{nz(sampler2.snapshot)}/{mnemonic_len}):")
    print(f"  {sampler2.snapshot.hex()}")

    for word in MNEMONIC_WORDS:
        if word.encode() in sampler2.snapshot:
            print(f"\nFAIL: '{word}' from the first session's mnemonic is "
                  "still present in the second (fresh) process's mnemonic "
                  "buffer -- BOLOS did not reset this RAM across the "
                  "close/reopen. Do not treat this as a routine application "
                  "bug -- see this script's module docstring for what to do.")
            sys.exit(1)

    print("\nPASS: no trace of the first session's typed mnemonic is present "
          "in the second, freshly-booted process's buffer -- BOLOS resets "
          "this RAM on app reload, as expected for a platform guarantee "
          "this app relies on rather than implements itself.")
    sys.exit(0)


if __name__ == "__main__":
    main()
