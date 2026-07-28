# Speculos-based regression checks

Manual, first-of-its-kind checks that run the real app under Speculos and
inspect its actual RAM over a GDB connection, rather than reading source
code and hoping. `tests/functional/` (Ragger/pytest) already covers "does
the right thing show on screen" — this directory is for a different
question: "is a secret still sitting in memory after the user backed out?"

Not wired into CI or the cmocka harness. Run by hand when touching secret
buffer lifecycle code in the NBGL UI layer. See `TRACKING.md`/this repo's
usual process for whether/how to promote this further.

## Contents

- `rsp_client.py` — a ~100-line GDB Remote Serial Protocol client (stdlib
  only, no real `gdb` binary needed).
- `verify_bip39_cancel_clears_buffer.py` — the check itself.

## Prerequisites

1. Docker, with network access to `ghcr.io`.
2. The `flex` build, already compiled:

   ```bash
   docker run --rm -v "$(pwd)":/app \
     ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
     bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'
   ```

## Running

```bash
python3 tests/speculos/verify_bip39_cancel_clears_buffer.py
```

Takes a few minutes (see "Why is this so slow" below). Prints a snapshot
table and a `PASS`/`FAIL` line, exits 1 on failure. The script starts and
always tears down its own Speculos container (`speculos-bip39-cancel-test`)
— safe to re-run, and safe to Ctrl-C (though a stray container may need
`docker rm -f speculos-bip39-cancel-test` after a hard interrupt).

## What it checks

"Check BIP39" screen, flex layout: type a word, confirm it (so it lands in
the real `mnemonic` buffer, `src/nbgl/bip39_mnemonic.c`), start a second
word without confirming it, then cancel (back button) before finishing.
Reads raw memory before and after to confirm the confirmed word doesn't
survive the cancel.

**This exercises `bip39_mnemonic_word_remove()` /
`bip39_mnemonic_shrink()`, not `bip39_mnemonic_reset()`.** Worth spelling
out because it wasn't the initial assumption: reading `ui.c`
(`bip39_keyboard_dispatcher`), the back button calls
`bip39_mnemonic_word_remove()`, which internally calls
`bip39_mnemonic_shrink()` — *that's* what actually does the
`memzero()` for a real "typed a word, changed my mind" cancel.
`bip39_mnemonic_reset()` is only invoked when backing out of the *first*
word (nothing confirmed yet), to leave the screen entirely — a real call,
confirmed by this script too, but by construction the buffer is already
empty by the time it runs (shrink already cleared it on the way there), so
on its own it doesn't prove much. Both are exercised here; neither showed
a problem.

The check on "after" content is a byte-string containment check for the
typed word (`b"abandon" not in after`), **not** "the whole struct reads as
zero". `mnemonic.current_word_index` is deliberately left at its
`(size_t)-1` "no word" sentinel by `shrink()` — legitimate bookkeeping, not
a leak. Checking for an all-zero struct would be the wrong invariant and
would (did, in an earlier version of this script) produce a false FAIL.

## What you'd see on a real failure

The typed word's bytes turning up in the "after" snapshot, e.g.:

```
FAIL: 'abandon' is still present in the mnemonic buffer after cancelling -- typed content was not cleared:
  6162616e646f6e00...
```

That's a real bug (secret residue in RAM after the user cancelled) — not
an infrastructure problem. Don't dismiss it as flakiness; the memory read
happens synchronously at a breakpoint, there's no timing race to blame.

## Gotchas this script works around

None of this was documented anywhere before this script existed; recorded
here so the next person (or the next scenario) doesn't have to
rediscover it the hard way.

### 1. The published Speculos debug image is broken, and the fix isn't "done" once

`ghcr.io/ledgerhq/speculos:latest`'s `-d` (debug/GDB) mode passes
`-singlestep` to `qemu-arm-static`, an option removed from the bundled
QEMU (10.0.8; confirmed via `qemu-arm -help`, no `singlestep` option
listed). The script extracts `main.py` from the image at run time and
replaces `-singlestep` with `-one-insn-per-tb` (the modern equivalent) —
see `prepare_patched_speculos_main()`. This alone gets you a working GDB
port, but is *not* sufficient to keep the app usable while attached — see
next point.

### 2. A live GDB connection freezes the app solid, unless you tell it to ignore SIGILL

