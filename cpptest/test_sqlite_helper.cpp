/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "sqlite_helper.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

using Rows = std::vector<std::vector<std::string>>;

int collect_rows(void* out, int cols, char** row, char**)
{
    std::vector<std::string> r;
    for(int i = 0; i < cols; i++)
        r.push_back(row[i] != nullptr ? row[i] : "<NULL>");
    static_cast<Rows*>(out)->push_back(std::move(r));
    return 0;
}

int throwing_callback(void* counter, int, char**, char**)
{
    ++*static_cast<int*>(counter);
    throw std::invalid_argument("from callback");
}

int aborting_callback(void* counter, int, char**, char**)
{
    ++*static_cast<int*>(counter);
    return 1;
}

class SqliteHelper : public ::testing::Test
{
 protected:
    TempDir tmp;
    fs::path db;
    void SetUp() override
    {
        db = tmp / "x.sqlite";
        exec_sql(db, "CREATE TABLE t (k INTEGER, v TEXT); "
                     "INSERT INTO t VALUES (1, 'one'), (2, NULL), (3, 'three');");
    }
};

} // anonymous namespace

TEST_F(SqliteHelper, QueryReturnsAllRows)
{
    RAIISqlite conn(db.string());
    Rows rows;
    conn.query("SELECT k, v FROM t ORDER BY k", collect_rows, &rows);
    ASSERT_EQ(rows.size(), 3u);
    EXPECT_EQ(rows[0], (std::vector<std::string>{"1", "one"}));
    EXPECT_EQ(rows[1], (std::vector<std::string>{"2", "<NULL>"}));
    EXPECT_EQ(rows[2], (std::vector<std::string>{"3", "three"}));
}

TEST_F(SqliteHelper, NullCallbackRunsStatement)
{
    RAIISqlite conn(db.string());
    EXPECT_NO_THROW(conn.query("SELECT 1", nullptr, nullptr));
}

TEST_F(SqliteHelper, OpensReadOnly)
{
    RAIISqlite conn(db.string());
    EXPECT_THROW(conn.query("INSERT INTO t VALUES (4, 'four')", nullptr, nullptr), std::runtime_error);
}

TEST_F(SqliteHelper, SyntaxErrorThrowsWithMessage)
{
    RAIISqlite conn(db.string());
    try
    {
        conn.query("SELEKT nonsense", nullptr, nullptr);
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("SQLite error msg"), std::string::npos) << e.what();
    }
}

TEST_F(SqliteHelper, MissingTableThrows)
{
    RAIISqlite conn(db.string());
    Rows rows;
    EXPECT_THROW(conn.query("SELECT * FROM no_such_table", collect_rows, &rows), std::runtime_error);
}

TEST_F(SqliteHelper, CallbackExceptionPropagatesWithItsType)
{
    RAIISqlite conn(db.string());
    int calls = 0;
    EXPECT_THROW(conn.query("SELECT k FROM t", throwing_callback, &calls), std::invalid_argument);
    EXPECT_EQ(calls, 1) << "the query must stop at the first exception";
    // The connection stays usable after an aborted query.
    Rows rows;
    conn.query("SELECT COUNT(*) FROM t", collect_rows, &rows);
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0][0], "3");
}

TEST_F(SqliteHelper, CallbackAbortThrows)
{
    RAIISqlite conn(db.string());
    int calls = 0;
    EXPECT_THROW(conn.query("SELECT k FROM t", aborting_callback, &calls), std::runtime_error);
    EXPECT_EQ(calls, 1);
}

TEST_F(SqliteHelper, ManyConnectionsDoNotLeak)
{
    // Under LeakSanitizer/Valgrind, a leaked connection or statement shows up here.
    for(int i = 0; i < 200; i++)
    {
        RAIISqlite conn(db.string());
        int calls = 0;
        try { conn.query("SELECT k FROM t", throwing_callback, &calls); }
        catch(const std::invalid_argument&) {}
    }
}

TEST(SqliteHelperOpen, MissingFileThrowsAndCreatesNothing)
{
    TempDir tmp;
    const fs::path missing = tmp / "missing.sqlite";
    try
    {
        RAIISqlite conn(missing.string());
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find(missing.string()), std::string::npos) << e.what();
    }
    EXPECT_FALSE(fs::exists(missing));
}

TEST(SqliteHelperOpen, NotADatabaseThrowsOnQuery)
{
    TempDir tmp;
    const fs::path junk = tmp / "junk.sqlite";
    {
        std::ofstream out(junk, std::ios::binary);
        out << std::string(4096, 'x');
    }
    // sqlite opens lazily; the error surfaces on first use at the latest.
    EXPECT_THROW({
        RAIISqlite conn(junk.string());
        conn.query("SELECT * FROM sqlite_master", nullptr, nullptr);
    }, std::runtime_error);
}

TEST(SqliteHelperOpen, DirectoryInsteadOfFileThrows)
{
    TempDir tmp;
    EXPECT_THROW({
        RAIISqlite conn(tmp.path().string());
        conn.query("SELECT * FROM sqlite_master", nullptr, nullptr);
    }, std::runtime_error);
}
