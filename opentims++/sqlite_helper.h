#pragma once

#ifndef OPENTIMS_BUILDING_R
//#include "sqlite/sqlite3.h"

//#ifndef OPENTIMSPY_FAST_BUILD
//namespace ot_sqlite
//{
//    using ::sqlite3_open_v2;
//    using ::sqlite3_close;
//    using ::sqlite3_exec;
//    using ::sqlite3_free;
//    using ::sqlite3_errmsg;
//};
//#else
#include <optional>
class ot_sqlite
{
public:
    static std::optional<LoadedLibraryHandle> sqlite_so_handle;
    static int sqlite3_open_v2(const char* s, sqlite3** ptr, int flags, const char*)
    {
        static decltype(::sqlite3_open_v2)* fun = nullptr;
        if(fun == nullptr)
            fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_open_v2)>("sqlite3_open_v2");
        return fun(s, ptr, flags, NULL);
    }
    static int sqlite3_close(sqlite3* db)
    {
        static decltype(::sqlite3_close)* fun = nullptr;
        if(fun == nullptr)
            fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_close)>("sqlite3_close");
        return fun(db);
    }
    static int sqlite3_exec(sqlite3* db, const char* query, int (*callback)(void*,int,char**,char**), void* arg, char **err)
    {
        static decltype(::sqlite3_exec)* fun = nullptr;
        if(fun == nullptr)
            fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_exec)>("sqlite3_exec");
        return fun(db, query, callback, arg, err);
    }
    static void sqlite3_free(void* ptr)
    {
        static decltype(::sqlite3_free)* fun = nullptr;
        if(fun == nullptr)
            fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_free)>("sqlite3_free");
        fun(ptr);
    }
    static const char* sqlite3_errmsg(sqlite3* db)
    {
        static decltype(::sqlite3_errmsg)* fun = nullptr;
        if(fun == nullptr)
            fun = sqlite_so_handle.value().symbol_lookup<decltype(::sqlite3_errmsg)>("sqlite3_errmsg");
        return fun(db);
    }

};
#endif

class RAIISqlite
{
    sqlite3* db_conn;
#ifdef OPENTIMSPY_FAST_BUILD
    static std::optional<LoadedLibraryHandle> sqlite_so_handle;
#endif

 public:
    RAIISqlite(const std::string& tims_tdf_path) : db_conn(nullptr)
    {
        if(ot_sqlite::sqlite3_open_v2(tims_tdf_path.c_str(), &db_conn, SQLITE_OPEN_READONLY, NULL))
            throw std::runtime_error(std::string("ERROR opening database: " + tims_tdf_path + " SQLite error msg: ") + ot_sqlite::sqlite3_errmsg(db_conn));
    }
    ~RAIISqlite()
    {
        if(db_conn != nullptr)
            ot_sqlite::sqlite3_close(db_conn);
    }
    void query(const std::string& sql, int (*callback)(void*,int,char**,char**), void* arg)
    {
        char* error = NULL;

        if(ot_sqlite::sqlite3_exec(db_conn, sql.c_str(), callback, arg, &error) != SQLITE_OK)
        {
	    std::string err_msg(std::string("ERROR performing SQL query. SQLite error msg: ") + error);
	    ot_sqlite::sqlite3_free(error);
	    throw std::runtime_error(err_msg);
        }
    }

};


//#endif