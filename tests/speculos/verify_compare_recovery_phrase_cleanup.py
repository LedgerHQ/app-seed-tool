#!/usr/bin/env python3
"""Speculos regression check: compare_recovery_phrase() must not leave the
derived root key sitting in RAM after it reaches its cleanup label.

Scenario: on the "Check BIP39" screen, type and confirm all 12 words of a
known test mnemonic. Confirming the 12th word triggers
bip39_mnemonic_check() (src/nbgl/bip39_mnemonic.c), which calls
compare_recovery_phrase() (src/common/common_seed.c) -- the function fixed
earlier in this project's history by collapsing three early returns into a
single `goto cleanup;`, verified only by code review until this script.

compare_recovery_phrase() has two 64-byte stack locals:
  - `buffer`: filled with the BIP39 seed derived from the typed mnemonic,
    then overwritten in place with HMAC-SHA512("Bitcoin seed", seed) -- the
    root key derived from the *input*.
  - `buffer_device`: the device's own root key, from
    os_derive_bip32_no_throw().
Both are supposed to be wiped by two back-to-back explicit_bzero() calls at
the `cleanup:` label before the function returns.

Speculos is booted with `-s <mnemonic>` (the same 128-bit BlockchainCommons
test vector already used by tests/functional/test_bip39_12word.py), so the
mnemonic typed on screen also matches the *device's* real seed. That makes
`buffer` and `buffer_device` hold the exact same, independently-computable
64-byte root key right before cleanup -- the strongest possible known
content to check for absence afterward.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_compare_recovery_phrase_cleanup.py

Exit code 0 and "PASS" if neither buffer contains the root key (and both
read all-zero) after compare_recovery_phrase() reaches its cleanup label.
Exit code 1 and "FAIL" otherwise -- a real bug (a derived root key staying
resident in RAM after the comparison is done), not a test infra problem.

See README.md in this directory for the Speculos/GDB gotchas this script
relies on (shared with verify_bip39_cancel_clears_buffer.py), and the new
entry on SP-relative stack-local addressing added for this script.
"""
import hashlib
import hmac
import json
import os
import re
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
CONTAINER_NAME = "speculos-compare-recovery-phrase-cleanup-test"
API_PORT = 5002
GDB_PORT = 1235
BASE_URL = f"http://localhost:{API_PORT}"

# Same 128-bit BlockchainCommons test vector already used by
# tests/functional/test_bip39_12word.py (configuration.OPTIONAL.CUSTOM_SEED,
# sourced from sskr-test-vector.md). Booting Speculos with this as the
# device seed (-s) means the mnemonic typed on screen also matches the
# device's own seed -- compare_recovery_phrase() takes its match path and
# both stack buffers hold the same, independently-computable root key.
MNEMONIC = ("fly mule excess resource treat plunge nose soda reflect adult "
           "ramp planet")
MNEMONIC_WORDS = MNEMONIC.split()

# Link-time code base for the flex target vs. the address speculos's
# launcher actually maps it at -- see README.md gotcha #4.
CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

SIGILL = 4
SOCKET_TIMEOUT = 40

# FLEX (480x600) touch-keyboard letter positions, taken from Ledger's own
# `ragger` package (ragger/firmware/touch/positions.py,
# POSITIONS["LetterOnlyKeyboard"][DeviceType.FLEX]) -- authoritative, not
# guessed. Only the 26 letters are needed here (no "back"/space row).
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
# ragger POSITIONS["Suggestions"][DeviceType.FLEX]: only slots 1 and 2 are
# directly tappable without swiping. A fully-typed word should always rank
# as the top (1) suggestion.
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


def resolve_compare_recovery_phrase():
    """nm -S the built ELF for compare_recovery_phrase's link address and
    size -- never hardcoded, they shift on any source change to this
    function."""
    out = docker_run_tool("nm", "-S", "/app/app.elf")
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 4 and parts[3] == "compare_recovery_phrase":
            return int(parts[0], 16) & ~1, int(parts[1], 16)
    die("could not resolve compare_recovery_phrase in app.elf "
        "(did common_seed.c change the function name?)")


