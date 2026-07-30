#!/usr/bin/env python3
"""Speculos regression check: cancelling mid-entry on the "Check SSKR"
screen must not leave a confirmed share word's decoded byte behind in RAM.

Scenario: type one SSKR share word ("acid"), confirm it (lands in the real
`shares` static buffer, src/nbgl/sskr_shares.c, via sskr_shares_word_add()),
start typing a second word but don't confirm it, then press "back" until
the entry screen is left. Read the raw `shares.buffer` over a GDB
connection before and after, on the real running app -- not the source,
the actual bytes in RAM.

Same overall pattern as verify_bip39_cancel_clears_buffer.py (R9-relative
global, entry+LR-return breakpoints on the three word-lifecycle
functions) -- see README.md for the shared gotchas this relies on.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_sskr_share_cancel_clears_buffer.py

Exit code 0 and "PASS" if the confirmed word's decoded byte no longer
appears in shares.buffer after cancelling. Exit code 1 and "FAIL" if it
does -- a real bug (a secret share byte staying resident in RAM after the
user backed out), not a test infrastructure problem.
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

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ELF_PATH = os.path.join(REPO_ROOT, "build", "flex", "bin", "app.elf")
BUILDER_IMAGE = "ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest"
SPECULOS_IMAGE = "ghcr.io/ledgerhq/speculos:latest"
CONTAINER_NAME = "speculos-sskr-cancel-test"
API_PORT = 5003
GDB_PORT = 1236
BASE_URL = f"http://localhost:{API_PORT}"

# SSKR_WORDLIST (src/common/sskr/seed_rom_variables.c) is a flat list of
# 4-char words; bolos_ux_sskr_byteword_to_hex() (src/common/sskr/seed_sskr.c)
# does a linear scan and returns `index_in_list`, i.e. wordlist index ==
# decoded byte value. Index 0 is "able" -> byte 0x00 -- a bad choice here,
# since the buffer position would read 0x00 both *before* typing anything
# (untouched .bss) and *after* a correct cancel clears it, proving nothing.
# Index 1, "acid" -> byte 0x01, gives an unambiguous non-zero value to
# check for absence afterward.
WORD = "acid"
WORD_VALUE = 0x01
PARTIAL_SECOND_WORD = "al"  # never confirmed; content doesn't matter

# Link-time code base for the flex target vs. the address speculos's
# launcher actually maps it at -- see README.md gotcha #4.
CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

SIGILL = 4
SOCKET_TIMEOUT = 40

# FLEX (480x600) touch-keyboard letter positions, from Ledger's own
# `ragger` package (ragger/firmware/touch/positions.py,
# POSITIONS["LetterOnlyKeyboard"][DeviceType.FLEX]) -- same table already
# used by verify_compare_recovery_phrase_cleanup.py.
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

# Tool-select screen (POSITIONS["GenericLayout"][DeviceType.FLEX], same
# table this repo's own tests/functional/genericlayout.py uses):
# choice(1)=(240,540)="BIP39 Check", choice(2)=(240,430)="SSKR Check",
# choice(3)=(240,320)="BIP85 Generate". Unlike BIP39, selecting SSKR Check
# goes straight to the entry keyboard -- no length-selection screen
# (see select_tool_callback() in src/nbgl/ui.c).
SSKR_CHECK_BUTTON = (240, 430)
BACK_BUTTON = (48, 48)


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
    wanted = {"sskr_shares_reset", "sskr_shares_word_remove",
              "sskr_shares_word_add", "shares"}
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
    """Translate a linked code address to where speculos's launcher
    actually maps it (Thumb bit stripped, not part of the real address)."""
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


def start_container(patched_main_py):
    subprocess.run(["docker", "rm", "-f", CONTAINER_NAME], capture_output=True)
    subprocess.run([
        "docker", "run", "-d", "--name", CONTAINER_NAME,
        "-p", f"{GDB_PORT}:1234", "-p", f"{API_PORT}:5001",
        "-v", f"{os.path.dirname(ELF_PATH)}:/app",
        "-v", f"{patched_main_py}:/speculos/speculos/main.py",
        SPECULOS_IMAGE, "-m", "flex", "--display", "headless",
        "--api-port", "5001", "-d", "/app/app.elf",
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
    """Runs the app to completion under GDB by passing every SIGILL trap
    straight through (see README.md gotcha #2), and takes a synchronous
    memory snapshot of `shares.buffer` at the entry and return of each
    watched function, keyed by function name. Same pattern as
    verify_bip39_cancel_clears_buffer.py's MemorySampler, applied to the
    `shares` global instead of `mnemonic`.
    """

    def __init__(self, watch_addrs, shares_offset, read_len):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.watch = watch_addrs  # {runtime_addr: name}
        self.shares_offset = shares_offset
        self.read_len = read_len
        self.snapshots = []  # (name, before_bytes, after_bytes), in hit order
        self._pending_return = None  # (ret_addr, name, before_bytes)
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
    """Type letters, no confirm -- used for the never-confirmed 2nd word."""
    for letter in word:
        tap(*FLEX_KEYS[letter])


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
    tap(*SUGGESTION_1)


def navigate_and_cancel():
    """Coordinates are for the flex layout only (480x600); see README.md."""
    time.sleep(3)  # let the app finish booting before the first tap
    tap(371, 436)  # home screen: "Select Tool"
    time.sleep(1)
    tap(*SSKR_CHECK_BUTTON)  # "SSKR Check" -- goes straight to the keyboard
    time.sleep(1)

    # word 1: type "acid" in full, confirm it -> lands in shares.buffer via
    # sskr_shares_word_add()
    type_and_confirm_word(WORD)

    # word 2: type a couple letters but never confirm -- the "cancel while
    # typing" moment this scenario is about
    type_word(PARTIAL_SECOND_WORD)

    # back out: 1st tap removes the confirmed word (sskr_shares_word_remove
    # -> sskr_shares_shrink, the real per-word clearing path), 2nd tap
    # exits the screen entirely (sskr_shares_reset, defense in depth)
    tap(*BACK_BUTTON)
    time.sleep(1)
    tap(*BACK_BUTTON)
    time.sleep(1)


def main():
    check_build()
    print("Resolving symbols from build/flex/bin/app.elf ...")
    syms = resolve_symbols()
    bss_base = resolve_bss_base()
    shares_addr, shares_size = syms["shares"]
    shares_offset = shares_addr - bss_base
    read_len = 8  # only need to see shares.buffer[0], a little headroom
    watch = {
        code_runtime_addr(syms["sskr_shares_word_add"][0]): "sskr_shares_word_add",
        code_runtime_addr(syms["sskr_shares_word_remove"][0]): "sskr_shares_word_remove",
        code_runtime_addr(syms["sskr_shares_reset"][0]): "sskr_shares_reset",
    }
    print(f"  shares buffer: offset R9+0x{shares_offset:x} (struct size {shares_size} bytes)")
    for addr, name in watch.items():
        print(f"  breakpoint: 0x{addr:x} ({name})")
    print(f"  test word: '{WORD}' -> expected decoded byte 0x{WORD_VALUE:02x}")

    sampler = None
    with tempfile.TemporaryDirectory() as scratch:
        print("Patching Speculos debug image (singlestep -> one-insn-per-tb) ...")
        patched_main = prepare_patched_speculos_main(scratch)

        print("Starting Speculos ...")
        start_container(patched_main)
        try:
            if not wait_for_gdb(GDB_PORT):
                die(f"Speculos GDB stub on port {GDB_PORT} did not "
                    "become ready in time")
            sampler = MemorySampler(watch, shares_offset, read_len)
            sampler.start()
            print("Navigating: home -> SSKR Check -> type+confirm 'acid' -> "
                  "type partial 'al' -> cancel (back x2) ...")
            print("(this is slow -- every guest syscall round-trips through this "
                  "script while GDB is attached)")
            navigate_and_cancel()
            time.sleep(2)
        finally:
            stop_container()

    if sampler is None or not sampler.snapshots:
        die("no breakpoint ever fired -- navigation didn't reach the expected "
            "code paths (did the UI flow change?)")

    print(f"\nCaptured {len(sampler.snapshots)} snapshots:")
    add_hit = None
    remove_hit = None
    for name, before, after in sampler.snapshots:
        print(f"  {name}: before={before.hex()} after={after.hex()}")
        if name == "sskr_shares_word_add" and add_hit is None:
            add_hit = (before, after)
        if name == "sskr_shares_word_remove" and remove_hit is None:
            remove_hit = (before, after)

    if add_hit is None:
        die("sskr_shares_word_add was never hit -- 'acid' was never confirmed")
    if add_hit[1][0] != WORD_VALUE:
        die(f"confirming '{WORD}' did not write byte 0x{WORD_VALUE:02x} into "
            f"shares.buffer[0] -- got 0x{add_hit[1][0]:02x} -- test setup is "
            "broken, this isn't the bug under test")
    print(f"\n  confirmed: '{WORD}' (byte 0x{WORD_VALUE:02x}) is present in "
          "shares.buffer right after being typed (as expected -- this is "
          "what makes the next check meaningful)")

    if remove_hit is None:
        die("sskr_shares_word_remove was never hit -- cancel didn't reach "
            "the buffer-clearing code path at all")
    before, after = remove_hit
    if before[0] != WORD_VALUE:
        die(f"sskr_shares_word_remove fired without byte 0x{WORD_VALUE:02x} "
            f"present before it (got 0x{before[0]:02x}) -- navigation/timing "
            "assumption is wrong, re-check the scenario")
    if after[0] == WORD_VALUE:
        print(f"\nFAIL: byte 0x{WORD_VALUE:02x} ('{WORD}') is still present "
              "in shares.buffer[0] after cancelling -- typed content was "
              "not cleared:")
        print(f"  {after.hex()}")
        sys.exit(1)

    print(f"\nPASS: byte 0x{WORD_VALUE:02x} ('{WORD}') is no longer present "
          "in shares.buffer[0] after cancelling (sskr_shares_word_remove -> "
          "sskr_shares_shrink zeroed it)")
    sys.exit(0)


if __name__ == "__main__":
    main()
