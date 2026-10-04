/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <exception>
#include <string>
#include <stdexcept>

#if defined(OPENTIMS_BUILDING_R)
#error "R builds read SQLite tables through DBI/RSQLite; sqlite_helper.h must not be included"

#elif defined(OPENTIMS_LINK_SQLITE_STATICALLY)
// When linking sqlite3 statically (e.g., as part of a larger project like OpenMS),
// use direct function calls instead of dlopen-based symbol lookup.
#include <sqlite3.h>

class ot_sqlite
{
public:
    static int sqlite3_open_v2(const char* s, sqlite3** ptr, int flags, const char*) { return ::sqlite3_open_v2(s, ptr, flags, NULL); }
    static int sqlite3_close(sqlite3* db) { return ::sqlite3_close(db); }
    static int sqlite3_exec(sqlite3* db, const char* query, int (*callback)(void*,int,char**,char**), void* arg, char **err) { return ::sqlite3_exec(db, query, callback, arg, err); }
    static void sqlite3_free(void* ptr) { ::sqlite3_free(ptr); }
    static const char* sqlite3_errmsg(sqlite3* db) { return ::sqlite3_errmsg(db); }
    static int sqlite3_busy_timeout(sqlite3* db, int ms) { return ::sqlite3_busy_timeout(db, ms); }
};

#else // dynamic loading via so_manager

#include <optional>
#include "so_manager.h"
#include "sqlite/sqlite3.h"
// Each wrapper resolves its symbol once, in a function-local static, whose
// initialisation is thread-safe: handles may be opened from several threads.
class ot_sqlite
{
public:
    static std::optional<LoadedLibraryHandle> sqlite_so_handle;
    static int sqlite3_open_v2(const char* s, sqlite3** ptr, int flags, const char*)
    {
        static decltype(::sqlite3_open_v2)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_open_v2)>("sqlite3_open_v2");
        return fun(s, ptr, flags, NULL);
    }
    static int sqlite3_close(sqlite3* db)
    {
        static decltype(::sqlite3_close)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_close)>("sqlite3_close");
        return fun(db);
    }
    static int sqlite3_exec(sqlite3* db, const char* query, int (*callback)(void*,int,char**,char**), void* arg, char **err)
    {
        static decltype(::sqlite3_exec)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_exec)>("sqlite3_exec");
        return fun(db, query, callback, arg, err);
    }
    static void sqlite3_free(void* ptr)
    {
        static decltype(::sqlite3_free)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_free)>("sqlite3_free");
        fun(ptr);
    }
    static const char* sqlite3_errmsg(sqlite3* db)
    {
        static decltype(::sqlite3_errmsg)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_errmsg)>("sqlite3_errmsg");
        return fun(db);
    }
    static int sqlite3_busy_timeout(sqlite3* db, int ms)
    {
        static decltype(::sqlite3_busy_timeout)* const fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_busy_timeout)>("sqlite3_busy_timeout");
        return fun(db, ms);
    }

};
#endif // ot_sqlite implementation selector

class RAIISqlite
{
    sqlite3* db_conn;

    // Exceptions must not unwind through sqlite3_exec(): it is C code (possibly
    // without unwind tables), and skipping its cleanup leaks the prepared
    // statement, which makes sqlite3_close() fail and leaks the connection.
    // Callbacks run inside this trampoline instead; an exception aborts the
    // query and is rethrown once sqlite3_exec() has returned.
    struct CallbackContext
    {
        int (*callback)(void*,int,char**,char**);
        void* arg;
        std::exception_ptr error;
    };

    static int callback_trampoline(void* ctx_ptr, int cols, char** row, char** colnames)
    {
        CallbackContext* ctx = static_cast<CallbackContext*>(ctx_ptr);
        try
        {
            return ctx->callback(ctx->arg, cols, row, colnames);
        }
        catch(...)
        {
            ctx->error = std::current_exception();
            return 1; // makes sqlite3_exec() abort with SQLITE_ABORT
        }
    }

 public:
    RAIISqlite(const std::string& tims_tdf_path) : db_conn(nullptr)
    {
        if(ot_sqlite::sqlite3_open_v2(tims_tdf_path.c_str(), &db_conn, SQLITE_OPEN_READONLY, NULL))
        {
            std::string err_msg = "ERROR opening database: " + tims_tdf_path + " SQLite error msg: ";
            const char* sqlite_msg = db_conn != nullptr ? ot_sqlite::sqlite3_errmsg(db_conn) : nullptr;
            err_msg += sqlite_msg != nullptr ? sqlite_msg : "out of memory";
            // The handle must be closed even when opening failed.
            if(db_conn != nullptr)
                ot_sqlite::sqlite3_close(db_conn);
            throw std::runtime_error(err_msg);
        }
        // Even read-only connections take file locks, and on Windows a reader
        // briefly holds the PENDING lock while acquiring SHARED, retrying only
        // a few times; readers opening the same file at once then fail with
        // SQLITE_BUSY ("database is locked"). Wait for each other instead.
        ot_sqlite::sqlite3_busy_timeout(db_conn, 5000);
    }
    ~RAIISqlite()
    {
        if(db_conn != nullptr)
            ot_sqlite::sqlite3_close(db_conn);
    }
    RAIISqlite(const RAIISqlite&) = delete;
    RAIISqlite& operator=(const RAIISqlite&) = delete;

    void query(const std::string& sql, int (*callback)(void*,int,char**,char**), void* arg)
    {
        char* error = NULL;
        CallbackContext ctx{callback, arg, nullptr};

        const int rc = ot_sqlite::sqlite3_exec(db_conn, sql.c_str(), callback != nullptr ? callback_trampoline : nullptr, &ctx, &error);
        std::string err_msg;
        if(rc != SQLITE_OK)
            err_msg = std::string("ERROR performing SQL query. SQLite error msg: ") + (error != NULL ? error : "unknown error");
        if(error != NULL)
            ot_sqlite::sqlite3_free(error);
        if(ctx.error)
            std::rethrow_exception(ctx.error);
        if(rc != SQLITE_OK)
            throw std::runtime_error(err_msg);
    }

};