def resolve_cleanup_breakpoints(link_addr, size):
    """Disassemble compare_recovery_phrase (arm-none-eabi-objdump -- the
    generic `objdump` in this image can't disassemble ARM, see README.md)
    and locate the two explicit_bzero() calls that implement the
    `cleanup:` label. Returns (before_link_addr, after_link_addr,
    buffer_device_offset, buffer_offset):

    - before_link_addr: address of the *first* `bl ... <explicit_bzero>`
      instruction itself -- fires before either call has run, so both
      stack buffers still hold whatever secret was actually computed.
    - after_link_addr: address of the disassembly line immediately
      following the *second* `bl ... <explicit_bzero>` -- both calls have
      completed by the time this fires.
    - the two offsets are read back from the `add rX, sp, #N` immediately
      preceding each bl, as a sanity check that the two calls really do
      target the two known 64-byte stack locals (buffer_device then
      buffer, per the source's cleanup: order) rather than something else.

    Nothing here is hardcoded as a raw address -- only the function's own
    resolved link_addr/size (from resolve_compare_recovery_phrase) are used
    to bound the disassembly window.
    """
    out = docker_run_tool(
        "arm-none-eabi-objdump", "-d",
        f"--start-address=0x{link_addr:x}",
        f"--stop-address=0x{link_addr + size:x}",
        "/app/app.elf",
    )
    lines = []  # (addr, text)
    line_re = re.compile(r"^\s*([0-9a-f]+):\s*(.*)$")
    for raw in out.splitlines():
        m = line_re.match(raw)
        if m:
            lines.append((int(m.group(1), 16), m.group(2)))

    bl_bzero_re = re.compile(r"bl\s+\S+\s+<explicit_bzero>")
    add_sp_re = re.compile(r"add(?:\.w)?\s+r\d+,\s*sp,\s*#(\d+)")

    bl_indices = [i for i, (_, text) in enumerate(lines)
                  if bl_bzero_re.search(text)]
    if len(bl_indices) != 2:
        die(f"expected exactly 2 explicit_bzero() calls in "
            f"compare_recovery_phrase, found {len(bl_indices)} -- the "
            "function's cleanup path changed, this script needs "
            "re-checking by hand")

    offsets = []
    for idx in bl_indices:
        offset = None
        for j in range(idx - 1, max(idx - 4, -1), -1):
            m = add_sp_re.search(lines[j][1])
            if m:
                offset = int(m.group(1))
                break
        if offset is None:
            die("could not find the 'add rX, sp, #N' feeding one of the "
                "explicit_bzero() calls -- compiler codegen changed, this "
                "script needs re-checking by hand")
        offsets.append(offset)

    before_addr = lines[bl_indices[0]][0]
    after_idx = bl_indices[1] + 1
    if after_idx >= len(lines):
        die("no instruction found after the second explicit_bzero() call "
            "-- disassembly window too narrow")
    after_addr = lines[after_idx][0]

    buffer_device_offset, buffer_offset = offsets
    print(f"  cleanup: found at 0x{before_addr:x} (link), first "
          f"explicit_bzero(sp+{buffer_device_offset}, 64) [buffer_device], "
          f"second explicit_bzero(sp+{buffer_offset}, 64) [buffer]")
    print(f"  post-cleanup instruction at 0x{after_addr:x} (link)")
    return before_addr, after_addr, buffer_device_offset, buffer_offset