This app's BOLOS syscall trampolines (crypto, display, timing — anything
that becomes a real `SVC` on hardware) are implemented, under Speculos, as
deliberately illegal Thumb instructions that trap with `SIGILL`; the
launcher's own signal handler normally catches these and emulates the
syscall. The moment *any* GDB client is attached, QEMU's gdbstub reports
every single one of these traps as a stop instead of letting the launcher
handle it — and with a plain `continue` that's never told otherwise, the
target just sits there at the very first syscall of boot, forever. Screen
never renders, taps never register, no error, no timeout — it just never
makes progress. (Confirmed by explicitly closing the GDB socket mid-flight
and watching the exact same target immediately boot and render normally —
this really is about the connection being open, not slowness.)

Fix: every "continue" is a *redeliver-the-signal* continue
(`Cxx`, `RSP.cont_with_signal(4)`), which is what a real interactive
`gdb` session would do too after `handle SIGILL nostop noprint pass`. A
background thread does this in a loop so the main script can drive the
touch UI while it's running. Expect **thousands** of these per screen
interaction (`_pump()` round-trips through Python for every one) — this is
also why a run takes a few minutes instead of a few seconds. There's no
known way to avoid this overhead with this Speculos build; budget for it,
don't try to shrink the scenario to compensate.

### 3. Async Ctrl-C interrupts are not reliable here; breakpoints are

An earlier version of this script tried to grab a memory snapshot "at the
right moment" by sending the RSP interrupt byte (`\x03`) whenever the
navigation script decided the timing was right (e.g. right before pressing
"back"). This didn't work reliably: the app spends most of its idle time
blocked in a genuine host-level blocking syscall (waiting for the next
touch event), and the interrupt byte only got noticed once *something*
woke the CPU dispatch loop back up — not on any predictable schedule.

What works: software breakpoints (`Z0`/`z0`) on the three functions of
interest, hit synchronously by the CPU itself the moment it actually
executes that code — no timing assumption needed. `MemorySampler` also
sets a temporary breakpoint at the return address (from `LR`) so it can
snapshot memory both at entry (secret still present) and after return
(should be cleared), the RSP-level equivalent of gdb's `finish`.

### 4. The app's global variables are not at their linked address at runtime

The obvious approach — take the `mnemonic` symbol's address from
`nm` (`0xda7a0004` at the time of writing) and read that address over
GDB — silently returns all-zero garbage. Not an error, just the wrong
memory, which is worse (it looks like a false "PASS" for the buffer being
cleared, when really you're reading memory nothing ever touches).

The code is compiled position-independent; globals are addressed
`R9`-relative, not via their linked address. Disassembly of
`bip39_mnemonic_reset()` makes this explicit:

```
ldr   r0, [pc, #24]     ; r0 = 4  (this word: mnemonic's offset in .bss)
add.w r4, r9, r0        ; r4 = &mnemonic = R9 + 4
```

So the real address is `R9 + (link_addr_of_mnemonic - link_addr_of_.bss)`,
with `R9` read live from the running target (`RSP.read_regs()[9]`) —
never computed from `/proc/<pid>/maps` or any other static guess. Code
addresses (for breakpoints) are a separate, simpler story: the linked code
base (`0xC0DE0000` for this SDK's flex scatter-loading) maps 1:1-offset to
`0x40000000` at runtime — confirmed by successfully hitting breakpoints
placed with that translation, and stable across every container restart
observed. `code_runtime_addr()` in the script does this translation; the
script never hardcodes an actual breakpoint address, only the two base
constants, and resolves everything else from `nm`/`readelf` against
whatever `app.elf` was actually built.

## Adapting to stax/apex or another scenario

The touch coordinates in `navigate_and_cancel()` are flex-only
(480×600). `tests/functional/keypad.py` and `genericlayout.py` have the
equivalent stax/apex positions if this needs extending — note those are
Ragger-side helpers and aren't reused here directly, since this script
doesn't go through Ragger at all (see the top-level project notes on why:
the GDB/breakpoint machinery has nothing to do with Ragger's
navigator/backend layer, and mixing the two adds coupling for no benefit
in a single-scenario script like this one).

`rsp_client.py` and the `MemorySampler` breakpoint-snapshot pattern in
`verify_bip39_cancel_clears_buffer.py` are generic — reusable for other
secret-buffer-lifecycle scenarios (SSKR shares, BIP-85 passwords, the
stack-based `buffer[64]` in `compare_recovery_phrase()`, etc.) without
needing to re-solve any of the four gotchas above. A stack buffer would
need one more thing this script didn't: its address changes per call, so
you'd resolve it the same way as `mnemonic` (register-relative, likely SP-
or frame-pointer-relative) rather than reading a fixed offset once.
