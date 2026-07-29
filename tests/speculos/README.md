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
- `verify_bip39_cancel_clears_buffer.py` — check #1: the `mnemonic` global
  is cleared when the user cancels mid-entry on "Check BIP39".
- `verify_compare_recovery_phrase_cleanup.py` — check #2: the two 64-byte
  stack secrets in `compare_recovery_phrase()` (`src/common/common_seed.c`)
  are cleared at its `cleanup:` label. See "What it checks (scenario 2)"
  below.
- `verify_sskr_share_cancel_clears_buffer.py` — check #3: the `shares`
  global (`src/nbgl/sskr_shares.c`) is cleared when the user cancels
  mid-entry on "Check SSKR", the SSKR analogue of check #1. See "What it
  checks (scenario 3)" below.

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
python3 tests/speculos/verify_compare_recovery_phrase_cleanup.py
python3 tests/speculos/verify_sskr_share_cancel_clears_buffer.py
```

Takes a few minutes (see "Why is this so slow" below) for the first script.
**The second one is much slower** — it types and confirms all 12 words of
a full mnemonic instead of one partial word, and a full run has taken
30+ minutes in practice. That is not a hang: the per-keystroke render
round-trip through the SIGILL-passthrough overhead (gotcha #2) can run
into the tens of seconds under load, and there is no per-word progress
line in the script's own output to show it — if you need to confirm it's
actually making progress rather than stuck, poll the container's screen
state directly instead of waiting on stdout:
```bash
curl -s 'http://localhost:5002/events?currentscreenonly=true' | python3 -m json.tool
```
(the `"Enter word n. X/12..."` text tells you exactly which word it's on).
The SSKR check only types one word plus a couple of letters, so it's back
to a few-minutes run like the first script, not 30+.

All three scripts print a snapshot table and a `PASS`/`FAIL` line, exit 1
on failure, and start/always tear down their own Speculos container
(`speculos-bip39-cancel-test` / `speculos-compare-recovery-phrase-cleanup-test`
/ `speculos-sskr-cancel-test`) — safe to re-run, and safe to Ctrl-C (though
a stray container may need `docker rm -f <name>` after a hard interrupt).

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

## What it checks (scenario 2: `compare_recovery_phrase()` cleanup)

`verify_compare_recovery_phrase_cleanup.py` closes the loop on a fix made
earlier in this project's history purely by code review: three early
returns in `compare_recovery_phrase()` (`src/common/common_seed.c`) were
collapsed into a single `goto cleanup;`, but never actually exercised
under a debugger until this script.

"Check BIP39", flex layout: type and confirm all 12 words of a known
128-bit test vector (`fly mule excess resource treat plunge nose soda
reflect adult ramp planet` — the same one `tests/functional/
test_bip39_12word.py` already uses, from `sskr-test-vector.md`). Speculos
is booted with `-s "<that mnemonic>"` so the device's own seed matches
what's typed — `compare_recovery_phrase()` then takes its match path, and
its two 64-byte stack locals (`buffer`, the input-derived root key;
`buffer_device`, the device's root key) both hold the exact same,
independently computable secret right before cleanup:
```python
seed = hashlib.pbkdf2_hmac("sha512", mnemonic.encode(), b"mnemonic", 2048, dklen=64)
root_key = hmac.new(b"Bitcoin seed", seed, hashlib.sha512).digest()
```
Confirming the 12th word triggers `bip39_mnemonic_check()`
(`src/nbgl/bip39_mnemonic.c`) → `compare_recovery_phrase()` automatically;
no further navigation is needed once both breakpoints fire.

Unlike the `mnemonic`/`shares` globals check (scenario 1), there's no
legitimate non-zero bookkeeping field living inside these two raw 64-byte
arrays, so the pass condition here really is "reads as all-zero after
cleanup", not just "no longer contains the specific secret".

### What you'd see on a real failure

`buffer` or `buffer_device` still containing the independently-computed
root key (or just being non-all-zero) after the `after cleanup`
breakpoint — the script prints which buffer and its hex on failure. That
would mean a real regression in the `goto cleanup;` fix (e.g. a future
edit reintroducing an early return that bypasses it) — not an
infrastructure problem, same reasoning as scenario 1.

## What it checks (scenario 3: SSKR share entry cancel)

`verify_sskr_share_cancel_clears_buffer.py` is check #1's direct sibling,
for the `shares` global (`src/nbgl/sskr_shares.c`) instead of `mnemonic`.

"Check SSKR" screen, flex layout: type an SSKR share word, confirm it (so
it lands in the real `shares.buffer` via `sskr_shares_word_add()`), start
a second word without confirming it, then cancel (back button). Unlike
BIP39, selecting "SSKR Check" from the tool-select screen goes straight to
the entry keyboard — there's no length-selection step in between
(`select_tool_callback()` in `src/nbgl/ui.c`).

**Confirmed by reading `sskr_shares_word_remove()`, not assumed going in:**
it calls `sskr_shares_shrink()`, the real clearing mechanism on this path
— same structure as `bip39_mnemonic_word_remove()` →
`bip39_mnemonic_shrink()` in check #1. `sskr_shares_reset()` is, again,
defense in depth: exercised by the second "back" tap, but by then the
buffer is already empty.

**Non-obvious pick: the test word is `"acid"`, not `"able"`.**
`bolos_ux_sskr_byteword_to_hex()` (`src/common/sskr/seed_sskr.c`) decodes
a typed word via a linear scan of `SSKR_WORDLIST`
(`src/common/sskr/seed_rom_variables.c`) and returns the word's index in
that list as the byte value. `"able"` is index 0 — decoding it writes
`0x00` into `shares.buffer[0]`, which is indistinguishable from "never
written" or "correctly cleared". `"acid"`, index 1, decodes to `0x01`: an
unambiguous non-zero value to check for both presence (right after
confirming) and absence (right after cancelling). Worth remembering for
any future SSKR-buffer scenario — the all-zero-by-default trap isn't
specific to this one.

### What you'd see on a real failure

Byte `0x01` still present at `shares.buffer[0]` after the "back" tap that
should have cleared it — the script prints the full captured bytes on
failure. Same reasoning as the other two checks: this is a synchronous
breakpoint read, not a timing-sensitive guess, so a FAIL here is real.

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
`verify_bip39_cancel_clears_buffer.py` are generic — reused as-is for
`shares` in `verify_sskr_share_cancel_clears_buffer.py` (check #3), and
still applicable to other secret-buffer-lifecycle scenarios (BIP-85
passwords, *generated* SSKR shares on dashboard return, etc.) without
needing to re-solve any of the four gotchas above.

### 5. Stack-local secrets (as opposed to globals) are SP-relative, and simpler than they look

`verify_compare_recovery_phrase_cleanup.py` needed this for
`compare_recovery_phrase()`'s `buffer`/`buffer_device` (both plain
`uint8_t foo[64]` stack locals, not globals). Confirmed by disassembling
the function (`objdump -d`, or `arm-none-eabi-objdump -d` if the image's
default `objdump` can't disassemble ARM for you — see below): the
prologue is `push {r4,r5,r7,lr}` then a single `sub sp, #N`, and every
reference to a stack local in the function body compiles to `add rX, sp,
#offset` — plain SP-relative, no frame pointer involved, *and* SP itself
is stable for the entire function body (set once by that `sub`, restored
only by the matching `add sp, #N` in the epilogue). That means, unlike a
global's `R9`-relative address (gotcha #4), there's no separate "capture
the frame base" step needed: read `regs[13]` (SP) directly at whichever
breakpoint you've placed inside the function, add the fixed offset you
got from the disassembly, done. This is simpler than register-relative
globals, not harder — the speculation in an earlier version of this
section ("likely SP- or frame-pointer-relative, to be confirmed") is now
resolved: it's SP-relative, and it's stable.

One extra gotcha specific to disassembly (not memory reads): the generic
`objdump` in the `ledger-app-builder-lite` image can read section/symbol
tables fine (`nm`, `readelf` work unprefixed) but fails to disassemble
ARM code at all (`can't disassemble for architecture UNKNOWN`) — use
`arm-none-eabi-objdump -d` explicitly for that.
