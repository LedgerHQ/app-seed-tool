#!/usr/bin/env python3
"""Speculos regression check: cancelling mid-entry on the "Check BIP39"
screen must not leave typed mnemonic content behind in RAM.

Scenario: type one word, confirm it (lands in the `mnemonic` static buffer,
src/nbgl/bip39_mnemonic.c), start typing a second word but don't confirm it,
then press "back" until the entry screen is left. Read the raw `mnemonic`
buffer over a GDB connection before and after, on the real running app --
not the source, the actual bytes in RAM.

Usage:
    docker run --rm -v "$(pwd)":/app \\
      ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \\
      bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'   # build first, once
    python3 tests/speculos/verify_bip39_cancel_clears_buffer.py

Exit code 0 and "PASS" printed if the typed word no longer appears anywhere
in the buffer after cancelling. Exit code 1 and "FAIL" if it does -- that is
a real bug (a secret mnemonic word staying resident in RAM after the user
backed out), not a test infrastructure problem.

See README.md in this directory for what this script works around (the
Speculos debug image, GDB-vs-SIGILL, and how the target's own RAM address
is found) and why.
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
CONTAINER_NAME = "speculos-bip39-cancel-test"
API_PORT = 5001
GDB_PORT = 1234
BASE_URL = f"http://localhost:{API_PORT}"

# Link-time code base for the flex target (from the SDK's scatter-loading)
# vs. the address speculos's launcher actually maps it at. Verified
# empirically via /proc/<pid>/maps and confirmed by successfully hitting
# breakpoints there -- see README.md. Only code addresses need this
# translation; the `mnemonic` buffer's real address is found a different
# way at runtime -- see MemorySampler._pump() and README.md.
CODE_LINK_BASE = 0xC0DE0000
CODE_RUNTIME_BASE = 0x40000000

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
    source change to this file). Returns {name: (address, size)}."""
    out = docker_run_tool("nm", "-S", "/app/app.elf")
    wanted = {"bip39_mnemonic_reset", "bip39_mnemonic_word_remove",
              "bip39_mnemonic_word_add", "mnemonic"}
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
            "(did bip39_mnemonic.c change function/variable names?)")
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
    actually maps it. `link_addr` may carry the Thumb bit (bit 0, per ARM
    ELF convention for Thumb function symbols) -- stripped here since it's
    not part of the real memory address."""
    even = link_addr & ~1
    return CODE_RUNTIME_BASE + (even - CODE_LINK_BASE)


def prepare_patched_speculos_main(scratch_dir):
    """The published ghcr.io/ledgerhq/speculos:latest image's debug mode
    (`-d`, used to get a GDB port) is broken: its main.py passes
    `-singlestep` to qemu-arm-static, an option removed from the bundled
    QEMU (10.0.8). Patch it to use `-one-insn-per-tb`, the modern
    replacement for the same purpose. See README.md."""
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
    straight through (the app's BOLOS syscall trampolines are deliberate
    illegal instructions -- see README.md), and takes a synchronous memory
    snapshot of `mnemonic` at the entry and return of each watched
    function, keyed by function name.

    Breakpoints are used (not an async Ctrl-C interrupt) because the app
    spends most of its time blocked in a real host syscall waiting for the
    next touch event; an injected interrupt byte was found not to be
    processed until *something* wakes the CPU's dispatch loop up again --
    unreliable. A breakpoint fires synchronously, exactly when the CPU
    itself reaches that instruction, with no such timing dependency.
    """

    def __init__(self, watch_addrs, mnemonic_offset, mnemonic_len):
        self.rsp = RSP(timeout=SOCKET_TIMEOUT)
        self.watch = watch_addrs  # {runtime_addr: name}
        self.mnemonic_offset = mnemonic_offset
        self.mnemonic_len = mnemonic_len
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
                continue  # genuinely idle, no SIGILL traffic right now
            except (ConnectionError, OSError):
                return  # container torn down from under us, nothing left to pump
            if self.rsp.stop_signal(reply) == SIGILL:
                self.rsp.cont_with_signal(SIGILL)
                continue

            regs = self.rsp.read_regs()
            pc = regs[15] & ~1
            mnemonic_addr = regs[9] + self.mnemonic_offset

            if self._pending_return and pc == self._pending_return[0]:
                ret_addr, name, before = self._pending_return
                after = self.rsp.read_mem(mnemonic_addr, self.mnemonic_len)
                self.snapshots.append((name, before, after))
                self.rsp.remove_bp(ret_addr, kind=2)
                self._pending_return = None
                self.rsp.cont()
                continue

            if pc in self.watch:
                name = self.watch[pc]
                ret_addr = regs[14] & ~1
                before = self.rsp.read_mem(mnemonic_addr, self.mnemonic_len)
                self.rsp.insert_bp(ret_addr, kind=2)
                self._pending_return = (ret_addr, name, before)
                # step over the breakpoint we're sitting on before resuming,
                # or we'd hit it again immediately without progressing
                self.rsp.remove_bp(pc, kind=2)
                self.rsp.step()
                self.rsp.insert_bp(pc, kind=2)
                self.rsp.cont()
                continue

            # unrelated stop (shouldn't normally happen); don't get stuck
            self.rsp.cont()


