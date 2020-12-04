#pragma once

#include <string>
#include <stdexcept>



#include "platform.h"

#if defined(OPENTIMS_UNIX)

#include "dlfcn.h"

class LoadedLibraryHandle
// RAII-style wrapper for results of dlopen()
{
    void* os_handle;
 public:
    LoadedLibraryHandle(const std::string& path) : os_handle(nullptr)
    {
        os_handle = dlopen(path.c_str(), RTLD_LAZY);
        if(os_handle == nullptr)
            throw std::runtime_error(std::string("dlopen(") + path + ") failed, reason: " + dlerror());
    }

    ~LoadedLibraryHandle()
    {
        if(os_handle != nullptr)
            dlclose(os_handle);
        // Deliberately not handling errors in dlclose() call here.
    }

    template<typename T> T* symbol_lookup(const std::string& symbol_name)
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
    LoadedLibraryHandle(const std::string& path) : os_handle(nullptr)
    {
        os_handle = LoadLibraryA(path.c_str());
        if(os_handle == nullptr)
            throw std::runtime_error(std::string("LoadLibraryA(") + path + ") failed, reason: " + std::to_string(GetLastError()));
    }

    ~LoadedLibraryHandle()
    {
        if(os_handle != nullptr)
            FreeLibrary(os_handle);
    }

    template<typename T> T* symbol_lookup(const std::string& symbol_name)
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
    LoadedLibraryHandle(const std::string&) {}

    ~LoadedLibraryHandle() {}

    template<typename T> T* symbol_lookup(const std::string& symbol_name)
    {
        throw std::runtime_error(std::string("Loading additional libraries is not supported on your platform"));
    }
};
#endif
