#!/bin/bash -eu

# build fuzzers

pushd fuzzing
cmake -DBOLOS_SDK=../BOLOS_SDK -Bbuild -H.
make -C build
for target in fuzz_sskr_hex_check fuzz_sskr_share_entry fuzz_sskr_wordlist fuzz_bip39_mnemonic; do
    mv "./build/${target}" "${OUT}"
    # Seed corpus: ClusterFuzzLite picks up <target>_seed_corpus.zip next to the
    # binary. Without it every run starts from a single empty input and spends
    # its budget rediscovering the CBOR tag and the CRC-32.
    if [ -d "corpus/${target}" ]; then
        zip -j "${OUT}/${target}_seed_corpus.zip" "corpus/${target}"/*
    fi
done
popd
