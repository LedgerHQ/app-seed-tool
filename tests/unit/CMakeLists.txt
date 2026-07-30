cmake_minimum_required(VERSION 3.10)

if(${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(unit_tests
        VERSION 1.9.0
	      DESCRIPTION "Unit tests for app-seed-tool Ledger Application"
        LANGUAGES C)


# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

include(CTest)
ENABLE_TESTING()

# specify C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pedantic -g -O0 --coverage")

set(GCC_COVERAGE_LINK_FLAGS "--coverage -lgcov")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

# Detect OS and install packages
if(EXISTS "/etc/alpine-release")
    # Alpine Linux
    function(install_packages)
        foreach(package ${ARGN})
            execute_process(
                COMMAND bash -c "apk add ${package}"
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
            )
            if(${result} EQUAL 0)
                message(STATUS "Installed ${package} package (apk)")
            else()
                message(FATAL_ERROR "Failed to install package: ${package}\n"
                                    "Command: ${output}\n"
                                    "Error: ${error}")
            endif()
        endforeach()
    endfunction()
elseif(EXISTS "/etc/debian_version")
    # Debian/Ubuntu
    function(install_packages)
        foreach(package ${ARGN})
            execute_process(
                COMMAND bash -c "apt update && apt install -y ${package}"
                RESULT_VARIABLE result
                OUTPUT_VARIABLE output
                ERROR_VARIABLE error
            )
            if(${result} EQUAL 0)
                message(STATUS "Installed ${package} package (apt)")
            else()
                message(FATAL_ERROR "Failed to install package: ${package}\n"
                                    "Command: ${output}\n"
                                    "Error: ${error}")
            endif()
        endforeach()
    endfunction()
else()
    message(FATAL_ERROR "Unsupported OS. Only Alpine and Debian based distributions are supported")
endif()

# Check if linux-headers package is installed
if(NOT EXISTS "/usr/include/linux/version.h")
  install_packages(linux-headers)
else()
  message(STATUS "linux-headers package found")
endif()

include(ExternalProject)
if (PRECOMPILED_DEPENDENCIES_DIR)
    message(STATUS "Using OpenSSL from ${PRECOMPILED_DEPENDENCIES_DIR}")
    set(INSTALL_DIR ${PRECOMPILED_DEPENDENCIES_DIR})
    add_library(openssl STATIC IMPORTED)
else()
    message(STATUS "Building OpenSSL from Git...")
    set(INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/install)

    set(OPENSSL_CFLAGS "${CMAKE_C_FLAGS}")
    get_directory_property(compile_options COMPILE_OPTIONS)
    foreach (opt ${compile_options})
        string(APPEND OPENSSL_CFLAGS " ${opt}")
    endforeach ()
    if (CMAKE_C_COMPILER_TARGET)
        string(APPEND OPENSSL_CFLAGS " --target=${CMAKE_C_COMPILER_TARGET}")
    endif ()
    if (CMAKE_SYSROOT)
        string(APPEND OPENSSL_CFLAGS " -isystem${CMAKE_SYSROOT}/include")
    endif ()
    string(APPEND OPENSSL_CFLAGS " -Wno-unused-parameter -Wno-missing-field-initializers")

    ExternalProject_Add(
        openssl
        GIT_REPOSITORY https://github.com/openssl/openssl.git
        GIT_TAG        openssl-4.0.1
        CONFIGURE_COMMAND ./Configure "CC=${CMAKE_C_COMPILER}" "CFLAGS=${OPENSSL_CFLAGS}" no-afalgeng no-aria no-asan no-asm no-async no-autoalginit no-autoerrinit no-autoload-config no-bf no-buildtest-c++ no-camellia no-capieng no-cast no-chacha no-cmac no-cms no-comp no-crypto-mdebug no-ct no-deprecated no-des no-devcryptoeng no-dgram no-dh no-dsa no-dso no-dtls no-ecdh no-egd no-engine no-err no-external-tests no-filenames no-fuzz-afl no-fuzz-libfuzzer no-gost no-idea no-makedepend no-md2 no-md4 no-mdc2 no-msan no-multiblock no-nextprotoneg no-ocb no-ocsp no-pinshared no-poly1305 no-posix-io no-psk no-rc2 no-rc4 no-rc5 no-rdrand no-rfc3779 no-scrypt no-sctp no-seed no-shared no-siphash no-sm2 no-sm3 no-sm4 no-sock no-srp no-srtp no-sse2 no-ssl no-ssl3-method no-ssl-trace no-stdio no-tests no-threads no-tls no-ts no-ubsan no-ui-console no-unit-test no-whirlpool no-zlib no-zlib-dynamic linux-armv4 --prefix=${INSTALL_DIR}
        BUILD_COMMAND make
        INSTALL_COMMAND make install_sw
        BUILD_IN_SOURCE 1
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE # Ensure proper timestamp handling
    )
endif()

# Fetch cmocka
find_package(cmocka QUIET)
include(FetchContent)
FetchContent_Declare(
    cmocka
    GIT_REPOSITORY https://gitlab.com/cmocka/cmocka.git
    GIT_TAG        cmocka-2.0.2
    GIT_SHALLOW    1
)
set(WITH_STATIC_LIB ON CACHE BOOL "CMocka: Build with a static library" FORCE)
set(WITH_CMOCKERY_SUPPORT OFF CACHE BOOL "CMocka: Install a cmockery header" FORCE)
set(WITH_EXAMPLES OFF CACHE BOOL "CMocka: Build examples" FORCE)
set(UNIT_TESTING OFF CACHE BOOL "CMocka: Build with unit testing" FORCE)
set(PICKY_DEVELOPER OFF CACHE BOOL "CMocka: Build with picky developer flags" FORCE)
FetchContent_MakeAvailable(cmocka)

add_compile_definitions(TEST DEBUG=0 SKIP_FOR_CMOCKA)
add_compile_definitions(HAVE_HASH HAVE_HMAC HAVE_SHA224 HAVE_SHA256 HAVE_SHA512 HAVE_PBKDF2 HAVE_ECC HAVE_CRC HAVE_RNG IO_HID_EP_LENGTH=64 OS_IO_SEPH_BUFFER_SIZE=272)

# Set LEDGER_SECURE_SDK only if it's not already set
if(NOT DEFINED ENV{LEDGER_SECURE_SDK})
  set(ENV{LEDGER_SECURE_SDK} "$ENV{BOLOS_SDK}")
  message(STATUS "LEDGER_SECURE_SDK set to $ENV{LEDGER_SECURE_SDK}")
else()
  message(STATUS "LEDGER_SECURE_SDK already set: $ENV{LEDGER_SECURE_SDK}")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib $ENV{LEDGER_SECURE_SDK}/include $ENV{LEDGER_SECURE_SDK}/lib_cxng/src $ENV{LEDGER_SECURE_SDK}/lib_cxng/include $ENV{LEDGER_SECURE_SDK}/lib_ux/include $ENV{LEDGER_SECURE_SDK}/lib_bagl/include $ENV{LEDGER_SECURE_SDK}/io/include $ENV{LEDGER_SECURE_SDK}/io_legacy/include)

# add src
set(ExternalProject_Get_property(openssl INSTALL_DIR))
set(BOLOS_SOURCES ./lib/bolos/cx_bn.c ./lib/bolos/cx_crc.c ./lib/bolos/cx_mpi.c)
set(LIBCXNG_SOURCES $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_ram.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_hash.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_sha256.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_sha512.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_pbkdf2.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_hmac.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_utils.c $ENV{LEDGER_SECURE_SDK}/lib_cxng/src/cx_sha3.c)
set(TESTLIB_SOURCES ./lib/testutils.c)
add_library(testutils SHARED ${TESTLIB_SOURCES} ${LIBCXNG_SOURCES} ${BOLOS_SOURCES})
target_include_directories(testutils PUBLIC ${INSTALL_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss ${CMAKE_CURRENT_SOURCE_DIR}/../../src/commonsskr)
target_link_directories(testutils PUBLIC ${INSTALL_DIR}/lib)
target_link_libraries(testutils PUBLIC ssl crypto)
# HAVE_SHA3 only affects this library's own SHA3 dispatch-table entry
# (cx_hash.c gates it behind `#ifdef HAVE_SHA3`) and the newly-added
# cx_sha3.c above; it does not change compile definitions for any other
# test executable, which is what actually links against testutils.
target_compile_definitions(testutils PRIVATE HAVE_SHA3)
add_dependencies(testutils openssl)

add_library(sss SHARED ../../src/common/sskr/sss/sss.c ../../src/common/sskr/sss/interpolate.c)
target_include_directories(sss PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)

add_library(sskr SHARED ../../src/common/sskr/sskr.c)
target_include_directories(sskr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr)

# add cmocka tests
add_executable(test_sss tests/sss.c)
target_link_libraries(test_sss PUBLIC cmocka gcov testutils sss)

# Built with AddressSanitizer: sss.c/interpolate.c are compiled directly into
# this target (rather than linked from libsss) so ASan instruments their
# stack frames, not just this file's.
add_executable(test_sss_recover_erase_length ./tests/sss_recover_erase_length.c ../../src/common/sskr/sss/sss.c ../../src/common/sskr/sss/interpolate.c)
target_include_directories(test_sss_recover_erase_length PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sss_recover_erase_length PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sss_recover_erase_length PRIVATE -fsanitize=address)
target_link_libraries(test_sss_recover_erase_length PUBLIC cmocka gcov testutils)

add_executable(test_sss_validate_parameters tests/sss_validate_parameters.c)
target_link_libraries(test_sss_validate_parameters PUBLIC cmocka gcov testutils sss)

add_executable(test_sskr tests/sskr.c)
target_link_libraries(test_sskr PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_generate_combine_guards tests/sskr_generate_combine_guards.c)
target_link_libraries(test_sskr_generate_combine_guards PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_threshold_three_roundtrip tests/sskr_threshold_three_roundtrip.c)
target_link_libraries(test_sskr_threshold_three_roundtrip PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_share_len ./tests/sskr_share_len.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_share_len PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_share_len PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_memzero ./tests/sskr_memzero.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_memzero PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_memzero PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_shard_count tests/sskr_shard_count.c)
target_link_libraries(test_sskr_shard_count PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_generate_split_error tests/sskr_generate_split_error.c)
target_link_libraries(test_sskr_generate_split_error PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_combine_erase_on_failure tests/sskr_combine_erase_on_failure.c)
target_link_libraries(test_sskr_combine_erase_on_failure PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_count_shards_invariants tests/sskr_count_shards_invariants.c)
target_link_libraries(test_sskr_count_shards_invariants PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_generate_erase_on_split_failure tests/sskr_generate_erase_on_split_failure.c)
target_link_libraries(test_sskr_generate_erase_on_split_failure PUBLIC cmocka gcov sskr sss testutils)

# Built with AddressSanitizer: this one is about what the code writes, not
# what it returns, so the sanitizer is the assertion.
add_executable(test_sskr_combine_bounds ./tests/sskr_combine_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_combine_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_combine_bounds PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_combine_bounds PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_combine_bounds PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_combine_error ./tests/sskr_combine_error.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_combine_error PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_combine_error PUBLIC cmocka gcov sskr sss testutils)

# Built with AddressSanitizer: like test_sskr_combine_bounds, this is about
# what the code writes (past the fixed-size `groups[SSKR_MAX_GROUP_COUNT]`
# local array), not what it returns, so the sanitizer is the assertion for
# the two oversized-groups_len cases.
add_executable(test_sskr_generate_bound_groups ./tests/sskr_generate_bound_groups.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_generate_bound_groups PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_generate_bound_groups PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_generate_bound_groups PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_generate_bound_groups PUBLIC cmocka gcov testutils sskr sss)

# Blockchain Commons 128-bit vector, raw Shamir math level (no CBOR/CRC).
add_executable(test_sskr_interop_bc128 tests/sskr_interop_bc128.c)
target_link_libraries(test_sskr_interop_bc128 PUBLIC cmocka gcov testutils sskr sss)
# Unlike test_sskr (tests/sskr.c), this file never references a testutils
# symbol directly (no cx_rng call: combine-only, no share generation), so
# the linker's default --as-needed drops libtestutils.so from the resolved
# set before it gets a chance to satisfy libsss.so's own undefined
# references to cx_bn_*/cx_hmac_sha256 (sss.c's interpolation code).
target_link_options(test_sskr_interop_bc128 PRIVATE -Wl,--no-as-needed)

# Blockchain Commons 128-bit vector, ByteWords/CBOR level.
add_executable(test_sskr_interop_bc128_bytewords ./tests/sskr_interop_bc128_bytewords.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_interop_bc128_bytewords PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_interop_bc128_bytewords PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_combine_invariants ./tests/sskr_combine_invariants.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_combine_invariants PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_combine_invariants PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_bip39 ./tests/bip39.c ../../src/common/bip39/seed_rom_variables.c  ../../src/common/bip39/seed_bip39.c)
target_include_directories(test_bip39 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_bip39 PUBLIC cmocka gcov testutils)

add_executable(test_roundtrip ./tests/roundtrip.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_roundtrip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_roundtrip PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_entry_bounds ./tests/sskr_entry_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c)
target_compile_definitions(test_sskr_entry_bounds PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_entry_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_entry_bounds PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_byteword_sentinel ./tests/sskr_byteword_sentinel.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c)
target_compile_definitions(test_sskr_byteword_sentinel PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_byteword_sentinel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_byteword_sentinel PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_cbor_additional_info ./tests/sskr_cbor_additional_info.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c)
target_compile_definitions(test_sskr_cbor_additional_info PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_cbor_additional_info PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_cbor_additional_info PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_words ./tests/words.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_words PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_words PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_base64 tests/base64.c ../../src/common/bip85/seed_rom_variables.c ../../src/common/bip85/base64.c)
target_link_libraries(test_base64 PUBLIC cmocka gcov)

# Built with UndefinedBehaviorSanitizer, in abort mode: base85_encode_64bytes()
# shifts a promoted uint8_t left by 24, which is undefined once the byte has
# its top bit set. Nothing about the result is visibly wrong, so a value
# assertion cannot see it -- the sanitizer is the assertion.
add_executable(test_base85 tests/base85.c ../../src/common/bip85/seed_rom_variables.c ../../src/common/bip85/base85.c)
target_compile_options(test_base85 PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)
target_link_options(test_base85 PRIVATE -fsanitize=undefined)
target_link_libraries(test_base85 PUBLIC cmocka gcov)

# bip85_dice_bits_per_roll() is defined outside the HAVE_NBGL guard in
# seed_bip85.c precisely so it can be linked here without the NBGL headers
# the rest of that file needs. Since HAVE_HMAC is defined globally above
# (unlike the per-target HAVE_SHA3 below), compiling seed_bip85.c as a whole
# now always pulls in bip85_entropy_from_key()'s reference to
# cx_hmac_sha512_init_no_throw()/cx_hmac_no_throw() too, even though this
# target itself never calls it -- testutils is where those symbols actually
# live.
add_executable(test_bip85_dice_bits_per_roll tests/bip85_dice_bits_per_roll.c ../../src/common/bip85/seed_bip85.c)
target_include_directories(test_bip85_dice_bits_per_roll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_bip85_dice_bits_per_roll PUBLIC cmocka gcov testutils)

# bip85_dice_roll() (also outside the HAVE_NBGL guard) calls
# cx_shake256_hash() -- pure software, no BOLOS syscall, unlike
# bolos_ux_bip85_entropy() which the rest of seed_bip85.c depends on and
# which cannot be tested on host. cx_sha3.c and cx_hash_iovec.c are
# compiled directly into this target rather than added to the shared
# `testutils` library, the same way sss.c/interpolate.c are for
# test_sss_recover_erase_length; HAVE_SHA3 is defined for this target only,
# matching the real app build (Makefile.conf.cx enables it unconditionally)
# without turning it on for any other unit test.
add_executable(test_bip85_dice_roll tests/bip85_dice_roll.c ../../src/common/bip85/seed_bip85.c $ENV{LEDGER_SECURE_SDK}/src/cx_hash_iovec.c)
# cx_shake256_hash_iovec() (which cx_shake256_hash() wraps) is behind
# `#ifdef HAVE_SHA3` in cx_hash_iovec.c, same as the SHA3 dispatch-table
# entry testutils now also compiles in (see `target_compile_definitions
# (testutils ...)` above) -- both need it for this target to link.
target_compile_definitions(test_bip85_dice_roll PRIVATE HAVE_SHA3)
# cx_hash_iovec.c does `#include "lib_cxng/src/cx_ram.h"`, resolved relative
# to the SDK root rather than to lib_cxng/src itself.
target_include_directories(test_bip85_dice_roll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src $ENV{LEDGER_SECURE_SDK})
target_link_libraries(test_bip85_dice_roll PUBLIC cmocka gcov testutils)

# bolos_ux_bip85_drng_with_seed() (also outside the HAVE_NBGL guard, moved
# there alongside bip85_dice_roll() since it needs the same HAVE_SHA3
# cx_shake256_hash()) takes its seed as a raw byte buffer rather than
# deriving it via bolos_ux_bip85_entropy() -- no BOLOS syscall, same
# reasoning and same compilation pattern as test_bip85_dice_roll above.
add_executable(test_bip85_drng_with_seed tests/bip85_drng_with_seed.c ../../src/common/bip85/seed_bip85.c $ENV{LEDGER_SECURE_SDK}/src/cx_hash_iovec.c)
target_compile_definitions(test_bip85_drng_with_seed PRIVATE HAVE_SHA3)
target_include_directories(test_bip85_drng_with_seed PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src $ENV{LEDGER_SECURE_SDK})
target_link_libraries(test_bip85_drng_with_seed PUBLIC cmocka gcov testutils)

# bip85_entropy_from_key() (also outside the HAVE_NBGL guard) is the
# HMAC-SHA512("bip-entropy-from-k", key) half of bolos_ux_bip85_entropy();
# the BIP32 derivation that produces `key` is a BOLOS syscall
# (os_derive_bip32_no_throw()) with no host equivalent and is not exercised
# here. Chained with bolos_ux_bip39_mnemonic_encode() (seed_bip39.c, also
# outside HAVE_NBGL) and checked word-for-word against the official BIP-85
# test vectors. HAVE_HMAC and HAVE_SHA512 are already defined globally
# above (needed for BIP39's own PBKDF2-HMAC-SHA512 seed derivation), so
# nothing extra is required here, unlike HAVE_SHA3 for test_bip85_dice_roll
# above.
add_executable(test_bip85_bip39_entropy tests/bip85_bip39_entropy.c ../../src/common/bip85/seed_bip85.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c)
target_include_directories(test_bip85_bip39_entropy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_bip85_bip39_entropy PUBLIC cmocka gcov testutils)

# Independent BIP-85 HEX-application vectors (bipsea, determinism); same
# HAVE_HMAC/HAVE_SHA512-only dependency as test_bip85_bip39_entropy above,
# nothing extra required.
add_executable(test_bip85_hex_vector tests/bip85_hex_vector.c ../../src/common/bip85/seed_bip85.c)
target_include_directories(test_bip85_hex_vector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_bip85_hex_vector PUBLIC cmocka gcov testutils)

# compare_recovery_phrase_finish() is the tail of compare_recovery_phrase()
# (common_seed.c) that runs once os_derive_bip32_no_throw() -- a BOLOS
# syscall, not testable on host -- has returned; splitting on that call's
# *result* rather than the call itself makes the comparison and buffer
# erasure logic testable, including the derivation-failure path, never
# exercised anywhere in this suite before. First time common_seed.c enters
# the test suite: compiling it whole still compiles the untouched
# compare_recovery_phrase() body too, whose own unconditional (not gated
# by HAVE_BAGL/HAVE_NBGL) call to os_derive_bip32_no_throw() would
# otherwise need the real SYSCALL os_perso_derive_node_with_seed_key() (and
# the BEGIN_TRY/CATCH machinery wrapping it) to resolve at link time --
# none of it defined for host builds, and none of it exercised by this
# target, which never calls compare_recovery_phrase() itself. Rather than
# add link-only stubs for that machinery to testutils.c (shared by every
# target in this suite, for something only this one needs), this target
# compiles with -ffunction-sections/-fdata-sections and links with
# --gc-sections so the linker drops compare_recovery_phrase() and
# everything it alone pulls in before ever needing to resolve those
# symbols -- verified with nm on the built test binary, not assumed: only
# compare_recovery_phrase_finish() and its own gcov counters remain: the
# defined text symbol for compare_recovery_phrase() itself is gone.
add_executable(test_compare_recovery_phrase_finish ./tests/compare_recovery_phrase_finish.c ../../src/common/common_seed.c)
target_include_directories(test_compare_recovery_phrase_finish PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
# common_seed.c's untouched compare_recovery_phrase() body uses
# CX_CURVE_256K1 (ox_ec.h). The real app build gets it from Makefile.conf.cx
# (HAVE_ECC_WEIERSTRASS + HAVE_SECP256K1_CURVE, both gating the enum value
# in ox_ec.h) via the app's own Makefile -- this test target reaches
# ox_ec.h through <cx.h> the same way, but does not otherwise go through
# that Makefile, so the two curve flags are forced here too, matching the
# SCREEN_SIZE_WALLET/HAVE_SHA3 precedent above. Needed regardless of the
# --gc-sections trick above: the file still has to compile before the
# linker gets a chance to drop anything.
target_compile_options(test_compare_recovery_phrase_finish PRIVATE -include cx.h -ffunction-sections -fdata-sections)
target_compile_definitions(test_compare_recovery_phrase_finish PRIVATE HAVE_ECC_WEIERSTRASS HAVE_SECP256K1_CURVE)
target_link_options(test_compare_recovery_phrase_finish PRIVATE -Wl,--gc-sections)
target_link_libraries(test_compare_recovery_phrase_finish PUBLIC cmocka gcov testutils)
# Expected, harmless compiler warning from this target only: with neither
# HAVE_BAGL nor HAVE_NBGL defined (a combination the real app build never
# hits -- one of the two is always set), the two `goto cleanup;` sites
# inside compare_recovery_phrase()'s UI-specific block are preprocessed
# out, leaving its `cleanup:` label with no remaining jump to it:
#   common_seed.c:126:1: warning: label 'cleanup' defined but not used
#     [-Wunused-label]

# bip85_finalize_pwd() (also outside the HAVE_NBGL guard) is the
# memcpy-and-terminate tail shared by bolos_ux_bip85_pwd_base64()/
# bolos_ux_bip85_pwd_base85(); no crypto dependency, so no extra compile
# definitions are needed beyond the globally-defined HAVE_HMAC (already
# required to link bip85_entropy_from_key(), also in this file, same as
# test_bip85_dice_bits_per_roll above).
add_executable(test_bip85_finalize_pwd tests/bip85_finalize_pwd.c ../../src/common/bip85/seed_bip85.c)
target_include_directories(test_bip85_finalize_pwd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_bip85_finalize_pwd PUBLIC cmocka gcov testutils)

foreach(target test_sss test_sss_recover_erase_length test_sss_validate_parameters test_sskr test_sskr_generate_combine_guards test_sskr_threshold_three_roundtrip test_sskr_share_len test_sskr_memzero test_sskr_shard_count test_sskr_generate_split_error test_sskr_generate_bound_groups test_sskr_combine_erase_on_failure test_sskr_count_shards_invariants test_sskr_generate_erase_on_split_failure test_sskr_combine_bounds test_sskr_combine_error test_sskr_combine_invariants test_sskr_interop_bc128 test_sskr_interop_bc128_bytewords test_sskr_entry_bounds test_sskr_byteword_sentinel test_sskr_cbor_additional_info test_bip39 test_roundtrip test_words test_base64 test_base85 test_bip85_dice_bits_per_roll test_bip85_dice_roll test_bip85_bip39_entropy test_bip85_hex_vector test_bip85_finalize_pwd test_bip85_drng_with_seed test_compare_recovery_phrase_finish)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
