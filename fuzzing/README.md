# Fuzzing the entry parsers

## Why this application is worth fuzzing

This application exchanges no APDU. Every byte of untrusted data it handles was
typed on the device screen -- a BIP-39 recovery phrase, or a set of SSKR shares
entered as ByteWords. That makes the input surface small, but it does not make
it simple: an entered share is a CBOR tag, a byte-string header whose form
decides where every later field sits, a serialized shard, and a CRC-32, and the
two numbers the entry path derives from it (how long a share is, how many
shares there are) come out of the typed bytes themselves and are then used as
divisors and offsets.

Short, structured, entirely attacker-chosen input feeding offset arithmetic is
what fuzzing is for. Several memory defects have already been found in these
parsers by reading them; the targets below are the automated version of that
reading.

## The targets

| Target | Entry point | What the fuzzer supplies |
| --- | --- | --- |
| `fuzz_sskr_hex_check` | `bolos_ux_sskr_hex_check()` | the share buffer **and** the share count, as a pair |
| `fuzz_sskr_share_entry` | `bolos_ux_sskr_entry_header_update()`, then `bolos_ux_sskr_hex_check()` | a stream of entered ByteWord values, replayed through the entry state machine |
| `fuzz_sskr_wordlist` | `bolos_ux_sskr_byteword_to_hex()`, `bolos_ux_sskr_share_hex_decode()`, the prefix searches | a typed word, a decoded share, or a keyboard prefix |
| `fuzz_bip39_mnemonic` | `bolos_ux_bip39_mnemonic_decode()`, `bolos_ux_bip39_mnemonic_check()`, the prefix searches | a typed phrase, or a keyboard prefix |

Two conventions matter more than they look:

- **Every buffer a target hands to the code under test is a heap allocation of
  exactly the size that call is allowed to touch.** On the device these buffers
  are fixed-size arrays -- 3664 bytes for the entered shares, 33 for the
  decoded BIP-39 bits -- and a read or write that runs past the meaningful data
  but stays inside the array is invisible to every sanitizer. Sizing the
  allocation to the data is what puts AddressSanitizer's redzone at the
  boundary the code is actually supposed to respect.
- **Parameters that no caller varies are not fuzzed.** `bitslength` in
  `bolos_ux_bip39_mnemonic_decode()` is 32 + 1 at every call site in the
  application, and the decoder writes into that buffer without consulting it;
  fuzzing it would produce an overflow report for a call the application never
  makes, and bury the findings that matter.

## What is deliberately not fuzzed

`sskr_combine_shards()` and `sskr_deserialize_shard()` are not covered here.
They reach `sss_recover_secret()`, which calls `interpolate()`, which is
written against the SDK's `cx_bn_*` big-number API. Off-device that API exists
only as the OpenSSL-backed stand-in under `tests/unit/lib/bolos`, which is why
the cmocka suite compiles OpenSSL from source; requiring the same of a fuzzer
would trade an order of magnitude of build time for coverage of finite-field
arithmetic rather than of a parser. `fuzzing/extra/unreachable.c` defines
`interpolate()` as an `abort()` so that a target which grows a path into that
code stops visibly instead of fuzzing a stub.

The metadata bounds that guard those two functions are covered from the other
side by `tests/unit/tests/sskr_deserialize_shard_bounds.c` and
`tests/unit/tests/sskr_share_len.c`, both built with AddressSanitizer.

## Corpus

`corpus/<target>/` holds seed inputs built from vectors this repository already
carries: the 128-bit Blockchain Commons share set (`tests/unit/tests/`
`sskr_hex_check_guards.c`, `sskr_interop_bc128.c`), the 256-bit set from
`tests/unit/tests/sskr_to_seed_convert.c` re-framed with its CRC-32, and the
BIP-39 phrases from `tests/unit/tests/bip39.c`.

This is not decoration. A share frame only gets past the first few comparisons
if its CBOR tag, its declared length and its CRC-32 all agree; starting from an
empty corpus, a fuzzer spends its whole budget failing the tag comparison and
never sees the code behind it.

## Running it locally