def code_runtime_addr(link_addr):
    """Translate a linked code address to where speculos's launcher
    actually maps it -- see README.md gotcha #4."""
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
    """Runs the app to completion under GDB (SIGILL-passthrough, see
    README.md gotcha #2), and takes a synchronous memory snapshot of both
    `buffer` and `buffer_device` at the two resolved cleanup breakpoints.

    Unlike verify_bip39_cancel_clears_buffer.py's MemorySampler (which
    watches R9-relative globals and needs an LR-based return breakpoint to
    catch a function's exit), these are SP-relative stack locals inside a
    single function body: SP is stable for the whole body (set once by the
    prologue's `sub sp, #488`, restored only by the epilogue), so it can be
    read directly off `regs[13]` at either breakpoint -- no separate
    "capture the frame base" step, no return-address bookkeeping.
    """

    def __init__(self, before_addr, after_addr, buffer_device_offset, buffer_offset):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.before_addr = before_addr
        self.after_addr = after_addr
        self.buffer_device_offset = buffer_device_offset
        self.buffer_offset = buffer_offset
        self.snapshots = {}  # "before"/"after" -> (buffer, buffer_device)
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        self.rsp.insert_bp(self.before_addr, kind=2)
        self.rsp.insert_bp(self.after_addr, kind=2)
        self.rsp.cont()
        self._thread.start()

    def _sample(self, sp):
        buffer = self.rsp.read_mem(sp + self.buffer_offset, 64)
        buffer_device = self.rsp.read_mem(sp + self.buffer_device_offset, 64)
        return buffer, buffer_device

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
                sp = regs[13]

                if pc == self.before_addr and "before" not in self.snapshots:
                    self.snapshots["before"] = self._sample(sp)
                    self.rsp.remove_bp(pc, kind=2)
                    self.rsp.step()
                    self.rsp.cont()
                    continue

                if pc == self.after_addr and "after" not in self.snapshots:
                    self.snapshots["after"] = self._sample(sp)
                    self.rsp.remove_bp(pc, kind=2)
                    self.rsp.step()
                    self.rsp.cont()
                    continue

                # unrelated stop (shouldn't normally happen); don't get stuck
                self.rsp.cont()
            except (ConnectionError, OSError):
                # main thread already got both snapshots and tore the
                # container down while this iteration's post-capture
                # cleanup (remove_bp/step/cont) was still in flight --
                # harmless, the snapshot itself was already recorded
                # above before this could raise.
                return


def type_and_confirm_word(word):
    """Type a BIP39 word in full (an exact match should always rank as the
    top suggestion, unlike a short prefix) and tap the first suggestion.

    Screen redraws under Speculos's -one-insn-per-tb debug mode + the
    SIGILL-passthrough overhead (README.md gotcha #2) can lag many tens of
    seconds behind the actual taps, confirmed empirically: with a GDB
    client attached and pumping, individual keystrokes on this screen were
    observed taking 15-30s to become visible via the /events API, though
    they are still processed in order (nothing is dropped -- the earlier,
    already-working cancel-and-clear script types its one demo word with
    the same blind, unthrottled taps and only checks the end result once).
    So: keep tapping letters at the normal pace, but poll patiently
    (up to ~100s) for the *complete* word before giving up -- a short
    poll ceiling here reads as a stuck/dropped key when it's actually just
    slow to render."""
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


def navigate_and_check():
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


