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
# -Wmissing-prototypes is here for a defect this suite could not see: a
# function whose definition contradicts the declaration callers use, in a
# file that never includes its own public header. Six such divergences lived
# in seed_bip39.c and seed_sskr.c for as long as those files did not include
# common_bip39.h / common_sskr.h. They do now, which makes any future
# divergence a compilation error; this flag covers the other half of the
# problem, a function with external linkage and no prototype at all, where
# there is nothing for the compiler to compare a definition against.
#
# src/ is at zero, and the block below keeps it there. Measured over a full
# build of this directory and deduplicated by (file, function), the flag
# reported 23 two commits ago and 21 as of this one; every one of the
# remaining 21 is in tests/, and none in src/.
#
# Getting there was not a matter of making anything static -- the flag asks
# for a prototype, not for internal linkage, and every one of these functions
# has a reason to keep external linkage. Each declaration went into the header
# its own directory already had, which the defining file already included:
#
#   base64_encode_64bytes, base85_encode_64bytes  bip85/bip85_internal.h
#   bolos_ux_bip39_..._hash_length128             bip39/common_bip39.h
#   compare_recovery_phrase_finish                common.h
#   sss_create_digest                             sskr/sss/sss.h
#   bip39_mnemonic_shrink, sskr_shares_shrink     nbgl/*.h
#
# Five hand-written declarations went away with them -- three in tests, and
# two in src/common/bip85/seed_bip85.c, which declared the two encoders with
# `extern` for its own use. Those two were the point: a caller in the
# application, a definition in another file, and nothing comparing them.
#
# tests/ is deliberately not held to this. Several test files define stubs
# with external linkage that stand in for functions the target does not link
# (bip39_mnemonic_get(), compare_recovery_phrase() and friends), and
# testutils.c defines os_secure_memcmp() the same way. Giving those prototypes
# would mean a header per set of stubs for no gain: nothing calls them across
# translation units.
#
# -Wstrict-prototypes is already clean everywhere and is here as a ratchet.
#
# Not added to the application build: measured there, the two flags report
# 120 warnings on stax and 151 on nanos, roughly half of them inside the SDK
# itself (39 and 49 in syscalls.c alone, plus lib_standard_app), which this
# repository cannot act on. ENABLE_SDK_WERROR defaults to 0 and is not
# overridden here, so they would not break that build -- they would just bury
# the app's own diagnostics.
# Held in a variable because two build types need the same list, and setting
# one of the two says nothing about the other. The -O2 and -Os entries of
# .github/workflows/unit-tests-matrix.yml build Release precisely so that the
# -O0 on the line below cannot win over their -O level; that also puts them
# past the only line that carried the warning flags. They are the only
# optimised builds in the repository, so they are the only configurations
# where -Wmaybe-uninitialized, an aliasing diagnostic or a dead-store
# observation can fire at all -- which made the two entries most likely to
# find something the only two that reported nothing.
#
# Deliberately not folded into CMAKE_C_FLAGS, which would cover both build
# types in a single line: OPENSSL_CFLAGS further down is derived from
# CMAKE_C_FLAGS, and the OpenSSL build is a third-party dependency this
# repository does not diagnose and cannot act on. The per-build-type variables
# do not reach it.
set(APP_WARNING_FLAGS "-Wall -Wmissing-prototypes -Wstrict-prototypes -pedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${APP_WARNING_FLAGS} -g -O0 --coverage")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${APP_WARNING_FLAGS}")

# Hold src/ at zero, rather than trusting the comment above to be re-read.
# Promoting -Wmissing-prototypes to an error globally is not an option: the
# tests deliberately define stubs with external linkage, and the SDK sources
# pulled into some targets add their own. Scoping the error to the application
# sources is what turns "the point of the flag is that a *new* one shows up"
# into something the build enforces.
#
# The glob is read at build time, not just at configure time, so a new file
# under src/ is covered without anyone remembering this block exists -- the
# same reason the ctest registration at the end of this file reads the targets
# rather than a list kept by hand.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../src/*.c)
set_source_files_properties(${APP_SOURCES} DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            PROPERTIES COMPILE_OPTIONS "-Werror=missing-prototypes")

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

# The twin of the above, on the splitting side. No AddressSanitizer here: the
# assertion is a canary field placed immediately after the caller's buffer, and
# ASan would add nothing -- memzero() resolves to explicit_bzero(), which this
# toolchain's ASan does not intercept (the reason the recovery-side test needs
# a canary too). Linking libsss is therefore enough.
add_executable(test_sss_split_erase_on_failure tests/sss_split_erase_on_failure.c)
target_link_libraries(test_sss_split_erase_on_failure PUBLIC cmocka gcov testutils sss)

