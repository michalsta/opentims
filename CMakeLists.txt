cmake_minimum_required(VERSION 3.15)

# Parse version from pyproject.toml — single source of truth.
# CMake project(VERSION ...) only accepts numeric dotted versions, so the
# pre-release suffix (e.g. "b2" in "1.2.0b2") is stripped and stored
# separately for informational use.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/pyproject.toml" _pyproject_toml)
string(REGEX MATCH "version = \"([0-9]+\\.[0-9]+\\.[0-9]+)([^\"]*)\"" _ "${_pyproject_toml}")
set(OPENTIMS_VERSION      "${CMAKE_MATCH_1}")              # 1.2.0  — used for project() / soname
set(OPENTIMS_VERSION_FULL "${CMAKE_MATCH_1}${CMAKE_MATCH_2}") # 1.2.0b2 — informational

project(opentims VERSION "${OPENTIMS_VERSION}" LANGUAGES CXX C)

# BUILD_SHARED_LIBS (standard CMake variable) controls static vs shared.
# Pass -DBUILD_SHARED_LIBS=ON for a shared library (.so/.dylib).
option(OPENTIMS_BUILD_LIB    "Build the opentims C++ library"      ON)
option(OPENTIMS_BUILD_PYTHON "Build Python pybind11 bindings"       OFF)
option(OPENTIMS_LINK_SQLITE_STATICALLY
       "Link sqlite3 at compile time instead of dlopening at runtime (static lib only)" OFF)

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

# Apply standard compile options to a target.
macro(opentims_compile_options _target)
    target_compile_features(${_target} PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${_target} PRIVATE -O3 -g -Wall -Wextra)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${_target} PRIVATE /O2)
        target_compile_definitions(${_target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endmacro()

# Link zstd to a target: prefer system, fall back to the bundled decoder.
macro(opentims_link_zstd _target)
    find_package(zstd QUIET)
    if(TARGET zstd::libzstd_shared)
        target_link_libraries(${_target} PRIVATE zstd::libzstd_shared)
        message(STATUS "opentims(${_target}): using system zstd (shared)")
    elseif(TARGET zstd::libzstd_static)
        target_link_libraries(${_target} PRIVATE zstd::libzstd_static)
        message(STATUS "opentims(${_target}): using system zstd (static)")
    else()
        target_sources(${_target} PRIVATE "${OPENTIMS_SRC_DIR}/zstd/zstddeclib.c")
        target_include_directories(${_target} PRIVATE "${OPENTIMS_SRC_DIR}/zstd")
        message(STATUS "opentims(${_target}): using bundled zstd decoder")
    endif()
endmacro()

# Link system sqlite3 to a target at compile time (not dlopen).
macro(opentims_link_sqlite _target)
    find_package(SQLite3 REQUIRED)
    target_link_libraries(${_target} PRIVATE SQLite::SQLite3)
    target_compile_definitions(${_target} PUBLIC OPENTIMS_LINK_SQLITE_STATICALLY)
endmacro()

#------------------------------------------------------------------------------
# C++ library (static or shared, controlled by BUILD_SHARED_LIBS)
#------------------------------------------------------------------------------
if(OPENTIMS_BUILD_LIB)
    include(GNUInstallDirs)

    add_library(opentims_cpp ${OPENTIMS_CPP_SOURCES})
    target_include_directories(opentims_cpp PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    opentims_compile_options(opentims_cpp)

    if(BUILD_SHARED_LIBS)
        set_target_properties(opentims_cpp PROPERTIES
            VERSION   "${OPENTIMS_VERSION}"
            SOVERSION "${PROJECT_VERSION_MAJOR}"
        )
        # Shared lib absorbs sqlite3 and zstd — no runtime dlopen, no consumer deps.
        opentims_link_sqlite(opentims_cpp)
        opentims_link_zstd(opentims_cpp)
    else()
        set_target_properties(opentims_cpp PROPERTIES POSITION_INDEPENDENT_CODE ON)
        if(OPENTIMS_LINK_SQLITE_STATICALLY)
            target_compile_definitions(opentims_cpp PUBLIC OPENTIMS_LINK_SQLITE_STATICALLY)
            # Consumer must supply sqlite3 headers and the library
            # (e.g. OpenMS injects its own bundled sqlite3).
        endif()
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(opentims_cpp PRIVATE pthread dl)
    endif()

    # Install rules — produces libopentims-dev / libopentims style package contents.
    include(CMakePackageConfigHelpers)

    if(BUILD_SHARED_LIBS)
        # Split the shared-lib filesystem entries across components:
        #   Runtime     — versioned .so and SONAME symlink  (libopentims)
        #   Development — unversioned linker symlink .so    (libopentims-dev)
        install(TARGETS opentims_cpp
            EXPORT opentimsTargets
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                NAMELINK_SKIP
            COMPONENT Runtime
        )
        install(TARGETS opentims_cpp
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                NAMELINK_ONLY
            COMPONENT Development
        )
    else()
        install(TARGETS opentims_cpp
            EXPORT opentimsTargets
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            COMPONENT Development
        )
    endif()

    file(GLOB_RECURSE _opentims_headers
        "${OPENTIMS_SRC_DIR}/*.h"
        "${OPENTIMS_SRC_DIR}/*.hpp"
    )
    install(FILES ${_opentims_headers}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/opentims++"
        COMPONENT Development
    )

    install(EXPORT opentimsTargets
        FILE      opentimsTargets.cmake
        NAMESPACE opentims::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/opentims"
        COMPONENT Development
    )

    # Shared lib absorbs all deps; static+OPENTIMS_LINK_SQLITE_STATICALLY needs
    # the consumer to find SQLite3 (unless the consumer, e.g. OpenMS, supplies it
    # itself — in that case the consumer ignores this find_dependency).
    if(BUILD_SHARED_LIBS OR NOT OPENTIMS_LINK_SQLITE_STATICALLY)
        set(OPENTIMS_FIND_DEPS "")
    else()
        set(OPENTIMS_FIND_DEPS "find_dependency(SQLite3 REQUIRED)")
    endif()

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/opentimsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/opentimsConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/opentims"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/opentimsConfigVersion.cmake"
        VERSION     "${OPENTIMS_VERSION}"
        COMPATIBILITY AnyNewerVersion
    )
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/opentimsConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/opentimsConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/opentims"
        COMPONENT Development
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/opentims.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/opentims.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/opentims.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT Development
    )

    if(BUILD_SHARED_LIBS)
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}"
            COMPONENT Runtime
            RENAME copyright
        )
    endif()
