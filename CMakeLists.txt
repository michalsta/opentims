cmake_minimum_required(VERSION 3.15)
project(opentimspy LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED)
find_package(Threads REQUIRED)


# Build the pybind11 extension as a Python module
pybind11_add_module(opentimspy_cpp MODULE opentims++/opentims_pybind11.cpp)

# Link pthread and dl if not on Windows
if (UNIX AND NOT APPLE)
    target_link_libraries(opentimspy_cpp PRIVATE pthread dl)
endif()

target_compile_features(opentimspy_cpp PRIVATE cxx_std_20)
target_compile_options(opentimspy_cpp PRIVATE -O3 -g -Wall -Wextra)

target_include_directories(opentimspy_cpp PRIVATE ${CMAKE_SOURCE_DIR}/opentims++)

# Install Python package files
install(TARGETS opentimspy_cpp LIBRARY DESTINATION opentimspy)
install(DIRECTORY opentimspy/ DESTINATION opentimspy FILES_MATCHING PATTERN "*.py" PATTERN "opentims++" EXCLUDE)
file(GLOB OPENTIMS_HEADERS
    "${CMAKE_SOURCE_DIR}/opentims++/*.h"
    "${CMAKE_SOURCE_DIR}/opentims++/*.hpp"
    "${CMAKE_SOURCE_DIR}/opentims++/*/*.h"
    "${CMAKE_SOURCE_DIR}/opentims++/*/*.hpp"
)
install(FILES ${OPENTIMS_HEADERS} DESTINATION opentimspy/opentims++)