add_executable(test_sss_validate_parameters tests/sss_validate_parameters.c)
target_link_libraries(test_sss_validate_parameters PUBLIC cmocka gcov testutils sss)

add_executable(test_sskr tests/sskr.c)
target_link_libraries(test_sskr PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_to_seed_convert ./tests/sskr_to_seed_convert.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_to_seed_convert PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_to_seed_convert PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_generate_combine_guards tests/sskr_generate_combine_guards.c)
target_link_libraries(test_sskr_generate_combine_guards PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_threshold_three_roundtrip tests/sskr_threshold_three_roundtrip.c)
target_link_libraries(test_sskr_threshold_three_roundtrip PUBLIC cmocka gcov testutils sskr sss)

# Built with AddressSanitizer, like test_sskr_combine_bounds and
# test_sskr_generate_bound_groups below. The bound this covers is what keeps
# sskr_deserialize_shard() from reading an entered length's worth of bytes out
# of a share buffer that is not that long, and the return value cannot tell
# the bounded build from the unbounded one -- so the sanitizer is the
# assertion. sskr.c lives in the shared `sskr` library and is not instrumented
# itself, but the copy goes through the intercepted memcpy() and the buffer it
# reads from is a stack array in this instrumented translation unit, so the
# redzone still catches it.
add_executable(test_sskr_share_len ./tests/sskr_share_len.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_share_len PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_compile_options(test_sskr_share_len PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_share_len PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_share_len PUBLIC cmocka gcov sskr sss testutils)

# The guards bolos_ux_sskr_hex_check() holds on entered shares. Built with
# AddressSanitizer, like test_sskr_combine_bounds and
# test_sskr_generate_bound_groups below, for the offsets this function derives
# by dividing a caller-supplied buffer length by a caller-supplied share count.
add_executable(test_sskr_hex_check_guards ./tests/sskr_hex_check_guards.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_hex_check_guards PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_compile_options(test_sskr_hex_check_guards PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_hex_check_guards PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_hex_check_guards PUBLIC cmocka gcov sskr sss testutils)

# The other half of the bound test_sskr_share_len holds: what happens in
# sskr_deserialize_shard() when a length that far out reaches it anyway.
# Built with AddressSanitizer, and with sskr.c compiled into the test itself
# rather than linked from libsskr -- that function is static, and reached
# through sskr_combine_shards() the overrun it is about lands inside that
# caller's own shards[] array, which is intra-object and something ASan does
# not report. The file says the same at more length. libsskr is deliberately
# absent from target_link_libraries below: linking it too would duplicate
# every symbol in the translation unit.
add_executable(test_sskr_deserialize_shard_bounds ./tests/sskr_deserialize_shard_bounds.c)
target_include_directories(test_sskr_deserialize_shard_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_deserialize_shard_bounds PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_deserialize_shard_bounds PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_deserialize_shard_bounds PUBLIC cmocka gcov testutils sss)

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

add_executable(test_sskr_generate_error_contract tests/sskr_generate_error_contract.c)
target_link_libraries(test_sskr_generate_error_contract PUBLIC cmocka gcov sskr sss testutils)

# Built with AddressSanitizer: without the bound under test, the overflow of
# group_shares happens inside sss_split_secret()/interpolate() through plain
# loads and stores, which ASan does instrument -- so it is the assertion for
# the case that matters. sskr.c, sss.c and interpolate.c are compiled directly
# into this target rather than linked from libsskr/libsss so their stack
# frames are instrumented too, the same way test_sskr_combine_bounds does it.
add_executable(test_sskr_bound_group_count ./tests/sskr_bound_group_count.c ../../src/common/sskr/sskr.c ../../src/common/sskr/sss/sss.c ../../src/common/sskr/sss/interpolate.c)
target_include_directories(test_sskr_bound_group_count PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_bound_group_count PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_bound_group_count PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_bound_group_count PUBLIC cmocka gcov testutils)

# Built with AddressSanitizer: this one is about what the code writes, not
# what it returns, so the sanitizer is the assertion.
add_executable(test_sskr_combine_bounds ./tests/sskr_combine_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_combine_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_combine_bounds PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_combine_bounds PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_combine_bounds PUBLIC cmocka gcov testutils sskr sss)

# Built with AddressSanitizer for the same reason as
# test_sskr_generate_bound_groups below, with the same caveat spelled out
# in the test file's header: the overrun this one covers is a memzero()
# (explicit_bzero()), which ASan does not intercept, so the sanitizer is
# not the assertion here -- surviving the call is. ASan is on to catch any
# neighbouring overrun, and seed_sskr.c is compiled directly into the
# target so its frames are instrumented either way.
add_executable(test_sskr_invalid_group_descriptor ./tests/sskr_invalid_group_descriptor.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_invalid_group_descriptor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_invalid_group_descriptor PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_invalid_group_descriptor PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_invalid_group_descriptor PUBLIC cmocka gcov testutils sskr sss)

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

# The seed_len bounds of the same entry point. Same source list and same
# sanitizer setup as test_sskr_generate_bound_groups above: seed_sskr.c is
# compiled in rather than linked so its frames are instrumented, since these
# lengths are what the shard buffers are sized from.
add_executable(test_sskr_generate_seed_len_bounds ./tests/sskr_generate_seed_len_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_generate_seed_len_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_generate_seed_len_bounds PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_generate_seed_len_bounds PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_generate_seed_len_bounds PUBLIC cmocka gcov testutils sskr sss)

# The group-descriptor expansion both of the above and bolos_ux_sskr_size_get()
# share, on group counts above the one SSKR_MAX_GROUP_COUNT allows their own
# fixed-size arrays -- which is why the function under test takes the
# destination's capacity from its caller, and why this target can reach a case
# neither entry point could. Same source list and same sanitizer setup as
# test_sskr_generate_bound_groups above, for the same reason: the capacity bound
# is about a write, so seed_sskr.c is compiled in rather than linked so its
# frames are instrumented.
add_executable(test_sskr_group_descriptor ./tests/sskr_group_descriptor.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_group_descriptor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_group_descriptor PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_group_descriptor PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_group_descriptor PUBLIC cmocka gcov testutils sskr sss)

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

# The duplicate member index guard, asserted on the error code itself rather
# than on the 0 the wrapper reports for every failure alike.
add_executable(test_sskr_duplicate_member_index ./tests/sskr_duplicate_member_index.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_duplicate_member_index PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_duplicate_member_index PUBLIC cmocka gcov sskr sss testutils)

# The ByteWords table itself, against BCR-2020-012. Data, not behaviour: the
# only source besides the test is seed_rom_variables.c, and nothing that reads
# the table is linked in, so a failure here can only be the table.
add_executable(test_sskr_bytewords_table ./tests/sskr_bytewords_table.c ../../src/common/sskr/seed_rom_variables.c)
target_include_directories(test_sskr_bytewords_table PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_bytewords_table PUBLIC cmocka gcov testutils)

# ---------------------------------------------------------------------------
# The same Shamir tests, replayed against the TARGET_NANOS build of the GF(2^8)
# arithmetic.
#
# interpolate.c carries two different implementations of the field
# multiplication every share is built from. Under
# `#if defined(TARGET_NANOS) && !defined API_LEVEL` it defines its own
# cx_bn_gf2_n_mul() -- a shift-and-XOR loop written by hand on top of
# elementary BN primitives; everywhere else the SDK's cx_bn_gf2_n_mul() is
# called instead. Only the second one was ever built here, so the one that
# ships on Nano S ran nowhere but on the device: had the two disagreed on a
# single input, shares produced on a Nano S would not have recombined
# anywhere else, silently.
#
# Nothing in src/ changes for this. The nine BN primitives the local
# implementation uses (cx_bn_alloc, cx_bn_cnt_bits, cx_bn_copy, cx_bn_destroy,
# cx_bn_nbytes, cx_bn_set_u32, cx_bn_shl, cx_bn_tst_bit, cx_bn_xor) are all
# already provided by lib/bolos/cx_bn.c, and API_LEVEL is defined nowhere in
# this repository, so defining TARGET_NANOS is all it takes to select the
# variant on host. The definition compiled into each target below takes
# precedence over the one libtestutils.so exports, which is what puts the
# local implementation on the call path.
#
# Two consequences drive the shape of these targets:
#
#  - the sources have to be compiled into the target rather than linked from
#    the shared `sss`/`sskr` libraries, which are built without the flag;
#  - SSS_MAX_SHARE_COUNT is 10 under TARGET_NANOS and 16 otherwise
#    (sss-constants.h), so the flag has to reach *every* translation unit of a
#    target, test file included. A target mixing the two would size arrays
#    differently on the two sides of a call -- a defect built by the test
#    setup rather than found by it. target_compile_definitions() below applies
#    to all of a target's sources, which is exactly what is wanted here.
#
# Only tests that actually interpolate are replayed; the four below cover
# splitting and recovery, 128- and 256-bit secrets, thresholds of 2 and 3, and
# use at most 5 shares, so none of them needs more than the 10 shares this
# configuration allows. Tests that only exercise input guards would run the
# same code either way and are left out.
set(SSS_NANOS_SOURCES ../../src/common/sskr/sss/sss.c ../../src/common/sskr/sss/interpolate.c)
set(SSKR_NANOS_SOURCES ../../src/common/sskr/sskr.c ${SSS_NANOS_SOURCES})
set(SSS_NANOS_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
set(SSKR_NANOS_INCLUDES ${SSS_NANOS_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr)

# Blockchain Commons vectors at raw Shamir math level, the ideal oracle here:
# they compare against values published outside this repository, so they hold
# the variant against the specification rather than against the other
# implementation.
add_executable(test_sss_nanos tests/sss.c ${SSS_NANOS_SOURCES})
target_include_directories(test_sss_nanos PUBLIC ${SSS_NANOS_INCLUDES})
target_compile_definitions(test_sss_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sss_nanos PUBLIC cmocka gcov testutils)

# Unlike its counterpart above, this one needs no -Wl,--no-as-needed: sskr.c
# and sss.c are compiled into the target, so their references to cx_bn_*/
# cx_hmac_sha256 are the executable's own undefined symbols and libtestutils.so
# is kept for them.
add_executable(test_sskr_interop_bc128_nanos tests/sskr_interop_bc128.c ${SSKR_NANOS_SOURCES})
target_include_directories(test_sskr_interop_bc128_nanos PUBLIC ${SSKR_NANOS_INCLUDES})
target_compile_definitions(test_sskr_interop_bc128_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sskr_interop_bc128_nanos PUBLIC cmocka gcov testutils)

# Generation and recombination of the 256-bit vector, 2-of-3.
add_executable(test_sskr_nanos tests/sskr.c ${SSKR_NANOS_SOURCES})
target_include_directories(test_sskr_nanos PUBLIC ${SSKR_NANOS_INCLUDES})
target_compile_definitions(test_sskr_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sskr_nanos PUBLIC cmocka gcov testutils)

# 3-of-5: the only case that makes the interpolation loop run over more than
# two shares, and the only one that exercises a threshold above 2.
add_executable(test_sskr_threshold_three_roundtrip_nanos tests/sskr_threshold_three_roundtrip.c ${SSKR_NANOS_SOURCES})
target_include_directories(test_sskr_threshold_three_roundtrip_nanos PUBLIC ${SSKR_NANOS_INCLUDES})
target_compile_definitions(test_sskr_threshold_three_roundtrip_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sskr_threshold_three_roundtrip_nanos PUBLIC cmocka gcov testutils)

# The two implementations of the field multiplication itself, each held to the
# same reference over all 65536 pairs the field has. The four targets above
# reach the Nano S variant only through the Blockchain Commons vectors, which
# exercise a few hundred products; a disagreement on any other pair would make
# shares generated on a Nano S unrecombinable elsewhere and show up nowhere.
#
# The pair below is the whole point: one file, two targets. test_gf2_8_mul
# links the harness's OpenSSL-backed cx_bn_gf2_n_mul() and needs no source of
# its own; test_gf2_8_mul_nanos compiles interpolate.c in with TARGET_NANOS so
# its hand-written definition takes precedence over the one libtestutils.so
# exports, the same mechanism the four targets above rely on. sss.c is not
# needed by either: nothing here interpolates, it multiplies.
add_executable(test_gf2_8_mul tests/gf2_8_mul.c)
target_link_libraries(test_gf2_8_mul PUBLIC cmocka gcov testutils)

add_executable(test_gf2_8_mul_nanos tests/gf2_8_mul.c ../../src/common/sskr/sss/interpolate.c)
target_include_directories(test_gf2_8_mul_nanos PUBLIC ${SSS_NANOS_INCLUDES})
target_compile_definitions(test_gf2_8_mul_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_gf2_8_mul_nanos PUBLIC cmocka gcov testutils)

# The primitive underneath all of the above, on the one input the rest of this
# directory never asks it about: zero.
#
# cx_mpi_cnt_bits() (lib/bolos/cx_mpi.c) is a host stub, and for a zero bignum
# BN_bn2bin() writes nothing into its 2048-byte scratch buffer and returns 0 --
# so the walk that follows read the buffer uninitialised and, the byte counter
# being unsigned, ran off its end. Only the TARGET_NANOS build of
# cx_bn_gf2_n_mul() ever reaches it, which is why the four targets above and
# nothing else were affected.
#
# Both targets are built with AddressSanitizer: the value the stub returns is
# only half of what is being held here, the other half being that producing it
# reads nothing it should not, and that half has no observable form without a
# sanitiser. Which is precisely how this survived every unsanitised run.
#
# The pair is the same one-file-two-targets shape as test_gf2_8_mul above, and
# for the same reason: the multiplication by zero has to hold for both
# implementations of cx_bn_gf2_n_mul(), not just the one that goes near the
# stub.
#
#  - test_cx_bn_cnt_bits compiles lib/bolos/ into the target, so the stub's own
#    stack frame is instrumented in every configuration this suite is built in,
#    rather than only in the sanitised entry of the matrix workflow where the
#    flags also reach libtestutils.so.
#  - test_cx_bn_cnt_bits_nanos cannot do the same: cx_bn.c and the TARGET_NANOS
#    part of interpolate.c both define cx_bn_gf2_n_mul(), and having the second
#    one on the call path is the whole point of the target. It links lib/bolos/
#    from libtestutils.so like the four targets above, and what it pins is the
#    returned value -- zero times anything is zero -- which a wrong bit count
#    breaks on its own, with no sanitiser needed.
add_executable(test_cx_bn_cnt_bits tests/cx_bn_cnt_bits.c ${BOLOS_SOURCES})
target_compile_options(test_cx_bn_cnt_bits PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_cx_bn_cnt_bits PRIVATE -fsanitize=address)
target_link_libraries(test_cx_bn_cnt_bits PUBLIC cmocka gcov testutils)

add_executable(test_cx_bn_cnt_bits_nanos tests/cx_bn_cnt_bits.c ../../src/common/sskr/sss/interpolate.c)
target_include_directories(test_cx_bn_cnt_bits_nanos PUBLIC ${SSS_NANOS_INCLUDES})
target_compile_definitions(test_cx_bn_cnt_bits_nanos PRIVATE TARGET_NANOS)
target_compile_options(test_cx_bn_cnt_bits_nanos PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_cx_bn_cnt_bits_nanos PRIVATE -fsanitize=address)
target_link_libraries(test_cx_bn_cnt_bits_nanos PUBLIC cmocka gcov testutils)
# ---------------------------------------------------------------------------

# Built with AddressSanitizer. Both buffer guards this file covers --
# the "word too long for current_word[]" bail-out in decode() and the
# "output buffer too small" bail-out in encode() -- are guards whose whole
# purpose is to prevent a write, so a test that only inspects the return
# value cannot tell a working guard from a missing one: encode() has a
# second, later check on `offset` that still returns 0 after an overrun has
# already happened. seed_bip39.c is compiled directly into the target, so
# its frames are instrumented and the sanitizer, not the return value, is
# what observes the write.
add_executable(test_bip39 ./tests/bip39.c ../../src/common/bip39/seed_rom_variables.c  ../../src/common/bip39/seed_bip39.c)
target_include_directories(test_bip39 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_compile_options(test_bip39 PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_bip39 PRIVATE -fsanitize=address)
target_link_libraries(test_bip39 PUBLIC cmocka gcov testutils)

add_executable(test_roundtrip ./tests/roundtrip.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_roundtrip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_roundtrip PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_sskr_entry_bounds ./tests/sskr_entry_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_sskr_entry_bounds PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_entry_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_entry_bounds PUBLIC cmocka gcov sskr sss testutils)

# src/nbgl/bip39_mnemonic.c is the direct counterpart of src/nbgl/sskr_shares.c
# just above, and was not compiled into any test target until this one. Same
# SCREEN_SIZE_WALLET requirement: the whole file sits behind that guard.
# sskr_shares.c comes along because bip39_mnemonic_from_sskr_shares() calls into
# it, and the two files reference each other's accessors; the only symbol left
# unresolved after that is compare_recovery_phrase(), which the test file stubs
# out rather than dragging common_seed.c and the BIP32 syscall in behind it.
add_executable(test_bip39_entry_bounds ./tests/bip39_entry_bounds.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/bip39_mnemonic.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_bip39_entry_bounds PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_bip39_entry_bounds PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_bip39_entry_bounds PUBLIC cmocka gcov sskr sss testutils)

# Same set of sources as test_sskr_entry_bounds above -- the entry buffer and
# the two conversion helpers it needs to link -- for the other half of that
# buffer's life: sskr_shares_word_remove() and the reach of the erasure it
# triggers. Kept in its own target rather than appended to the bounds test so
# that a failure names which of the two it is.
add_executable(test_sskr_entry_lifecycle ./tests/sskr_entry_lifecycle.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_sskr_entry_lifecycle PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_entry_lifecycle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_entry_lifecycle PUBLIC cmocka gcov sskr sss testutils)

# Same sources as test_sskr_entry_bounds above, for the one part of the entry
# state machine that only shows up when a secret takes more than one share:
# sskr_shares_complete_check() reopening entry for the next share.
add_executable(test_sskr_multi_share_entry ./tests/sskr_multi_share_entry.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_sskr_multi_share_entry PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_multi_share_entry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_multi_share_entry PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_byteword_sentinel ./tests/sskr_byteword_sentinel.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_sskr_byteword_sentinel PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_byteword_sentinel PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_byteword_sentinel PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_cbor_additional_info ./tests/sskr_cbor_additional_info.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c ../../src/nbgl/sskr_shares.c ../../src/common/sskr/sskr_entry_header.c)
target_compile_definitions(test_sskr_cbor_additional_info PRIVATE SCREEN_SIZE_WALLET)
target_include_directories(test_sskr_cbor_additional_info PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_sskr_cbor_additional_info PUBLIC cmocka gcov sskr sss testutils)

# The header arithmetic on its own. It holds no state and reaches into no UI
# storage, so unlike every target above it needs nothing but its own source --
# and unlike every target above, what it covers is the code all three entry
# paths run, not the one path a host build can link.
add_executable(test_sskr_entry_header ./tests/sskr_entry_header.c ../../src/common/sskr/sskr_entry_header.c)
target_include_directories(test_sskr_entry_header PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_entry_header PUBLIC cmocka gcov testutils)

add_executable(test_words ./tests/words.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_words PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_words PUBLIC cmocka gcov testutils sskr sss)

add_executable(test_base64 tests/base64.c ../../src/common/bip85/seed_rom_variables.c ../../src/common/bip85/base64.c)
target_include_directories(test_base64 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_base64 PUBLIC cmocka gcov)

# Built with UndefinedBehaviorSanitizer, in abort mode: base85_encode_64bytes()
# shifts a promoted uint8_t left by 24, which is undefined once the byte has
# its top bit set. Nothing about the result is visibly wrong, so a value
# assertion cannot see it -- the sanitizer is the assertion.
add_executable(test_base85 tests/base85.c ../../src/common/bip85/seed_rom_variables.c ../../src/common/bip85/base85.c)
target_include_directories(test_base85 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
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

# The BIP-85 derivation paths and the parameter ranges the bolos_ux_bip85_*
# entry points accept. Both are pure arithmetic evaluated before those entry
# points reach os_derive_bip32_no_throw(), so they need no crypto and no
# extra compile definitions -- same source file and same pattern as
# test_bip85_finalize_pwd above. Everything else in seed_bip85.c that these
# paths feed lives behind `#if defined(HAVE_NBGL)` and is compiled into no
# test target at all.
add_executable(test_bip85_derivation_path tests/bip85_derivation_path.c ../../src/common/bip85/seed_bip85.c)
target_include_directories(test_bip85_derivation_path PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
target_link_libraries(test_bip85_derivation_path PUBLIC cmocka gcov testutils)

# The property targets below sweep invariants over randomly drawn inputs, rather
# than checking a particular case the way the rest of the suite does. They share
# tests/unit/lib/testprng.h, a seeded generator kept header-only on purpose:
# adding it to the `testutils` library would relink every pre-existing target
# for the benefit of these. The suite's own cx_rng_no_throw() is neither used
# nor modified by them -- it writes 0, 1, 2, ... and restarts on every call,
# which is exactly what the frozen-vector tests need and exactly what would make
# a property test meaningless.
add_executable(test_sskr_properties tests/sskr_properties.c)
# Same reason as test_sskr_interop_bc128 above: this file passes its own
# generator rather than cx_rng, so it never references a testutils symbol
# directly and the linker's default --as-needed drops libtestutils.so before it
# can satisfy libsss.so's undefined cx_bn_*/cx_hmac_sha256 references.
target_link_options(test_sskr_properties PRIVATE -Wl,--no-as-needed)
target_link_libraries(test_sskr_properties PUBLIC cmocka gcov testutils sskr sss)

# Goes through bolos_ux_bip39_to_sskr_convert() and bolos_ux_sskr_hex_check(),
# so it needs the same set of sources as the other seed_sskr.c tests.
add_executable(test_sskr_wire_properties ./tests/sskr_wire_properties.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_wire_properties PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_wire_properties PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_bip39_properties ./tests/bip39_properties.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c)
target_include_directories(test_bip39_properties PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_bip39_properties PUBLIC cmocka gcov testutils)

# Built with AddressSanitizer, and with sskr.c (plus sss.c/interpolate.c)
# compiled into the test itself rather than linked from libsskr/libsss: the
# refusals this file is about are what keep sskr_combine_shards_internal()
# inside its own one-element groups[]/gx[]/gy[] arrays, so ASan has to
# instrument those frames rather than only this file's. Same arrangement as
# test_sskr_bound_group_count above. seed_sskr.c comes along for
# bolos_ux_sskr_hex_check(), the entry point the user reaches first.
add_executable(test_sskr_multi_group_refusal ./tests/sskr_multi_group_refusal.c ../../src/common/sskr/sskr.c ../../src/common/sskr/sss/sss.c ../../src/common/sskr/sss/interpolate.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_multi_group_refusal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_options(test_sskr_multi_group_refusal PRIVATE -fsanitize=address -fno-omit-frame-pointer)
target_link_options(test_sskr_multi_group_refusal PRIVATE -fsanitize=address)
target_link_libraries(test_sskr_multi_group_refusal PUBLIC cmocka gcov testutils)

# Goes through bolos_ux_bip39_to_sskr_convert(), so it needs the same set of
# sources as the other seed_sskr.c tests.
add_executable(test_sskr_cbor_length_header ./tests/sskr_cbor_length_header.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_include_directories(test_sskr_cbor_length_header PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_cbor_length_header PUBLIC cmocka gcov sskr sss testutils)

# The targets below are the first in this suite to define HAVE_NBGL.
#
# The keyboard suggestions of both entry paths -- bolos_ux_*_fill_with_candidates()
# and bolos_ux_*_get_keyboard_mask(), at the end of seed_sskr.c and seed_bip39.c
# -- live behind `#if defined(HAVE_NBGL)`, and until these targets existed the
# preprocessor removed them from every build the suite made. That is a harder
# gap to notice than an untested function: lines the preprocessor removes are
# neither covered nor uncovered in coverage.info, so lcov reported 96.2% for
# seed_sskr.c over lines 52-486 of a 542-line file, and 95.8% for seed_bip39.c
# over lines 26-319 of a 376-line file, with nothing marked missing. Comparing
# the extent of the DA: entries against the length of the file is what shows it.
# (The same is still true of src/common/bip85/seed_bip85.c, noted above: every
# function in its HAVE_NBGL block reaches os_derive_bip32_no_throw() through
# bolos_ux_bip85_entropy(), a BOLOS syscall with no host equivalent, so that one
# is out of reach for a different reason and stays where it is.)
#
# What blocked it was one line: both guarded blocks open with
# `#include <nbgl_layout.h>`, an SDK header the harness does not have. The only
# symbol they take from it is NB_MAX_SUGGESTION_BUTTONS, so lib/nbgl_layout.h
# supplies that and nothing else, on the model of the empty lib/glyphs.h and
# lib/bolos_target.h.
#
# That constant is target-dependent in the SDK -- 12 on Stax, 8 on Flex and
# Apex -- and it is the cap fill_with_candidates() applies with MIN() before
# writing into buffers the caller sized from the same constant. Since it decides
# what the cap test is testing, each of the two files is built twice, once per
# live value, rather than a value being picked. TARGET_STAX and TARGET_FLEX are
# read by lib/nbgl_layout.h only; neither seed_sskr.c nor seed_bip39.c nor the
# wordlists mention any TARGET_* macro.
add_executable(test_sskr_keyboard_candidates ./tests/sskr_keyboard_candidates.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_compile_definitions(test_sskr_keyboard_candidates PRIVATE HAVE_NBGL TARGET_FLEX)
target_include_directories(test_sskr_keyboard_candidates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_keyboard_candidates PUBLIC cmocka gcov sskr sss testutils)

add_executable(test_sskr_keyboard_candidates_stax ./tests/sskr_keyboard_candidates.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c ../../src/common/sskr/seed_rom_variables.c ../../src/common/sskr/seed_sskr.c)
target_compile_definitions(test_sskr_keyboard_candidates_stax PRIVATE HAVE_NBGL TARGET_STAX)
target_include_directories(test_sskr_keyboard_candidates_stax PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_sskr_keyboard_candidates_stax PUBLIC cmocka gcov sskr sss testutils)

# The BIP-39 half of the same gap, on the same pattern as the SSKR pair above:
# the two functions are structurally identical, and only the wordlist they walk
# differs. Unlike SSKR, this one needs neither sskr nor sss -- seed_bip39.c and
# its wordlist are the whole dependency, as they already are for test_bip39.
add_executable(test_bip39_keyboard_candidates ./tests/bip39_keyboard_candidates.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c)
target_compile_definitions(test_bip39_keyboard_candidates PRIVATE HAVE_NBGL TARGET_FLEX)
target_include_directories(test_bip39_keyboard_candidates PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_bip39_keyboard_candidates PUBLIC cmocka gcov testutils)

add_executable(test_bip39_keyboard_candidates_stax ./tests/bip39_keyboard_candidates.c ../../src/common/bip39/seed_rom_variables.c ../../src/common/bip39/seed_bip39.c)
target_compile_definitions(test_bip39_keyboard_candidates_stax PRIVATE HAVE_NBGL TARGET_STAX)
target_include_directories(test_bip39_keyboard_candidates_stax PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common)
target_link_libraries(test_bip39_keyboard_candidates_stax PUBLIC cmocka gcov testutils)

# ---------------------------------------------------------------------------
# The SSKR share display and the two SSKR menus of the BAGL stack.
#
# Both pieces come out of src/bagl/ux_sskr.c, which is compiled into no target
# here and so is absent from the coverage report rather than sitting in it at
# 0 %. Speculos dropped Nano S, so a unit test is the only thing that will
# ever run that stack's arithmetic; Nano S+ and Nano X share the file and are
# emulated, but only through a UX_FLOW.
#
# Each of the two is built twice, with and without TARGET_NANOS, for the same
# reason the Shamir targets above are: what is not compiled is neither covered
# nor uncovered. The share-count table has 7 entries under TARGET_NANOS and 16
# otherwise, and SSS_MAX_SHARE_COUNT is 10 and 16 -- the relation between them
# is the point of test_sskr_ux_menu, and it is a different relation in each of
# the two configurations. target_compile_definitions() applies to every source
# of a target, so the test file and the file under test always agree.
add_executable(test_sskr_share_slice ./tests/sskr_share_slice.c ../../src/common/sskr/sskr_share_slice.c)
target_include_directories(test_sskr_share_slice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_link_libraries(test_sskr_share_slice PUBLIC cmocka gcov testutils)

add_executable(test_sskr_share_slice_nanos ./tests/sskr_share_slice.c ../../src/common/sskr/sskr_share_slice.c)
target_include_directories(test_sskr_share_slice_nanos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_definitions(test_sskr_share_slice_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sskr_share_slice_nanos PUBLIC cmocka gcov testutils)

add_executable(test_sskr_ux_menu ./tests/sskr_ux_menu.c ../../src/bagl/ux_sskr_menu.c)
target_include_directories(test_sskr_ux_menu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_link_libraries(test_sskr_ux_menu PUBLIC cmocka gcov testutils)

add_executable(test_sskr_ux_menu_nanos ./tests/sskr_ux_menu.c ../../src/bagl/ux_sskr_menu.c)
target_include_directories(test_sskr_ux_menu_nanos PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../src/common/sskr/sss)
target_compile_definitions(test_sskr_ux_menu_nanos PRIVATE TARGET_NANOS)
target_link_libraries(test_sskr_ux_menu_nanos PUBLIC cmocka gcov testutils)
# ---------------------------------------------------------------------------

# Register every executable this file builds, instead of repeating their names
# in a list kept by hand. A name missing from such a list costs nothing
# visible: the target still compiles, still passes when run directly, and the
# suite stays green -- it is simply never run by ctest or by CI again.
#
# Keep this at the end of the file: it reads the targets that exist at the
# point it runs, so anything added below it would not be picked up. Adding a
# test needs nothing but its add_executable() above; there is no list to
# remember and no helper to know about.
#
# Every executable built here is a test, and the libraries (testutils, sss,
# sskr, openssl) are filtered out by type rather than by name, so a new
# library does not need this to be touched either.
get_property(built_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target ${built_targets})
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "EXECUTABLE")
        add_test(NAME ${target} COMMAND ${target})
    endif()
endforeach()
