cmake_minimum_required(VERSION 3.10)

if(${CMAKE_VERSION} VERSION_LESS 3.10)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# project information
project(FuzzSeedTool
        VERSION 1.0
        DESCRIPTION "Fuzzing of the Seed Tool entry parsers"
        LANGUAGES C)

# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Fuzzer needs to be built with Clang")
endif()

if (NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK environment variable not found.")
endif()

# guard against in-source builds
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

# compatible with ClusterFuzzLite
if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
	set(COMPILATION_FLAGS_ "-g -Wall -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer")
else()
	set(COMPILATION_FLAGS_ "$ENV{LIB_FUZZING_ENGINE} $ENV{CFLAGS}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(REPLACE " " ";" COMPILATION_FLAGS ${COMPILATION_FLAGS_})

include(extra/SeedParsers.cmake)

# The instrumentation has to reach the parsers, not just the harness: the
# library is built into each target's flags rather than linked as an
# uninstrumented blob, which is what makes coverage feedback -- and therefore
# the fuzzer's ability to explore -- work at all.
target_compile_options(seedparsers PUBLIC ${COMPILATION_FLAGS})
target_link_options(seedparsers PUBLIC ${COMPILATION_FLAGS})

# One target per parser. Naming them after the function under test rather than
# after the file it lives in is what keeps a ClusterFuzzLite report readable
# from the name alone.
set(FUZZ_TARGETS
    fuzz_sskr_hex_check
    fuzz_sskr_share_entry
    fuzz_sskr_wordlist
    fuzz_bip39_mnemonic
)

foreach(target ${FUZZ_TARGETS})
    add_executable(${target} ${target}.c)
    target_compile_options(${target} PUBLIC ${COMPILATION_FLAGS})
    target_link_options(${target} PUBLIC ${COMPILATION_FLAGS})
    target_link_libraries(${target} PUBLIC seedparsers)
endforeach()
