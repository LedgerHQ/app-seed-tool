#!/usr/bin/env python3
"""Speculos regression check: does a generated BIP-85 BIP39 output actually
get cleared from RAM once the user taps "Done" on the review screen and
returns to the dashboard?

`bip85_app_bip39_gen()` (src/nbgl/bip85_app.c) is the most sensitive of the
three BIP-85 apps: it produces a real recovery-phrase-shaped mnemonic, not
just a password. It writes into two static globals in one call:
  - `app_data.buffer` (src/nbgl/bip85_app.c): the raw derived entropy.
  - `mnemonic.buffer` (src/nbgl/bip39_mnemonic.c): the same static already
    watched by verify_bip39_cancel_clears_buffer.py, here populated via
    bip39_mnemonic_encode(app_data.buffer, app_data.length).

Both are `static` globals -- same R9-relative addressing as every other
buffer checked in this directory, nothing new to solve.

The erase path is `reset_globals()` (src/nbgl/ui.c, static, called from
review_done() after the "Done" tap on the generic review screen -- the
same nbgl_useCaseGenericReview widget already exercised, and proven not to
be bypassable on this exit path, by
verify_sskr_generated_shares_dashboard_return.py). reset_globals() is
small enough to plausibly get inlined into its callers, so this script
watches the two real, non-static functions it calls directly instead:
bip39_mnemonic_reset() (already known) and bip85_app_reset() (new). Since
bip85_app_reset() runs last in that sequence, its return is a clean point
to snapshot both buffers at once.

bip85_app_reset() is a full `memzero(&app_data, sizeof(app_data))` (unlike
the SSKR/BIP39 *cancel* scripts' partial-content checks, which deliberately
tolerate a legitimate non-zero bookkeeping field) -- so the pass condition
here is "reads as all-zero", for both buffers, in full.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_bip85_bip39_output_cleanup.py

Exit code 0 and "PASS" if both buffers read all-zero after "Done". Exit
code 1 and "FAIL" if either still contains generated content -- a real
bug, not a test infrastructure problem.
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
CONTAINER_NAME = "speculos-bip85-bip39-output-cleanup-test"
API_PORT = 5005
GDB_PORT = 1238
BASE_URL = f"http://localhost:{API_PORT}"

CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

# mnemonic.buffer's actual length within the `mnemonic` struct (the rest is
# bookkeeping -- length/current_word_index/word_lengths[]/final_size).
# BIP39_MNEMONIC_MAX_LENGTH = BIP39_MNEMONIC_SIZE_24 * (BIP39_MAX_WORD_LENGTH + 1)
# = 24 * (8 + 1) = 216 (src/nbgl/bip39_mnemonic.h, src/constants.h). Needed
# because bip39_mnemonic_reset() deliberately leaves current_word_index at
# its (size_t)-1 "no word" sentinel after memzero -- legitimate bookkeeping,
# not a leak, already documented for check 1 in README.md. Checking the
# *whole* mnemonic struct for all-zero would false-FAIL on that sentinel;
# only the buffer portion (the actual phrase text) needs to be all-zero.
MNEMONIC_BUFFER_LENGTH = 216

SIGILL = 4
SOCKET_TIMEOUT = 40




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
    source change to these files). Returns {name: (address, size)}."""
    out = docker_run_tool("nm", "-S", "/app/app.elf")
    wanted = {"app_data", "mnemonic", "bip85_app_bip39_gen",
              "bip85_app_reset", "bip39_mnemonic_reset"}
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
            "(did bip85_app.c/bip39_mnemonic.c change function/variable "
            "names?)")
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


def wait_for_text(target, timeout_s=60, poll=0.5):
    deadline = time.time() + timeout_s
    texts = []
    while time.time() < deadline:
        texts = screen_texts()
        if any(target in t for t in texts):
            return texts
        time.sleep(poll)
    die(f"expected screen text containing {target!r} within {timeout_s}s, "
        f"last seen: {texts}")


