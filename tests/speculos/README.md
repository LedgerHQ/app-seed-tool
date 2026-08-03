# Speculos-based regression checks

These scripts run the real app under Speculos and inspect its actual RAM
over a GDB connection, instead of relying on source-code review alone.
`tests/functional/` (Ragger/pytest) already covers "does the right thing
show on screen"; this directory answers a different question: "is a secret
still sitting in memory after the user backed out?"

They are not wired into CI or the cmocka harness. Run them by hand when
touching secret-buffer lifecycle code in the NBGL UI layer.

## Table of contents

- [Files](#files)
- [Prerequisites](#prerequisites)
- [Running](#running)
- [Checks](#checks)
  - [Check 1 — BIP39 mnemonic buffer, cancel mid-entry](#check-1--bip39-mnemonic-buffer-cancel-mid-entry)
  - [Check 2 — `compare_recovery_phrase_finish()` buffer erasure](#check-2--compare_recovery_phrase_finish-buffer-erasure)
  - [Check 3 — SSKR share entry buffer, cancel mid-entry](#check-3--sskr-share-entry-buffer-cancel-mid-entry)
  - [Check 4 — Generated SSKR shares, dashboard return](#check-4--generated-sskr-shares-dashboard-return)
  - [Check 5 — Generated BIP-85 BIP39 output, dashboard return](#check-5--generated-bip-85-bip39-output-dashboard-return)
  - [Check 6 — App reopen clears the mnemonic buffer](#check-6--app-reopen-clears-the-mnemonic-buffer)
- [Gotchas](#gotchas)
  - [1. The published Speculos debug image is broken](#1-the-published-speculos-debug-image-is-broken)
  - [2. A live GDB connection freezes the app unless SIGILL is passed through](#2-a-live-gdb-connection-freezes-the-app-unless-sigill-is-passed-through)
  - [3. Async Ctrl-C interrupts are unreliable; breakpoints are not](#3-async-ctrl-c-interrupts-are-unreliable-breakpoints-are-not)
  - [4. Global variables are not at their linked address at runtime](#4-global-variables-are-not-at-their-linked-address-at-runtime)
  - [5. Stack-local secrets are SP-relative, and simpler than globals](#5-stack-local-secrets-are-sp-relative-and-simpler-than-globals)
  - [6. The GDB stub is not ready when the container is](#6-the-gdb-stub-is-not-ready-when-the-container-is)
- [Adapting to stax/apex, or another scenario](#adapting-to-staxapex-or-another-scenario)

## Files

| File | Role |
|---|---|
| `rsp_client.py` | ~100-line GDB Remote Serial Protocol client (stdlib only, no `gdb` binary required). |
| `verify_bip39_cancel_clears_buffer.py` | Check 1 — BIP39 mnemonic entry, cancel mid-word. |
| `verify_compare_recovery_phrase_cleanup.py` | Check 2 — `compare_recovery_phrase_finish()` buffer erasure. |
| `verify_sskr_share_cancel_clears_buffer.py` | Check 3 — SSKR share entry, cancel mid-word. |
| `verify_sskr_generated_shares_dashboard_return.py` | Check 4 — generated SSKR shares, dashboard return. |
| `verify_bip85_bip39_output_cleanup.py` | Check 5 — generated BIP-85 BIP39 output, dashboard return. |
| `verify_app_reopen_clears_mnemonic.py` | Check 6 — app reopen, no residual mnemonic across a close/reopen. |

## Prerequisites

1. Docker, with network access to `ghcr.io`.
2. The `flex` build, already compiled:

   ```bash
   docker run --rm -v "$(pwd)":/app \
     ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest \
     bash -c 'BOLOS_SDK=/opt/flex-secure-sdk make -j4'
   ```

## Running

All six are executable and carry a `#!/usr/bin/env python3` shebang, so they
can be run directly (prefixing `python3` also works):

```bash
./tests/speculos/verify_bip39_cancel_clears_buffer.py
./tests/speculos/verify_compare_recovery_phrase_cleanup.py
./tests/speculos/verify_sskr_share_cancel_clears_buffer.py
./tests/speculos/verify_sskr_generated_shares_dashboard_return.py
./tests/speculos/verify_bip85_bip39_output_cleanup.py
./tests/speculos/verify_app_reopen_clears_mnemonic.py
```

Durations below are measured, from one sequential run of all six on a single
host — treat them as an order of magnitude, not a guarantee; the dominant
cost is the per-syscall GDB round-trip (gotcha 2), so they scale with host
load.

| Script | Measured | Why |
|---|---|---|
| Check 1 (BIP39 cancel) | ~4.5 min | Types one word only. |
| Check 2 (`compare_recovery_phrase_finish`) | ~33 min | Types and confirms a full 12-word mnemonic. |
| Check 3 (SSKR cancel) | ~4 min | Types one word plus a couple of letters. |
| Check 4 (generated shares) | ~35 min | Types the full 12-word mnemonic, then generates and pages through 3 shares. |
| Check 5 (BIP-85 BIP39 output) | ~1 min | Button taps and one numeric-keypad entry only, no mnemonic typing. |
| Check 6 (app reopen) | ~33 min | Types and confirms a full 12-word mnemonic, then boots a second container. |

Running all six back to back therefore takes roughly two hours.

The long runs are not hangs. Every guest syscall round-trips through this
script while GDB is attached (see gotcha 2), and individual keystroke
renders have been observed lagging the actual tap by tens of seconds under
that load — there is no per-word progress line in the scripts' own output.
To confirm forward progress on a slow run, poll the container's screen
state directly instead of watching stdout:

```bash
curl -s 'http://localhost:5002/events?currentscreenonly=true' | python3 -m json.tool
```

(the `"Enter word n. X/12..."` text shows exactly which word is in
progress; the API port differs per script — see each script's `API_PORT`).

All six scripts print a snapshot table and a `PASS`/`FAIL` line, exit 1 on
failure, and always tear down their own Speculos container(s)
(`speculos-bip39-cancel-test` / `speculos-compare-recovery-phrase-cleanup-test`
/ `speculos-sskr-cancel-test` / `speculos-sskr-generated-shares-reset-test`
/ `speculos-bip85-bip39-output-cleanup-test` /
`speculos-app-reopen-test-1`+`speculos-app-reopen-test-2`, check 6 being the
one script that runs two containers, one after the other). Safe to re-run,
and safe to Ctrl-C — though a stray container may need `docker rm -f <name>`
after a hard interrupt.

## Checks

Each check follows the same shape: what screen/flow it drives, which
function actually does the clearing (confirmed by reading the code, never
assumed), and what a real failure looks like. All read memory via
synchronous breakpoints, not a timing-based sample — a reported `FAIL` is a
real regression, not flakiness. Check 6 is the one exception to "which
function actually does the clearing": it verifies a platform guarantee (RAM
reset across an app close/reopen), not this app's own cleanup code — see its
section below for why that distinction matters.

### Check 1 — BIP39 mnemonic buffer, cancel mid-entry

`verify_bip39_cancel_clears_buffer.py`. On the "Check BIP39" screen: type a
word, confirm it (lands in the real `mnemonic` buffer,
`src/nbgl/bip39_mnemonic.c`), start a second word without confirming it,
then cancel (back button) before finishing. Reads raw memory before and
after to confirm the confirmed word does not survive the cancel.

**Clearing mechanism, confirmed by reading `ui.c`
(`bip39_keyboard_dispatcher`), not assumed going in:** the back button
calls `bip39_mnemonic_word_remove()`, which calls
`bip39_mnemonic_shrink()` — that is what performs the `memzero()` for a
"typed a word, changed my mind" cancel. `bip39_mnemonic_reset()` only runs
when backing out of the *first* word (nothing confirmed yet), to leave the
screen entirely; it is exercised here too, but by construction the buffer
is already empty by the time it runs, so on its own it proves little. Both
paths are exercised; neither showed a problem.

The "after" check is a byte-string containment check for the typed word
(`b"abandon" not in after`), not "the whole struct reads as zero".
`mnemonic.current_word_index` is deliberately left at its `(size_t)-1` "no
word" sentinel by `shrink()` — legitimate bookkeeping, not a leak. An
all-zero-struct check would be the wrong invariant (an earlier version of
this script used it and produced a false `FAIL`).

**On a real failure**, the typed word's bytes turn up in the "after"
snapshot:

```
FAIL: 'abandon' is still present in the mnemonic buffer after cancelling -- typed content was not cleared:
  6162616e646f6e00...
```

### Check 2 — `compare_recovery_phrase_finish()` buffer erasure

`verify_compare_recovery_phrase_cleanup.py`. Closes the loop on a fix made
earlier in this project purely by code review: three early returns in
`compare_recovery_phrase()` (`src/common/common_seed.c`) were collapsed
into a single `goto cleanup;`, but were never exercised under a debugger
until this script was first written.

**Mechanism update.** `compare_recovery_phrase()`'s tail — everything
after the device-seed derivation, including both `explicit_bzero()` calls
this check watches — has since been extracted into its own function,
`compare_recovery_phrase_finish(cx_err_t derivation_status, uint8_t
buffer[64], uint8_t buffer_device[64])`, so the derivation-failure path
could get unit-test coverage on host (the syscall itself still can't be).
Confirmed by disassembly (`arm-none-eabi-nm -S` on the `flex` build), not
assumed: this stayed a real, separate symbol rather than being inlined
back into its one caller —

```
c0de08d4 000000a8 T compare_recovery_phrase
c0de08a2 00000030 T compare_recovery_phrase_finish
```

— so this script's breakpoints now resolve `compare_recovery_phrase_finish`
and disassemble *that* function instead of `compare_recovery_phrase`
itself. The user-visible behavior this check verifies is unchanged (both
buffers are still erased on the same path); only where the two
`explicit_bzero()` calls physically live moved. This also simplified the
addressing: `compare_recovery_phrase_finish()` receives `buffer`/
`buffer_device` as plain pointer arguments rather than declaring them as
its own stack-local arrays — the compiler keeps both pointers in
call-preserved registers for the function's whole body instead of
spilling them to the stack. The function does now have a stack frame (the
fault-resistance work gave it a volatile checkpoint counter and a
volatile spin variable, both of which have to live in memory), but the
two pointers are not in it. That means this check no longer needs
gotcha 5's SP-relative-offset approach: it reads the two argument
registers directly at each breakpoint (which register holds which buffer
is read back from the disassembly, not hardcoded — see
`resolve_cleanup_breakpoints()`).

On "Check BIP39": type and confirm all 12 words of a known 128-bit test
vector (`fly mule excess resource treat plunge nose soda reflect adult
ramp planet` — the same one `tests/functional/test_bip39_12word.py` uses,
sourced from `sskr-test-vector.md`). Speculos boots with `-s "<that
mnemonic>"` so the device's own seed matches what is typed;
`compare_recovery_phrase()` then takes its match path and calls
`compare_recovery_phrase_finish()`, whose two 64-byte buffers (`buffer`,
the input-derived root key; `buffer_device`, the device's root key) both
hold the same, independently computable secret right before erasure:

```python
seed = hashlib.pbkdf2_hmac("sha512", mnemonic.encode(), b"mnemonic", 2048, dklen=64)
root_key = hmac.new(b"Bitcoin seed", seed, hashlib.sha512).digest()
```

Confirming the 12th word triggers `bip39_mnemonic_check()`
(`src/nbgl/bip39_mnemonic.c`) → `compare_recovery_phrase()` →
`compare_recovery_phrase_finish()` automatically; no further navigation is
needed once both breakpoints fire.

Unlike the global-buffer checks (1 and 3), neither `buffer` nor
`buffer_device` has a legitimate non-zero bookkeeping field, so the pass
condition here is "reads as all-zero after erasure", not just "no longer
contains the specific secret".

**On a real failure**, `buffer` or `buffer_device` still contains the
independently-computed root key (or is simply non-zero) after the
`after cleanup` breakpoint; the script prints the offending buffer and its
hex. That would mean a regression in the buffer-erasure logic — for
example a future edit reintroducing an early return that bypasses it.

### Check 3 — SSKR share entry buffer, cancel mid-entry

`verify_sskr_share_cancel_clears_buffer.py`, check 1's direct sibling for
the `shares` global (`src/nbgl/sskr_shares.c`) instead of `mnemonic`.

On "Check SSKR": type an SSKR share word, confirm it (lands in
`shares.buffer` via `sskr_shares_word_add()`), start a second word without
confirming it, then cancel. Unlike BIP39, selecting "SSKR Check" from the
tool-select screen goes straight to the entry keyboard — there is no
length-selection step in between (`select_tool_callback()` in
`src/nbgl/ui.c`).

**Clearing mechanism, confirmed by reading `sskr_shares_word_remove()`,
not assumed going in:** it calls `sskr_shares_shrink()`, the real clearing
mechanism on this path — same structure as check 1's
`bip39_mnemonic_word_remove()` → `bip39_mnemonic_shrink()`.
`sskr_shares_reset()` is again defense in depth: exercised by the second
"back" tap, but by then the buffer is already empty.

**Test-word choice: `"acid"`, not `"able"`.**
`bolos_ux_sskr_byteword_to_hex()` (`src/common/sskr/seed_sskr.c`) decodes a
typed word via a linear scan of `SSKR_WORDLIST`
(`src/common/sskr/seed_rom_variables.c`) and returns the word's index in
that list as the byte value. `"able"` is index 0 — decoding it writes
`0x00` into `shares.buffer[0]`, indistinguishable from "never written" or
"correctly cleared". `"acid"`, index 1, decodes to `0x01`: an unambiguous
non-zero value to check for both presence (right after confirming) and
absence (right after cancelling). This all-zero-by-default trap is not
specific to this one scenario — worth checking for in any future
SSKR-buffer test.

**On a real failure**, byte `0x01` is still present at
`shares.buffer[0]` after the "back" tap that should have cleared it; the
script prints the full captured bytes.

### Check 4 — Generated SSKR shares, dashboard return

`verify_sskr_generated_shares_dashboard_return.py` answers a genuinely
open question, not a presupposed bug. `sskr_shares_from_bip39_mnemonic()`
(`src/nbgl/sskr_shares.c`) writes *generated* shares into the same
`shares` global used by manual entry (check 3), and its caller,
`sskr_shares_check()`, has a comment explicitly deferring the erase:
"Don't clear the shares just yet as we may need it to generate BIP39
mnemonic". Does the real generate → view → return-to-dashboard flow always
reach `sskr_shares_reset()` before the buffer goes out of scope, or is
there an exit path that skips it?

Flow: type and confirm the full 12-word test mnemonic (same vector and
`-s` boot as check 2, needed so `bip39_mnemonic_check()`'s `seed_match`
comes back `true` — the UI only offers "Generate SSKR" on a real match),
choose "Generate SSKR" with 3 shares / threshold 2 (the same combination
`tests/functional/test_bip39_12word.py` exercises for real — this script
sanity-checks the rendered share text against that test's known
`"tuna next keep gyro"` prefix before proceeding, so UI/navigation drift
fails loudly as a setup problem rather than producing a false result),
page through all 3 generated shares, then tap the review screen's exit
control — the same `next`/`next`/`exit` sequence
`tests/functional/conftest.py`'s `all_eink_bip39_12word()` already proves
works on this exact screen.

**Result: `sskr_shares_reset()` does fire on this path, and the buffer is
genuinely cleared.** Captured on a real run:

```
sskr_shares_from_bip39_mnemonic: after=74756e61206e657874206b656570206779726f20706c757320696365642061626c6520616369642061626c65206c656773207065636b206269617320626c7565  (64/64 bytes nonzero)
sskr_shares_reset:               before=<same bytes>  after=00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000  (all zero)
```

Two observations from this run, neither a bug:

- **Generated `shares.buffer` holds literal ASCII ByteWords text**, unlike
  check 3's decoded-byte-per-word encoding for manual entry. The captured
  "before" bytes above decode as `"tuna next keep gyro plus iced able
  acid able legs peck bias blue"` — the share's rendered words themselves,
  space-separated. The encoding is not uniform across every code path
  that touches this struct; check per call site.
- **`sskr_shares_reset()` fires twice in a row on this exit path**
  (visible as two `before=all-zero after=all-zero` no-op snapshots
  bracketing the real one in a full capture): `review_done()` calls
  `reset_globals()` explicitly, and `display_home_page()` — which it then
  calls — calls `reset_globals()` again itself at its own top. Redundant,
  not harmful; the second call has nothing left to clear.

**On a real failure**, either `shares.buffer` is still non-zero right
after the exit tap even though `sskr_shares_reset()` did fire (a wipe that
doesn't actually wipe), or — the more structurally interesting failure —
`sskr_shares_reset()` never fires at all despite the script confirming, via
`screen_texts()`, that navigation genuinely reached the dashboard. The
script distinguishes these explicitly rather than lumping them into one
generic `FAIL`, since they point at different bugs: a broken `memzero()`
versus a missing call on some exit path.

### Check 5 — Generated BIP-85 BIP39 output, dashboard return

`verify_bip85_bip39_output_cleanup.py` covers the most sensitive of the
three BIP-85 apps: `bip85_app_bip39_gen()` (`src/nbgl/bip85_app.c`)
produces a real recovery-phrase-shaped mnemonic, not just a password. One
call writes into two `static` globals: `app_data.buffer`
(`src/nbgl/bip85_app.c`) with the raw derived entropy, then
`mnemonic.buffer` (`src/nbgl/bip39_mnemonic.c` — the same static already
watched by check 1) with the encoded phrase, via
`bip39_mnemonic_encode(app_data.buffer, app_data.length)`.

Flow: home → "BIP85 Generate" → "BIP39" → "12 words" → index `1` → review
screen → "Done". No mnemonic typing is needed anywhere on this path (it is
all button taps and one numeric-keypad entry), so a run takes a few
minutes, not the 15–30+ of the checks that type a full 12-word phrase.

**Clearing mechanism, confirmed by reading `ui.c`, not assumed going in:**
the erase path is `reset_globals()` (`static`, called from `review_done()`
after "Done" on the same `nbgl_useCaseGenericReview` widget already proven
not to be bypassable, in check 4). `reset_globals()` is small enough that
the compiler could plausibly inline it into its callers, so this script
watches the two real, non-static functions it calls directly instead:
`bip39_mnemonic_reset()` (already known from check 1) and
`bip85_app_reset()` (new — confirmed to be a full
`memzero(&app_data, sizeof(app_data))`, unlike `bip39_mnemonic_reset()`,
which does the same for `mnemonic` but then deliberately leaves
`current_word_index` at its `(size_t)-1` sentinel, exactly like check 1).
Since `bip85_app_reset()` runs last in that sequence, its return is a
clean point to snapshot both buffers at once.

**On a real run**, `bip85_app_bip39_gen()` returned with real generated
content in both buffers — the captured `mnemonic` bytes decode as
`"reduce burger sign project owner gun caught clarify monster occur
sustain hazard"`, a genuine 12-word phrase. After "Done",
`bip85_app_reset()` fired and both buffers were clear: `app_data.buffer`
(96/96 bytes) fully zero, and `mnemonic.buffer`'s phrase-text portion
(the first 216 of its 324 bytes — the rest is the same bookkeeping tail as
check 1: `length`/`current_word_index`/`word_lengths[]`/`final_size`)
fully zero too, with only the expected sentinel in the excluded tail.

**Non-obvious pitfall while building this check, worth recording:** an
early version asserted the *entire* `mnemonic` struct must read all-zero
after cleanup, reasoning from `bip85_app_reset()`'s own full-`memzero()`
body — but `mnemonic` is cleared by `bip39_mnemonic_reset()`, not
`bip85_app_reset()`, and that function's sentinel-setting behavior is
exactly what check 1 already documents. The blanket "whole struct is
zero" property from `app_data` does not carry over to `mnemonic`; each
buffer's actual clearing function has to be checked on its own terms, not
assumed uniform because they get wiped from the same reset path.

**On a real failure**, either buffer still containing generated content
after `bip85_app_reset()` fires — the script prints the offending bytes
and distinguishes `app_data` from `mnemonic.buffer` explicitly.

### Check 6 — App reopen clears the mnemonic buffer

`verify_app_reopen_clears_mnemonic.py` is different in kind from checks 1-5.
`git grep -n "nvm_write\|nvm_erase" -- src/` finds nothing: this app never
writes to NVM/flash, so every secret buffer — the `mnemonic` global watched
here included — lives exclusively in RAM. The only way a secret could
survive an app close-then-reopen is if BOLOS does not actually reset that
RAM when (re)loading the app process — a platform guarantee, not something
this app's own code controls. This script verifies that guarantee
empirically rather than looking for an application bug.

Flow: start a first Speculos container, type and confirm all 12 words of the
same test mnemonic used by check 2 (`fly mule excess resource treat plunge
nose soda reflect adult ramp planet`) on "Check BIP39", then read the real
`mnemonic` buffer at the return of `bip39_mnemonic_check()`
(`src/nbgl/bip39_mnemonic.c`) — the point furthest into the flow where the
buffer is still guaranteed to hold the full typed phrase, since that
function deliberately does not reset it on a valid-checksum path ("Don't
clear the mnemonic just yet as we may need it to generate BIP39 mnemonic").
Tear that container down entirely, start a second, completely independent
container from the same `app.elf` (no shared volumes/state with the first),
and read the same global — its `R9`-relative address re-resolved from
scratch, never assumed identical to the first session, see gotcha 4 — before
any simulated user interaction, at the entry of `ui_idle_init()` /
`display_home_page()` (`src/nbgl/ui.c`), the very first UI code a fresh
process runs at boot.

**Why `docker stop`/`rm` rather than tapping the real "Quit app" action,
decided deliberately, not a shortcut for lack of trying:** `on_quit()`
(`src/nbgl/ui.c`, `os_sched_exit(-1)`) is trivially reachable — confirmed
empirically, it is a "Quit app" footer link directly on the home screen
(`nbgl_useCaseHomeAndSettings`) — but reaching it from the
just-confirmed-12-words state requires first navigating back to the
dashboard, and `display_home_page()` unconditionally calls
`reset_globals()` (→ `bip39_mnemonic_reset()`) as its first statement,
before it ever draws that screen. Doing that would wipe the buffer with this
app's own defense-in-depth code (already verified by checks 1/4/5) before
the platform-level guarantee under test is ever exercised — a `PASS` reached
that way would be ambiguous (clean because of BOLOS, or clean because of
application code that had nothing to do with the platform question this
script asks?). Tearing the first container down directly, right after the
sanity-check read, keeps the buffer populated with real content up to the
moment the first process actually disappears — the more rigorous test.
Symmetrically, the second container's read point is deliberately
`ui_idle_init()`'s entry rather than "as soon as GDB can attach": reading
after `display_home_page()`'s own `reset_globals()` call would let this
fresh process's own init code launder away whatever the loader actually
left behind, which is exactly what this script needs to observe untouched.

**On a real run**, the first container's buffer at `bip39_mnemonic_check()`
return held the full typed phrase (88/324 bytes nonzero — the 73-byte phrase
text plus the same non-secret bookkeeping tail documented in check 5:
`length`/`current_word_index`/`word_lengths[]`/`final_size`):

```
666c79206d756c6520657863657373207265736f7572636520747265617420706c756e6765...
```
(decodes as `"fly mule excess resource treat plunge nose soda reflect adult
ramp planet"`, followed by zero padding and the expected bookkeeping tail).
The second container's buffer, read at `ui_idle_init()` entry before any
interaction (with a freshly re-resolved `R9`, confirmed different from the
first container's), was entirely zero (0/324 bytes nonzero) — `PASS`.

**On a real failure** (any of the first session's 12 typed words present in
the second container's buffer): do not treat this as a routine one-line
application fix. Possible causes range from a test-infrastructure artifact
(unintended shared state between the two Docker containers, a wrong address
resolution reading the wrong buffer) to something genuinely significant
about BOLOS's RAM loading/initialization under Speculos — either way, this
warrants understanding before proposing any fix.

## Gotchas

None of the following was documented anywhere before these scripts
existed; recorded here so the next person, or the next scenario, doesn't
have to rediscover it.

### 1. The published Speculos debug image is broken

`ghcr.io/ledgerhq/speculos:latest`'s `-d` (debug/GDB) mode passes
`-singlestep` to `qemu-arm-static`, an option removed from the bundled
QEMU (10.0.8; confirmed via `qemu-arm -help`, no `singlestep` option
listed). Each script extracts `main.py` from the image at run time and
replaces `-singlestep` with `-one-insn-per-tb` (the modern equivalent) —
see `prepare_patched_speculos_main()`. This gets a working GDB port, but is
not by itself sufficient to keep the app usable while attached — see
gotcha 2.

### 2. A live GDB connection freezes the app unless SIGILL is passed through

This app's BOLOS syscall trampolines (crypto, display, timing — anything
that becomes a real `SVC` on hardware) are implemented, under Speculos, as
deliberately illegal Thumb instructions that trap with `SIGILL`; the
launcher's own signal handler normally catches these and emulates the
syscall. The moment any GDB client attaches, QEMU's gdbstub reports every
one of these traps as a stop instead of letting the launcher handle it —
with a plain `continue` left unconfigured, the target sits at the first
syscall of boot forever. Screen never renders, taps never register, no
error, no timeout. (Confirmed by closing the GDB socket mid-flight and
watching the same target immediately boot and render normally — this is
about the connection being open, not slowness.)

Fix: every "continue" is a redeliver-the-signal continue (`Cxx`,
`RSP.cont_with_signal(4)`), equivalent to a real interactive `gdb` session
after `handle SIGILL nostop noprint pass`. A background thread does this
in a loop so the main script can drive the touch UI concurrently. Expect
thousands of these per screen interaction — this is also why a run takes
minutes instead of seconds. There is no known way to avoid this overhead
with this Speculos build; budget for it rather than trying to shrink the
scenario to compensate.

### 3. Async Ctrl-C interrupts are unreliable; breakpoints are not

An earlier version of the first script tried to grab a memory snapshot "at
the right moment" by sending the RSP interrupt byte (`\x03`) whenever the
navigation script judged the timing right (e.g. right before pressing
"back"). This did not work reliably: the app spends most of its idle time
blocked in a genuine host-level blocking syscall waiting for the next
touch event, and the interrupt byte was only noticed once something woke
the CPU dispatch loop — on no predictable schedule.

What works: software breakpoints (`Z0`/`z0`) on the functions of interest,
hit synchronously by the CPU the moment it executes that code, with no
timing assumption needed. `MemorySampler` also sets a temporary breakpoint
at the return address (from `LR`) to snapshot memory both at entry (secret
still present) and after return (should be cleared) — the RSP-level
equivalent of gdb's `finish`.

### 4. Global variables are not at their linked address at runtime

Reading a global's `nm`-reported link address (`0xda7a0004` for `mnemonic`
at the time of writing) over GDB silently returns all-zero garbage — not
an error, just the wrong memory, which is worse: it looks like a false
`PASS` for the buffer being cleared when the read never touched real
memory.

The code is compiled position-independent; globals are addressed
`R9`-relative, not via their linked address. Disassembly of
`bip39_mnemonic_reset()` makes this explicit:

```
ldr   r0, [pc, #24]     ; r0 = 4  (this word: mnemonic's offset in .bss)
add.w r4, r9, r0        ; r4 = &mnemonic = R9 + 4
```

The real address is `R9 + (link_addr_of_mnemonic - link_addr_of_.bss)`,
with `R9` read live from the running target (`RSP.read_regs()[9]`) — never
computed from `/proc/<pid>/maps` or any other static guess. Code addresses
(for breakpoints) are simpler: the linked code base (`0xC0DE0000` for this
SDK's flex scatter-loading) maps at a fixed offset to `0x40000000` at
runtime, confirmed by successfully hitting breakpoints placed with that
translation, stable across every container restart observed.
`code_runtime_addr()` in each script performs this translation; no script
hardcodes an actual breakpoint address, only the two base constants —
everything else is resolved from `nm`/`readelf` against whatever `app.elf`
was actually built.

### 5. Stack-local secrets are SP-relative, and simpler than globals

`verify_compare_recovery_phrase_cleanup.py` originally needed this for
`compare_recovery_phrase()`'s `buffer`/`buffer_device` (plain
`uint8_t foo[64]` stack locals, not globals). Confirmed by disassembling
the function (`arm-none-eabi-objdump -d`; see the note below on why the
generic `objdump` cannot be used here): the prologue was
`push {r4,r5,r7,lr}` then a single `sub sp, #N`, and every reference to a
stack local in the function body compiled to `add rX, sp, #offset` — plain
SP-relative, no frame pointer involved. SP itself was stable for the
entire function body, set once by that `sub` and restored only by the
matching `add sp, #N` in the epilogue. Unlike a global's `R9`-relative
address (gotcha 4), no separate "capture the frame base" step was needed:
read `regs[13]` (SP) directly at whichever breakpoint is placed inside the
function, add the fixed offset from the disassembly. This is simpler than
register-relative globals, not harder — it is SP-relative, and stable for
the whole call.

**Update: this no longer describes how Check 2 locates its two buffers.**
Since `compare_recovery_phrase()`'s tail (the two `explicit_bzero()` calls)
was extracted into `compare_recovery_phrase_finish()` — see Check 2 above
— `buffer`/`buffer_device` arrive there as plain pointer *arguments*
rather than that function's own stack-local arrays. Whatever stack frame
that function has holds its own volatile counters and nothing else, so
there is no per-buffer offset to be SP-relative *into* in the first
place. The compiler instead keeps both
pointers in call-preserved registers for the whole function body — an
even simpler case than the SP-relative one above: read the two argument
registers directly at either breakpoint and dereference them, no offset
arithmetic at all. Which register holds which buffer is read back from
each call's `mov r0, rN` in the disassembly (see
`resolve_cleanup_breakpoints()` in `verify_compare_recovery_phrase_cleanup.py`),
not hardcoded — the pattern in this gotcha (SP-relative stack locals) is
kept here as-is because it remains the right approach for a genuine
stack-local secret in a function with its own frame; a future check
should use whichever of the two actually matches what disassembly shows
for its target function, not assume either one.

Disassembly note: the generic `objdump` in the `ledger-app-builder-lite`
image reads section/symbol tables fine (`nm`, `readelf` work unprefixed)
but fails to disassemble ARM code at all
(`can't disassemble for architecture UNKNOWN`) — use
`arm-none-eabi-objdump -d` explicitly for that.

### 6. The GDB stub is not ready when the container is

`docker run` returning does not mean the gdbstub inside is listening. These
scripts originally slept a flat 2 seconds before connecting, which is
enough on some hosts and not on others; when it isn't, the failure lands on
the *first RSP command* rather than at `connect()`:

```
  File "tests/speculos/rsp_client.py", line 78, in insert_bp
    if self.send_command(f"Z0,{addr:x},{kind}") != "OK":
ConnectionResetError: [Errno 104] Connection reset by peer
```

`wait_for_gdb()` (`rsp_client.py`) replaces that sleep in every script: it
polls the port with a plain TCP connect until it is accepted, then allows a
short settle before returning.

**Do not "improve" it into an RSP-level probe.** The obvious objection to a
TCP-only check is that it can succeed too early — and that is real, and
Docker-configuration-dependent. With Docker's userland proxy (check with
`ps aux | grep docker-proxy`) the host port is bound the instant the
container starts, so `connect()` succeeds while the stub is still coming
up; one host measured TCP accepted at +0.20s but the stub only answering at
+0.70s, with `ConnectionResetError` on every RSP attempt in between.
Without the userland proxy (iptables DNAT), the connection is refused until
the stub listens, so there the two coincide. Tightening the probe to send
`?` and require a well-formed reply was tried, and it *broke the run*:
QEMU's gdbstub serves a single GDB session, so a throwaway connection that
actually speaks RSP and then closes reads as a client attaching and
detaching, after which the real connection hangs on its first command. The
trailing settle sleep is the deliberate, working alternative — the probe
must stay silent.

(A related framing trap, if anyone does revisit this: the stub's `+` ack
and its `$T05...#xx` stop-reply may arrive coalesced in one TCP segment or
split across two. Both were observed on the same host on consecutive runs,
so any reply-parsing logic must read until it has a complete packet rather
than trusting a single `recv()`.)

## Adapting to stax/apex, or another scenario

The touch coordinates in each script are flex-only (480×600).
`tests/functional/keypad.py` and `genericlayout.py` have the equivalent
stax/apex positions if this needs extending; those are Ragger-side helpers
and are not reused here directly, since these scripts do not go through
Ragger at all — the GDB/breakpoint machinery has nothing to do with
Ragger's navigator/backend layer, and mixing the two would add coupling
for no benefit in single-scenario scripts like these.

`rsp_client.py` and the `MemorySampler` breakpoint-snapshot pattern are
generic and are reused as-is across checks 1, 3, 4, and 6 (all R9-relative
globals) and adapted for check 2 (SP-relative stack locals, gotcha 5).
They remain applicable to other secret-buffer-lifecycle scenarios, such as
BIP-85 passwords, without needing to re-solve any of the gotchas above.
Check 6 additionally shows the pattern extends cleanly to a two-container
scenario (a "before" process and an independent "after" process) with no new
Speculos/GDB gotcha of its own — only the address re-resolution already
required per-run by gotcha 4.
