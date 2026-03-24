cmake_minimum_required(VERSION 3.15)
project(opentims LANGUAGES CXX)

option(OPENTIMS_BUILD_PYTHON "Build Python pybind11 bindings" ON)
option(OPENTIMS_BUILD_CPP_LIB "Build opentims as a static C++ library" OFF)
option(OPENTIMS_LINK_SQLITE_STATICALLY "Link sqlite3 statically instead of dlopening it at runtime" OFF)

find_package(Threads REQUIRED)

set(OPENTIMS_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++")

set(OPENTIMS_CPP_SOURCES
    "${OPENTIMS_SRC_DIR}/opentims.cpp"
    "${OPENTIMS_SRC_DIR}/tof2mz_converter.cpp"
    "${OPENTIMS_SRC_DIR}/scan2inv_ion_mobility_converter.cpp"
    "${OPENTIMS_SRC_DIR}/converters.cpp"
    "${OPENTIMS_SRC_DIR}/so_manager.cpp"
    "${OPENTIMS_SRC_DIR}/thread_mgr.cpp"
    "${OPENTIMS_SRC_DIR}/sqlite_helper.cpp"
)

#------------------------------------------------------------------------------
# C++ static library target (for embedding in other projects like OpenMS)
#------------------------------------------------------------------------------
if (OPENTIMS_BUILD_CPP_LIB)
    add_library(opentims_cpp STATIC ${OPENTIMS_CPP_SOURCES})
    target_include_directories(opentims_cpp PUBLIC "${OPENTIMS_SRC_DIR}")
    target_compile_features(opentims_cpp PRIVATE cxx_std_20)
    set_target_properties(opentims_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)

    if (OPENTIMS_LINK_SQLITE_STATICALLY)
        target_compile_definitions(opentims_cpp PUBLIC OPENTIMS_LINK_SQLITE_STATICALLY)
        # Consumer must provide sqlite3 headers via target_include_directories
        # and link sqlite3 via target_link_libraries.
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(opentims_cpp PRIVATE -O3 -g -Wall -Wextra)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(opentims_cpp PRIVATE /O2)
        target_compile_definitions(opentims_cpp PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()

    if (UNIX AND NOT APPLE)
        target_link_libraries(opentims_cpp PRIVATE pthread dl)
    endif()
endif()

#------------------------------------------------------------------------------
# Python pybind11 module target
#------------------------------------------------------------------------------
if (OPENTIMS_BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 REQUIRED)

    # Build the pybind11 extension as a Python module
    pybind11_add_module(opentimspy_cpp MODULE src/opentims++/opentims_pybind11.cpp)

    # Link pthread and dl if not on Windows
    if (UNIX AND NOT APPLE)
        target_link_libraries(opentimspy_cpp PRIVATE pthread dl)
    endif()

    target_compile_features(opentimspy_cpp PRIVATE cxx_std_20)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(opentimspy_cpp PRIVATE -O3 -g -Wall -Wextra)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(opentimspy_cpp PRIVATE /O2)
    endif()

    target_include_directories(opentimspy_cpp PRIVATE ${CMAKE_SOURCE_DIR}/src/opentims++)

    # Install Python package files
    install(TARGETS opentimspy_cpp LIBRARY DESTINATION opentimspy)
    install(DIRECTORY src/opentimspy/ DESTINATION opentimspy FILES_MATCHING PATTERN "*.py" PATTERN "opentims++" EXCLUDE)
    file(GLOB OPENTIMS_HEADERS
        "${CMAKE_SOURCE_DIR}/src/opentims++/*.h"
        "${CMAKE_SOURCE_DIR}/src/opentims++/*.hpp"
        "${CMAKE_SOURCE_DIR}/src/opentims++/*/*.h"
        "${CMAKE_SOURCE_DIR}/src/opentims++/*/*.hpp"
)
    install(FILES ${OPENTIMS_HEADERS} DESTINATION opentimspy/opentims++)
endif() # OPENTIMS_BUILD_PYTHON
