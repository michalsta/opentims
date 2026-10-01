/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "so_manager.h"

#if defined(OPENTIMS_WINDOWS)
static const char* const kSystemLibrary = "kernel32.dll";
static const char* const kSystemSymbol = "GetTickCount";
#elif defined(__APPLE__)
static const char* const kSystemLibrary = "/usr/lib/libSystem.B.dylib";
static const char* const kSystemSymbol = "strlen";
#else
static const char* const kSystemLibrary = "libc.so.6";
static const char* const kSystemSymbol = "strlen";
#endif

TEST(LoadedLibraryHandle, MissingLibraryThrowsNamingIt)
{
    try
    {
        LoadedLibraryHandle h("/nonexistent/dir/libopentims_no_such_library.so");
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("libopentims_no_such_library"), std::string::npos) << e.what();
    }
}

TEST(LoadedLibraryHandle, SystemLibrarySymbolLookup)
{
#if defined(__linux__) && !defined(__GLIBC__)
    GTEST_SKIP() << "libc soname differs off glibc";
#endif
    LoadedLibraryHandle h(kSystemLibrary);
    EXPECT_NE(h.symbol_lookup<void>(kSystemSymbol), nullptr);
}

TEST(LoadedLibraryHandle, MissingSymbolThrows)
{
#if defined(__linux__) && !defined(__GLIBC__)
    GTEST_SKIP() << "libc soname differs off glibc";
#endif
    LoadedLibraryHandle h(kSystemLibrary);
    EXPECT_THROW(h.symbol_lookup<void>("opentims_no_such_symbol_xyz"), std::runtime_error);
}

TEST(LoadedLibraryHandle, EmptyPathMeansTheProcess)
{
    LoadedLibraryHandle h("");
    EXPECT_THROW(h.symbol_lookup<void>("opentims_no_such_symbol_xyz"), std::runtime_error);
}

TEST(LoadedLibraryHandle, RepeatedLoadUnload)
{
#if defined(__linux__) && !defined(__GLIBC__)
    GTEST_SKIP() << "libc soname differs off glibc";
#endif
    for(int i = 0; i < 50; i++)
    {
        LoadedLibraryHandle h(kSystemLibrary);
        ASSERT_NE(h.symbol_lookup<void>(kSystemSymbol), nullptr);
    }
}
