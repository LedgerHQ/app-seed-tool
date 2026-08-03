# The application code the fuzz targets call into, plus the host stand-ins the
# device syscalls need. Kept in its own file, the way app-boilerplate keeps
# extra/TxParser.cmake, so the top-level CMakeLists.txt holds nothing but the
# targets.
#
# Only the parsers reachable from the screen are built here. Everything that
# reaches sss_recover_secret() is deliberately absent: interpolate() is written
# against the SDK's cx_bn_* big-number API, which on a host exists only as the
# OpenSSL-backed stand-in under tests/unit/lib/bolos, and that stand-in is why
# the unit test suite compiles OpenSSL from source. Pulling that into a fuzzer
# build would multiply its build time by an order of magnitude for a code path
# that is arithmetic, not parsing. fuzzing/README.md says the same at more
# length, and unreachable.c makes the boundary an abort rather than a silent
# stub.

project(SeedParsers
        VERSION 1.0
        DESCRIPTION "Entry-side parsers of the Seed Tool application"
        LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

set(APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# DEBUG=0 is what turns the SDK's PRINTF into a no-op off-device, as it does in
# the unit test build. HAVE_* mirror the subset tests/unit/CMakeLists.txt
# enables, restricted to what these sources use: the mnemonic-to-seed step is
# PBKDF2-HMAC-SHA512, and the SSKR share digest is HMAC-SHA256.
add_definitions(-DTEST -DDEBUG=0)
add_definitions(-DHAVE_HASH -DHAVE_HMAC -DHAVE_SHA224 -DHAVE_SHA256
                -DHAVE_SHA512 -DHAVE_PBKDF2 -DHAVE_CRC -DHAVE_RNG -DHAVE_ECC)
# Sizes the SDK's I/O headers ask for. Nothing here does any I/O -- the
# application exchanges no APDU at all -- but os.h reaches them, so they need
# a value; these are the ones the unit test build uses.
add_definitions(-DIO_HID_EP_LENGTH=64 -DOS_IO_SEPH_BUFFER_SIZE=272)

add_library(seedparsers
    ${APP_DIR}/common/bip39/seed_bip39.c
    ${APP_DIR}/common/bip39/seed_rom_variables.c
    ${APP_DIR}/common/sskr/seed_sskr.c
    ${APP_DIR}/common/sskr/seed_rom_variables.c
    ${APP_DIR}/common/sskr/sskr_entry_header.c
    ${APP_DIR}/common/sskr/sskr_share_slice.c
    ${APP_DIR}/common/sskr/sskr.c
    ${APP_DIR}/common/sskr/sss/sss.c
    ${BOLOS_SDK}/lib_cxng/src/cx_ram.c
    ${BOLOS_SDK}/lib_cxng/src/cx_hash.c
    ${BOLOS_SDK}/lib_cxng/src/cx_sha256.c
    ${BOLOS_SDK}/lib_cxng/src/cx_sha512.c
    ${BOLOS_SDK}/lib_cxng/src/cx_hmac.c
    ${BOLOS_SDK}/lib_cxng/src/cx_pbkdf2.c
    ${BOLOS_SDK}/lib_cxng/src/cx_utils.c
    ${BOLOS_SDK}/lib_cxng/src/cx_crc32.c
    # cx_crc32() delegates to the cx_crc_hw() syscall. The SDK ships its own
    # host stand-in for it, which is preferred here over writing a third
    # CRC-32 (the cmocka suite has one in tests/unit/lib/bolos, and
    # tests/unit/tests/sskr_hex_check_guards.c a deliberately independent one).
    ${BOLOS_SDK}/unit-tests/mock/src/cx_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extra/host_syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/extra/unreachable.c
)

set_target_properties(seedparsers PROPERTIES SOVERSION 1)

target_include_directories(seedparsers PUBLIC
    # bolos_target.h and the other headers the SDK expects a device build to
    # provide. The cmocka suite already carries them; a second copy under
    # fuzzing/ would be a second thing to keep in step.
    ${CMAKE_CURRENT_SOURCE_DIR}/../tests/unit/lib
    ${APP_DIR}
    ${APP_DIR}/common
    ${APP_DIR}/common/sskr
    ${APP_DIR}/common/sskr/sss
    ${BOLOS_SDK}/include
    ${BOLOS_SDK}/lib_cxng/include
    ${BOLOS_SDK}/lib_cxng/src
    ${BOLOS_SDK}/io/include
    ${BOLOS_SDK}/io_legacy/include
    ${BOLOS_SDK}/lib_ux/include
    ${BOLOS_SDK}/lib_bagl/include
)

# The application build compiles these sources for a device, where several of
# the SDK's own headers carry warnings this repository cannot act on. The
# fuzzers care about what the sanitizers say at run time, not about the
# warnings, so the list is the same one the unit tests use minus -Werror.
target_compile_options(seedparsers PRIVATE -Wall -Wno-unused-function)
