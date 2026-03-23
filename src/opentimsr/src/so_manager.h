/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <string>
#include <stdexcept>



#include "platform.h"

#if defined(OPENTIMS_UNIX)

#include <dlfcn.h>

class LoadedLibraryHandle
// RAII-style wrapper for results of dlopen()
{
    void* os_handle;
 public:
    LoadedLibraryHandle(const std::string& path);
    ~LoadedLibraryHandle();

    template<typename T> T* symbol_lookup(const std::string& symbol_name) const
    {
        dlerror(); // Clear dllerror
        void* ret = dlsym(os_handle, symbol_name.c_str()); // nullptr might be a valid result here, got to check dlerror...
        const char* errmsg = dlerror();
        if(errmsg != nullptr)
            throw std::runtime_error(std::string("Symbol lookup failed for ") + symbol_name + ", reason: " + errmsg);
        return reinterpret_cast<T*>(ret);
    }
};



#elif defined(OPENTIMS_WINDOWS)

#include <libloaderapi.h>
#include <errhandlingapi.h>

class LoadedLibraryHandle
// RAII-style wrapper for results of LoadLibrary()
{
    HMODULE os_handle;
 public:
    LoadedLibraryHandle(const std::string& path);
    ~LoadedLibraryHandle();

    template<typename T> T* symbol_lookup(const std::string& symbol_name) const
    {
        FARPROC ret = GetProcAddress(os_handle, symbol_name.c_str()); // nullptr might be a valid result here, got to check dlerror...
        if(ret == nullptr)
            throw std::runtime_error(std::string("Symbol lookup failed for ") + symbol_name + ", reason: " + std::to_string(GetLastError()));
        return reinterpret_cast<T*>(ret);
    }
};



#else
// provide empty stubs

class LoadedLibraryHandle
// RAII-style wrapper for results of dlopen()
{
 public:
    LoadedLibraryHandle(const std::string&);
    ~LoadedLibraryHandle();

    template<typename T> T* symbol_lookup(const std::string& symbol_name) const
    {
        throw std::runtime_error(std::string("Loading additional libraries is not supported on your platform"));
    }
};
#endif