def main():
    check_build()
    print("Resolving compare_recovery_phrase from build/flex/bin/app.elf ...")
    link_addr, size = resolve_compare_recovery_phrase()
    print(f"  compare_recovery_phrase: link addr 0x{link_addr:x}, size 0x{size:x}")
    before_link, after_link, buffer_device_offset, buffer_offset = \
        resolve_cleanup_breakpoints(link_addr, size)
    before_addr = code_runtime_addr(before_link)
    after_addr = code_runtime_addr(after_link)
    print(f"  breakpoint 'before cleanup': 0x{before_addr:x}")
    print(f"  breakpoint 'after cleanup': 0x{after_addr:x}")

    seed = hashlib.pbkdf2_hmac("sha512", MNEMONIC.encode(), b"mnemonic", 2048, dklen=64)
    root_key = hmac.new(b"Bitcoin seed", seed, hashlib.sha512).digest()
    print(f"  independently-computed root key: {root_key.hex()}")

    sampler = None
    with tempfile.TemporaryDirectory() as scratch:
        print("Patching Speculos debug image (singlestep -> one-insn-per-tb) ...")
        patched_main = prepare_patched_speculos_main(scratch)

        print(f"Starting Speculos (device seed: \"{MNEMONIC}\") ...")
        start_container(patched_main)
        try:
            time.sleep(2)
            sampler = MemorySampler(before_addr, after_addr,
                                    buffer_device_offset, buffer_offset)
            sampler.start()
            print("Navigating: home -> BIP39 Check -> 12 words -> type+confirm "
                  "all 12 words of the test mnemonic ...")
            print("(this is slow -- every guest syscall round-trips through this "
                  "script while GDB is attached; a full run takes a few minutes)")
            navigate_and_check()
            # give the two breakpoints time to fire once the 12th word
            # triggers bip39_mnemonic_check() -> compare_recovery_phrase();
            # rendering/API lag observed elsewhere in this run can be tens
            # of seconds, so this is deliberately generous
            for _ in range(150):
                if "before" in sampler.snapshots and "after" in sampler.snapshots:
                    break
                time.sleep(0.5)
        finally:
            stop_container()

    if sampler is None or "before" not in sampler.snapshots:
        die("the 'before cleanup' breakpoint never fired -- "
            "compare_recovery_phrase() was never reached (did the UI flow "
            "change, or did the typed mnemonic fail its checksum?)")
    if "after" not in sampler.snapshots:
        die("the 'before cleanup' breakpoint fired but 'after cleanup' "
            "never did -- compare_recovery_phrase() didn't return normally "
            "(LEDGER_ASSERT on the HMAC init/final path?)")

    before_buffer, before_buffer_device = sampler.snapshots["before"]
    after_buffer, after_buffer_device = sampler.snapshots["after"]

    def nz(b):
        return sum(1 for x in b if x)

    print("\nCaptured snapshots:")
    print(f"  before cleanup: buffer nonzero={nz(before_buffer)}/64 "
          f"buffer_device nonzero={nz(before_buffer_device)}/64")
    print(f"  after cleanup:  buffer nonzero={nz(after_buffer)}/64 "
          f"buffer_device nonzero={nz(after_buffer_device)}/64")

    if before_buffer != root_key:
        die("'buffer' at the cleanup label does not match the "
            "independently-computed root key -- test setup is broken (wrong "
            "word typed/confirmed, or the device seed didn't take), this "
            f"isn't the bug under test:\n  expected: {root_key.hex()}\n"
            f"  got:      {before_buffer.hex()}")
    if before_buffer_device != root_key:
        die("'buffer_device' at the cleanup label does not match the "
            "independently-computed root key -- test setup is broken (the "
            "-s device seed didn't take), this isn't the bug under "
            f"test:\n  expected: {root_key.hex()}\n"
            f"  got:      {before_buffer_device.hex()}")
    print("\n  confirmed: both 'buffer' and 'buffer_device' hold the real, "
          "independently-computed root key right before cleanup (as "
          "expected -- this is what makes the next check meaningful)")

    all_zero = b"\x00" * 64
    failed = False
    if after_buffer != all_zero:
        print("\nFAIL: 'buffer' still contains data after cleanup -- "
              "expected all-zero:")
        print(f"  {after_buffer.hex()}")
        if root_key in after_buffer:
            print("  (contains the independently-computed root key)")
        failed = True
    if after_buffer_device != all_zero:
        print("\nFAIL: 'buffer_device' still contains data after cleanup -- "
              "expected all-zero:")
        print(f"  {after_buffer_device.hex()}")
        if root_key in after_buffer_device:
            print("  (contains the independently-computed root key)")
        failed = True

    if failed:
        sys.exit(1)

    print("\nPASS: both 'buffer' and 'buffer_device' read as all-zero after "
          "compare_recovery_phrase() reaches its cleanup label (the "
          "goto-cleanup fix does clear both secrets on this path)")
    sys.exit(0)


if __name__ == "__main__":
    main()