The fuzzers need a Clang with the compiler-rt sanitizer runtimes.
`ledger-app-builder-lite` ships the compiler but not those runtimes, so they
have to be added:

```console
docker run --rm -ti -v "$(realpath .):/app" -w /app/fuzzing \
    ghcr.io/ledgerhq/ledger-app-builder/ledger-app-builder-lite:latest bash

# inside the container
apt-get update && apt-get install -y libclang-rt-21-dev
# the Debian package uses the older runtime layout; Clang looks for the
# per-target one
D=/usr/lib/llvm-21/lib/clang/21/lib
mkdir -p $D/x86_64-pc-linux-gnu
for f in $D/linux/libclang_rt.*-x86_64.a; do
    ln -sf "$f" "$D/x86_64-pc-linux-gnu/$(basename "${f%-x86_64.a}").a"
done

cmake -DBOLOS_SDK=/opt/ledger-secure-sdk -DCMAKE_C_COMPILER=/usr/bin/clang -Bbuild -H.
make -C build
```

Then run one target, seeded with its corpus:

```console
./build/fuzz_sskr_hex_check corpus/fuzz_sskr_hex_check -max_total_time=300
```

A crash is written next to the working directory as `crash-<sha1>`; replaying
it is `./build/fuzz_sskr_hex_check crash-<sha1>`.

### Coverage

What a target actually reaches is a question worth asking of every fuzz target,
and the answer is rarely the whole function. Rebuild with coverage
instrumentation and report over `src/` only:

```console
cmake -DBOLOS_SDK=/opt/ledger-secure-sdk -DCMAKE_C_COMPILER=/usr/bin/clang \
      -DCMAKE_C_FLAGS="-fprofile-instr-generate -fcoverage-mapping" \
      -Bbuild-cov -H.
make -C build-cov

LLVM_PROFILE_FILE=cov.profraw ./build-cov/fuzz_sskr_hex_check corpus/fuzz_sskr_hex_check/*
llvm-profdata-21 merge -sparse cov.profraw -o cov.profdata
llvm-cov-21 report ./build-cov/fuzz_sskr_hex_check -instr-profile=cov.profdata ../src
```

## In CI

`.clusterfuzzlite/` and `.github/workflows/clusterfuzzlite.yml` are the
integration Ledger's `app-boilerplate` uses, adapted to these targets: the
build runs in the OSS-Fuzz base builder with the SDK copied in from
`ledger-app-builder-lite`, and `reusable_clusterfuzz_tests.yml` decides how
long to fuzz from the event (a short run on a pull request, a longer one on
the weekly schedule).

That workflow runs a **matrix of sanitizers**, not just AddressSanitizer:
`address`, `undefined` and `memory` on a pull request, and `address`,
`memory`, a corpus prune and a `coverage` build on push and on schedule. All
four builds are expected to work; a target that only ever gets built under
ASan locally will fail in CI under one of the others.

MemorySanitizer is the one with a catch here, and `extra/host_syscalls.c`
carries the answer: MSan's shadow only follows the writes it can see, the
OSS-Fuzz toolchain does not intercept `explicit_bzero()`, and `memzero()`
expands to `explicit_bzero()` throughout `src/`. Without the shim in that
file, every buffer the application clears reads back as uninitialized and the
`memory` job reports on the seed corpus itself.

The full container flow can be reproduced locally:

```console
mkdir -p fuzzing/corpus_work fuzzing/out
docker build -t app-seed-tool-fuzz --file .clusterfuzzlite/Dockerfile .
docker run --rm --privileged -e FUZZING_LANGUAGE=c \
    -v "$(realpath .)/fuzzing/out:/out" -ti app-seed-tool-fuzz
docker run --rm --privileged -e FUZZING_ENGINE=libfuzzer \
    -e RUN_FUZZER_MODE=interactive \
    -v "$(realpath .)/fuzzing/corpus_work:/tmp/fuzz_corpus" \
    -v "$(realpath .)/fuzzing/out:/out" \
    -ti gcr.io/oss-fuzz-base/base-runner run_fuzzer fuzz_sskr_hex_check
```

> **Note**: the container holds a copy of the sources, so `docker build` has to
> be re-run after each change.