class MemorySampler:
    """Same entry+LR-return breakpoint pattern as the other scripts in this
    directory, but samples *two* R9-relative static buffers (app_data and
    mnemonic) at every hit instead of one."""

    def __init__(self, watch_addrs, app_data_offset, app_data_len,
                 mnemonic_offset, mnemonic_len):
        self.rsp = RSP(port=GDB_PORT, timeout=SOCKET_TIMEOUT)
        self.watch = watch_addrs  # {runtime_addr: name}
        self.app_data_offset = app_data_offset
        self.app_data_len = app_data_len
        self.mnemonic_offset = mnemonic_offset
        self.mnemonic_len = mnemonic_len
        self.snapshots = []  # (name, before_pair, after_pair), before/after = (app_data_bytes, mnemonic_bytes)
        self._pending_return = None
        self._thread = threading.Thread(target=self._pump, daemon=True)

    def start(self):
        for addr in self.watch:
            self.rsp.insert_bp(addr, kind=2)
        self.rsp.cont()
        self._thread.start()

    def _sample(self, r9):
        app_data_bytes = self.rsp.read_mem(r9 + self.app_data_offset, self.app_data_len)
        mnemonic_bytes = self.rsp.read_mem(r9 + self.mnemonic_offset, self.mnemonic_len)
        return app_data_bytes, mnemonic_bytes

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
                r9 = regs[9]

                if self._pending_return and pc == self._pending_return[0]:
                    ret_addr, name, before = self._pending_return
                    after = self._sample(r9)
                    self.snapshots.append((name, before, after))
                    self.rsp.remove_bp(ret_addr, kind=2)
                    self._pending_return = None
                    self.rsp.cont()
                    continue

                if pc in self.watch:
                    name = self.watch[pc]
                    ret_addr = regs[14] & ~1
                    before = self._sample(r9)
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


def navigate_generate_view_and_return():
    """Coordinates are for the flex layout only (480x600); see README.md."""
    time.sleep(3)  # let the app finish booting before the first tap
    tap(*screens.HOME_ACTION)  # "Select an action"
    time.sleep(1)

    wait_for_text("do?")
    tap(*screens.list_row(screens.MENU_DERIVE))  # "Derive with BIP85"
    time.sleep(1)

    wait_for_text("BIP85")
    tap(*screens.CONTINUE_FOOTER)  # "How BIP85 works"
    time.sleep(1)

    wait_for_text("secret")
    tap(*screens.list_row(screens.BIP85_APP_BIP39))  # "BIP39"
    time.sleep(1)

    wait_for_text("Length of BIP39")
    tap(*screens.list_row(screens.WORDS_12))  # "12 words"
    time.sleep(1)

    wait_for_text("index", timeout_s=60)
    tap(*screens.CONTINUE_FOOTER)  # "What is an index?"
    time.sleep(1)

    wait_for_text("index", timeout_s=60)
    tap(*screens.KEYPAD["1"])  # index 1
    tap(*screens.KEYPAD["enter"])
    time.sleep(1)

    # The review of the three values and the path they combine into, then the
    # warning and the long press that reveals the secret.
    wait_for_text("Path", timeout_s=60)
    tap(*screens.REVIEW_NEXT)
    wait_for_text("Anyone who sees", timeout_s=60)

    # bip85_generate_and_display() -> display_generic_review(); "Close" is the
    # reject control of that review, and the only way off the screen.
    long_press_until(*screens.REVIEW_LONG_PRESS, "Close")
    tap(*screens.SECRET_CLOSE)  # -> review_done() -> reset_globals()
    time.sleep(2)