endif()

#------------------------------------------------------------------------------
# Python pybind11 module
#------------------------------------------------------------------------------
if(OPENTIMS_BUILD_PYTHON)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
    find_package(pybind11 REQUIRED)

    if(OPENTIMS_BUILD_LIB AND BUILD_SHARED_LIBS)
        # Link against the shared lib — no source recompilation needed.
        pybind11_add_module(opentimspy_cpp MODULE
            src/opentims++/opentims_pybind11.cpp
        )
        target_link_libraries(opentimspy_cpp PRIVATE opentims_cpp)
    else()
        # No shared lib — compile C++ sources directly into the module.
        pybind11_add_module(opentimspy_cpp MODULE
            src/opentims++/opentims_pybind11.cpp
            ${OPENTIMS_CPP_SOURCES}
        )
        # Python wheels: always use the bundled zstd decoder rather than system
        # libzstd.  System shared libzstd on macOS CI runners is typically a fat
        # universal binary (arm64 + x86_64); delocate-wheel --require-archs arm64
        # rejects such libraries, causing the wheel build to fail with code 1.
        # The bundled single-file decoder produces no external dylib dependency
        # and restores the behaviour that existed before the CMakeLists rewrite.
        target_sources(opentimspy_cpp PRIVATE "${OPENTIMS_SRC_DIR}/zstd/zstddeclib.c")
        target_include_directories(opentimspy_cpp PRIVATE "${OPENTIMS_SRC_DIR}/zstd")
        if(OPENTIMS_LINK_SQLITE_STATICALLY)
            opentims_link_sqlite(opentimspy_cpp)
        endif()
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(opentimspy_cpp PRIVATE pthread dl)
    endif()

    target_compile_features(opentimspy_cpp PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(opentimspy_cpp PRIVATE -O3 -g -Wall -Wextra)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(opentimspy_cpp PRIVATE /O2)
    endif()

    target_include_directories(opentimspy_cpp
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++"
    )

    install(TARGETS opentimspy_cpp
        LIBRARY DESTINATION opentimspy
        COMPONENT Python
    )
    install(DIRECTORY src/opentimspy/
        DESTINATION opentimspy
        COMPONENT Python
        FILES_MATCHING PATTERN "*.py" PATTERN "opentims++" EXCLUDE
    )
    file(GLOB _opentims_py_headers
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++/*/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/opentims++/*/*.hpp"
    )
    install(FILES ${_opentims_py_headers}
        DESTINATION opentimspy/opentims++
        COMPONENT Python
    )
endif()
