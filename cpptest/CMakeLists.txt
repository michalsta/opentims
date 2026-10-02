# C++ test suite for the opentims++ core. Enabled with -DOPENTIMS_BUILD_TESTS=ON.
#
# Two test executables run the same tests against the two SQLite backends:
#   opentims_tests         sqlite linked in (OPENTIMS_LINK_SQLITE_STATICALLY)
#   opentims_tests_dlopen  sqlite dlopen'ed at runtime, as the Python module does
# Both compile the core, the bundled zstd decoder and the bundled sqlite from
# source, so sanitizers instrument all of it.
#
# Checking modes, applied to everything built here (GoogleTest included):
#   -DOPENTIMS_SANITIZE="address;undefined"   any of address, undefined, thread,
#                                             leak, float-cast-overflow, memory;
#                                             MSVC: address only. memory (MSan)
#                                             also needs an MSan-instrumented C++
#                                             standard library, or it reports
#                                             false positives inside it.
#   -DOPENTIMS_HARDENED=ON                    debug-mode standard library
#                                             containers, _FORTIFY_SOURCE=3,
#                                             stack protection, pattern-initialised
#                                             locals
#   -DOPENTIMS_BUILD_FUZZERS=ON               libFuzzer targets (Clang)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(OPENTIMS_SANITIZE "" CACHE STRING "Sanitizers for the test build (;-separated)")
option(OPENTIMS_HARDENED "Hardened standard library and runtime checks for the test build" OFF)
option(OPENTIMS_BUILD_FUZZERS "Build libFuzzer targets (requires Clang)" OFF)

include(CheckCXXCompilerFlag)

#------------------------------------------------------------------------------
# Sanitizers and hardening: directory-wide, so GoogleTest gets them too
# (needed for the debug-mode containers' ABI, and for MemorySanitizer).
#------------------------------------------------------------------------------
if(OPENTIMS_SANITIZE)
    if(MSVC)
        if(NOT OPENTIMS_SANITIZE STREQUAL "address")
            message(FATAL_ERROR "MSVC supports only OPENTIMS_SANITIZE=address")
        endif()
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO /DEBUG)
    else()
        list(JOIN OPENTIMS_SANITIZE "," _ot_sanitizers)
        add_compile_options(-fsanitize=${_ot_sanitizers} -fno-sanitize-recover=all -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${_ot_sanitizers})
        if("memory" IN_LIST OPENTIMS_SANITIZE)
            add_compile_options(-fsanitize-memory-track-origins=2)
        endif()
    endif()
    message(STATUS "opentims tests: sanitizers ${OPENTIMS_SANITIZE}")
endif()