def navigate_and_cancel():
    """Coordinates are for the flex layout only (480x600, touch UI). See
    README.md for how these were found and how to adapt to stax/apex."""
    time.sleep(3)  # let the app finish booting before the first tap
    tap(371, 436)  # home screen: "Select Tool"
    time.sleep(1)
    tap(358, 524)  # "BIP39 Check"
    time.sleep(1)
    tap(387, 524)  # "12 words"
    time.sleep(1)

    # word 1: type "aban", confirm the "abandon" suggestion -> lands in
    # mnemonic.buffer via bip39_mnemonic_word_add()
    for x, y in [(40, 474), (207, 546), (40, 474), (256, 546)]:
        tap(x, y)
    tap(176, 300)

    # word 2: type "aba" but never confirm it -- this is the "cancel while
    # typing" moment the scenario is about
    for x, y in [(40, 474), (207, 546), (40, 474)]:
        tap(x, y)

    texts = screen_texts()
    if "abandon" not in texts:
        die(f"expected 'abandon' suggestion on screen before cancelling, got: {texts}")

    # back out: 1st tap removes the confirmed word (bip39_mnemonic_word_remove
    # -> bip39_mnemonic_shrink, the real per-word clearing path), 2nd tap
    # exits the screen entirely (bip39_mnemonic_reset, defense in depth)
    tap(48, 48)
    time.sleep(1)
    tap(48, 48)
    time.sleep(1)


def main():
    check_build()
    print("Resolving symbols from build/flex/bin/app.elf ...")
    syms = resolve_symbols()
    bss_base = resolve_bss_base()
    mnemonic_addr, mnemonic_len = syms["mnemonic"]
    mnemonic_offset = mnemonic_addr - bss_base
    watch = {
        code_runtime_addr(syms["bip39_mnemonic_word_add"][0]): "bip39_mnemonic_word_add",
        code_runtime_addr(syms["bip39_mnemonic_word_remove"][0]): "bip39_mnemonic_word_remove",
        code_runtime_addr(syms["bip39_mnemonic_reset"][0]): "bip39_mnemonic_reset",
    }
    print(f"  mnemonic buffer: offset R9+0x{mnemonic_offset:x}, {mnemonic_len} bytes")
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
            sampler = MemorySampler(watch, mnemonic_offset, mnemonic_len)
            sampler.start()
            print("Navigating: home -> BIP39 Check -> 12 words -> type+confirm "
                  "'abandon' -> type partial 'aba' -> cancel (back x2) ...")
            print("(this is slow -- every guest syscall round-trips through this "
                  "script while GDB is attached; a full run takes a few minutes)")
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
        nz_before = sum(1 for b in before if b)
        nz_after = sum(1 for b in after if b)
        print(f"  {name}: before_nonzero={nz_before}/{len(before)} "
              f"after_nonzero={nz_after}/{len(after)}")
        if name == "bip39_mnemonic_word_add" and add_hit is None:
            add_hit = (before, after)
        if name == "bip39_mnemonic_word_remove" and remove_hit is None:
            remove_hit = (before, after)

    if add_hit is None:
        die("bip39_mnemonic_word_add was never hit -- 'abandon' was never confirmed")
    if b"abandon" not in add_hit[1]:
        die("confirming 'abandon' did not write it into the mnemonic buffer -- "
            "test setup is broken, this isn't the bug under test")
    print("\n  confirmed: 'abandon' is present in RAM right after being typed "
          "(as expected -- this is what makes the next check meaningful)")

    if remove_hit is None:
        die("bip39_mnemonic_word_remove was never hit -- cancel didn't reach "
            "the buffer-clearing code path at all")
    before, after = remove_hit
    if b"abandon" not in before:
        die("bip39_mnemonic_word_remove fired without 'abandon' present before "
            "it -- navigation/timing assumption is wrong, re-check the scenario")
    # NOTE: the struct is *not* expected to be all-zero here -- e.g.
    # current_word_index is deliberately left at its (size_t)-1 "no word"
    # sentinel by bip39_mnemonic_shrink(), which is legitimate bookkeeping,
    # not a leak. The actual security property is that the *secret content*
    # (the word itself) is gone, so that's what's checked -- not "the whole
    # struct reads as zero".
    if b"abandon" in after:
        print("\nFAIL: 'abandon' is still present in the mnemonic buffer "
              "after cancelling -- typed content was not cleared:")
        print(f"  {after.hex()}")
        sys.exit(1)

    print("\nPASS: 'abandon' is no longer present in the mnemonic buffer "
          "after cancelling (bip39_mnemonic_word_remove -> bip39_mnemonic_shrink "
          "zeroed it)")
    sys.exit(0)


if __name__ == "__main__":
    main()