def main():
    check_build()
    print("Resolving symbols from build/flex/bin/app.elf ...")
    syms = resolve_symbols()
    bss_base = resolve_bss_base()
    app_data_addr, app_data_len = syms["app_data"]
    mnemonic_addr, mnemonic_len = syms["mnemonic"]
    app_data_offset = app_data_addr - bss_base
    mnemonic_offset = mnemonic_addr - bss_base
    watch = {
        code_runtime_addr(syms["bip85_app_bip39_gen"][0]): "bip85_app_bip39_gen",
        code_runtime_addr(syms["bip39_mnemonic_reset"][0]): "bip39_mnemonic_reset",
        code_runtime_addr(syms["bip85_app_reset"][0]): "bip85_app_reset",
    }
    print(f"  app_data buffer: offset R9+0x{app_data_offset:x} (struct size {app_data_len} bytes)")
    print(f"  mnemonic buffer: offset R9+0x{mnemonic_offset:x} (struct size {mnemonic_len} bytes)")
    for addr, name in watch.items():
        print(f"  breakpoint: 0x{addr:x} ({name})")

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
            sampler = MemorySampler(watch, app_data_offset, app_data_len,
                                    mnemonic_offset, mnemonic_len)
            sampler.start()
            print("Navigating: home -> Derive with BIP85 -> explanation -> "
                  "BIP39 -> 12 words -> index 1 -> review -> long press -> "
                  "Close -> dashboard ...")
            print("(no mnemonic typing needed on this path -- should be a few "
                  "minutes, not 15-30+)")
            navigate_generate_view_and_return()
            for _ in range(60):
                names = [n for n, _, _ in sampler.snapshots]
                if "bip85_app_reset" in names:
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
    mnemonic_reset_hit = None
    for name, before, after in sampler.snapshots:
        app_data_after, mnemonic_after = after
        print(f"  {name}: app_data.after={app_data_after.hex()}")
        print(f"  {' ' * len(name)}  mnemonic.after={mnemonic_after.hex()}")
        if name == "bip85_app_bip39_gen" and gen_hit is None:
            gen_hit = (before, after)
        if name == "bip85_app_reset" and reset_hit is None:
            reset_hit = (before, after)
        if name == "bip39_mnemonic_reset" and mnemonic_reset_hit is None:
            mnemonic_reset_hit = (before, after)

    if gen_hit is None:
        die("bip85_app_bip39_gen was never hit -- generation was never "
            "reached")
    gen_app_data_after, gen_mnemonic_after = gen_hit[1]
    nz_app_data = sum(1 for b in gen_app_data_after if b)
    nz_mnemonic = sum(1 for b in gen_mnemonic_after if b)
    print(f"\n  bip85_app_bip39_gen returned with {nz_app_data}/{app_data_len} "
          f"nonzero bytes in app_data, {nz_mnemonic}/{mnemonic_len} nonzero "
          "bytes in mnemonic")
    if nz_app_data == 0 or nz_mnemonic == 0:
        die("app_data and/or mnemonic reads all-zero right after "
            "bip85_app_bip39_gen() returns -- generation didn't actually "
            "produce data, this is a test-setup problem, not the bug under "
            "test")
    print("  confirmed: real generated BIP-85 output is present in RAM "
          "right after generation (as expected -- this is what makes the "
          "next check meaningful)")

    if reset_hit is None:
        print("\nFAIL: bip85_app_reset() was never hit, despite the "
              "navigation completing its \"Done\" tap on the review screen. "
              "That means the generated output captured above is exactly "
              "what's left resident in RAM on this return-to-dashboard "
              "path -- a real gap, not a test infrastructure problem.")
        print(f"  app_data right after generation:  {gen_app_data_after.hex()}")
        print(f"  mnemonic right after generation:  {gen_mnemonic_after.hex()}")
        sys.exit(1)

    reset_app_data_after, reset_mnemonic_after = reset_hit[1]
    all_zero_app_data = b"\x00" * app_data_len
    # Only the buffer (phrase text) portion of `mnemonic` must be all-zero
    # -- see MNEMONIC_BUFFER_LENGTH comment above for why the rest of the
    # struct is excluded.
    mnemonic_buffer_after = reset_mnemonic_after[:MNEMONIC_BUFFER_LENGTH]
    mnemonic_bookkeeping_after = reset_mnemonic_after[MNEMONIC_BUFFER_LENGTH:]
    all_zero_mnemonic_buffer = b"\x00" * MNEMONIC_BUFFER_LENGTH
    failed = False
    if reset_app_data_after != all_zero_app_data:
        nz = sum(1 for b in reset_app_data_after if b)
        print(f"\nFAIL: bip85_app_reset() fired, but app_data still has "
              f"{nz}/{app_data_len} nonzero bytes after it returns:")
        print(f"  {reset_app_data_after.hex()}")
        failed = True
    if mnemonic_buffer_after != all_zero_mnemonic_buffer:
        nz = sum(1 for b in mnemonic_buffer_after if b)
        print(f"\nFAIL: bip85_app_reset() fired, but mnemonic.buffer (the "
              f"phrase text itself) still has {nz}/{MNEMONIC_BUFFER_LENGTH} "
              "nonzero bytes after it returns:")
        print(f"  {mnemonic_buffer_after.hex()}")
        failed = True
    if failed:
        sys.exit(1)
    print(f"\n  mnemonic.buffer (the phrase text, {MNEMONIC_BUFFER_LENGTH} "
          "bytes) reads all-zero; bookkeeping tail "
          f"(current_word_index/word_lengths[]/final_size) = "
          f"{mnemonic_bookkeeping_after.hex()} -- current_word_index's "
          "0xffffffff sentinel there, if present, is expected and not a leak")

    if mnemonic_reset_hit is not None:
        cross_check_after = mnemonic_reset_hit[1][1]
        print(f"\n  cross-check: bip39_mnemonic_reset() also fired "
              f"independently, mnemonic.after={cross_check_after.hex()}")

    print("\nPASS: bip85_app_reset() fired after \"Done\" and both app_data "
          "and mnemonic read all-zero afterward -- the generated BIP-85 "
          "BIP39 output does not survive the return to the dashboard")
    sys.exit(0)


if __name__ == "__main__":
    main()