if(OPENTIMS_HARDENED)
    if(MSVC)
        # The debug CRT already checks iterators (_ITERATOR_DEBUG_LEVEL=2) in Debug builds.
        add_compile_options(/GS /sdl)
    else()
        add_compile_definitions(
            _GLIBCXX_DEBUG _GLIBCXX_DEBUG_PEDANTIC _GLIBCXX_ASSERTIONS
            _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG)
        # Fortify needs optimisation; replace any distribution default level.
        add_compile_options($<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE> $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
        foreach(_flag -fstack-protector-strong -fstack-clash-protection -ftrivial-auto-var-init=pattern)
            string(MAKE_C_IDENTIFIER "OT_HAS${_flag}" _var)
            check_cxx_compiler_flag(${_flag} ${_var})
            if(${_var})
                add_compile_options(${_flag})
            endif()
        endforeach()
    endif()
    message(STATUS "opentims tests: hardened build")
endif()

#------------------------------------------------------------------------------
# GoogleTest
#------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

#------------------------------------------------------------------------------
# Bundled third-party code, built from source (warnings off: not ours to fix)
#------------------------------------------------------------------------------
add_library(opentims_test_zstd STATIC "${OPENTIMS_SRC_DIR}/zstd/zstddeclib.c")
target_include_directories(opentims_test_zstd PUBLIC "${OPENTIMS_SRC_DIR}/zstd")

add_library(opentims_test_sqlite STATIC "${OPENTIMS_SRC_DIR}/sqlite/sqlite3.c")
target_include_directories(opentims_test_sqlite PUBLIC "${OPENTIMS_SRC_DIR}/sqlite")
target_link_libraries(opentims_test_sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

add_library(opentims_test_sqlite_module MODULE "${OPENTIMS_SRC_DIR}/sqlite/sqlite3.c")
if(WIN32)
    target_compile_definitions(opentims_test_sqlite_module PRIVATE "SQLITE_API=__declspec(dllexport)")
endif()
target_link_libraries(opentims_test_sqlite_module PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

# The bundled single-file zstd decoder uses intptr_t without <stdint.h> in its
# MemorySanitizer-only code path.
if("memory" IN_LIST OPENTIMS_SANITIZE)
    target_compile_options(opentims_test_zstd PRIVATE -include stdint.h)
endif()

foreach(_t opentims_test_zstd opentims_test_sqlite opentims_test_sqlite_module)
    set_target_properties(${_t} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    if(MSVC)
        target_compile_options(${_t} PRIVATE /w)
    else()
        target_compile_options(${_t} PRIVATE -w)
    endif()
endforeach()

#------------------------------------------------------------------------------
# Test executables
#------------------------------------------------------------------------------
add_library(opentims_test_bruker_module MODULE "${CMAKE_CURRENT_SOURCE_DIR}/bruker_stub.cpp")
opentims_compile_options(opentims_test_bruker_module)
target_include_directories(opentims_test_bruker_module PRIVATE "${OPENTIMS_SRC_DIR}")

set(OPENTIMS_TEST_SUPPORT
    "${CMAKE_CURRENT_SOURCE_DIR}/dataset_builder.cpp"
)
set(OPENTIMS_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/test_main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_parse_number.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_sqlite_helper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_so_manager.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_converters.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_bruker_errors.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_handle.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_extraction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_corruption.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test_threading.cpp"
)

# Core sources + test support, for one sqlite backend.
function(opentims_test_target _name _sqlite_static)
    target_sources(${_name} PRIVATE ${OPENTIMS_CPP_SOURCES} ${OPENTIMS_TEST_SUPPORT})
    opentims_compile_options(${_name})
    target_include_directories(${_name} PRIVATE "${OPENTIMS_SRC_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${_name} PRIVATE "OPENTIMS_TEST_D=\"${PROJECT_SOURCE_DIR}/pytest/test.d\"")
    target_link_libraries(${_name} PRIVATE opentims_test_zstd Threads::Threads ${CMAKE_DL_LIBS})
    if(_sqlite_static)
        target_compile_definitions(${_name} PRIVATE OPENTIMS_LINK_SQLITE_STATICALLY)
        target_link_libraries(${_name} PRIVATE opentims_test_sqlite)
    else()
        add_dependencies(${_name} opentims_test_sqlite_module)
        target_compile_definitions(${_name} PRIVATE
            "OPENTIMS_TEST_SQLITE_MODULE=\"$<TARGET_FILE:opentims_test_sqlite_module>\"")
    endif()
endfunction()

foreach(_variant static dlopen)
    if(_variant STREQUAL "static")
        set(_exe opentims_tests)
        set(_static ON)
    else()
        set(_exe opentims_tests_dlopen)
        set(_static OFF)
    endif()
    add_executable(${_exe} ${OPENTIMS_TEST_SOURCES})
    opentims_test_target(${_exe} ${_static})
    target_link_libraries(${_exe} PRIVATE GTest::gtest)
    add_dependencies(${_exe} opentims_test_bruker_module)
    target_compile_definitions(${_exe} PRIVATE
        "OPENTIMS_TEST_BRUKER_MODULE=\"$<TARGET_FILE:opentims_test_bruker_module>\"")
    gtest_discover_tests(${_exe}
        TEST_PREFIX "${_variant}."
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 120
        PROPERTIES TIMEOUT 600)
endforeach()

#------------------------------------------------------------------------------
# Fuzzers
#------------------------------------------------------------------------------
if(OPENTIMS_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "OPENTIMS_BUILD_FUZZERS requires Clang (libFuzzer)")
    endif()
    add_executable(opentims_fuzz_frame "${CMAKE_CURRENT_SOURCE_DIR}/fuzz_frame.cpp")
    opentims_test_target(opentims_fuzz_frame ON)
    target_compile_options(opentims_fuzz_frame PRIVATE -fsanitize=fuzzer)
    target_link_options(opentims_fuzz_frame PRIVATE -fsanitize=fuzzer)
endif()
