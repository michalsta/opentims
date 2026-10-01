/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <clocale>
#include <cstdlib>
#include <locale>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "parse_number.h"
#include "locale_guard.h"

TEST(ParseDouble, PlainValues)
{
    EXPECT_EQ(parse_double_c_locale("0"), 0.0);
    EXPECT_EQ(parse_double_c_locale("1"), 1.0);
    EXPECT_EQ(parse_double_c_locale("-2.5"), -2.5);
    EXPECT_EQ(parse_double_c_locale("0.644238"), 0.644238);
    EXPECT_EQ(parse_double_c_locale("1700.000000"), 1700.0);
    EXPECT_EQ(parse_double_c_locale("1e3"), 1000.0);
    EXPECT_EQ(parse_double_c_locale("2.6651259287054483"), 2.6651259287054483);
    EXPECT_EQ(parse_double_c_locale("   7.25"), 7.25); // leading whitespace, as atof
}

TEST(ParseDouble, FailureGivesZeroLikeAtof)
{
    EXPECT_EQ(parse_double_c_locale(""), 0.0);
    EXPECT_EQ(parse_double_c_locale("abc"), 0.0);
    EXPECT_EQ(parse_double_c_locale("-"), 0.0);
}

TEST(ParseDouble, StopsAtTrailingGarbage)
{
    EXPECT_EQ(parse_double_c_locale("3.5xyz"), 3.5);
    EXPECT_EQ(parse_double_c_locale("3,5"), 3.0); // ',' is never a decimal separator here
    // Letters that may continue a hex float must not spoil the parse (libc++).
    EXPECT_EQ(parse_double_c_locale("1.5abc"), 1.5);
    EXPECT_EQ(parse_double_c_locale("0x10"), 0.0);
    EXPECT_EQ(parse_double_c_locale("2.5e"), 2.5);
    EXPECT_EQ(parse_double_c_locale("7e+"), 7.0);
    EXPECT_EQ(parse_double_c_locale("7e-1x"), 0.7);
    EXPECT_EQ(parse_double_c_locale(".5"), 0.5);
    EXPECT_EQ(parse_double_c_locale("5."), 5.0);
    EXPECT_EQ(parse_double_c_locale("."), 0.0);
}

TEST(ParseDouble, StateDoesNotLeakBetweenCalls)
{
    // The parser reuses a thread_local stream; a failed parse must not poison the next.
    EXPECT_EQ(parse_double_c_locale("junk"), 0.0);
    EXPECT_EQ(parse_double_c_locale("4.5"), 4.5);
    EXPECT_EQ(parse_double_c_locale("1.25 9"), 1.25);
    EXPECT_EQ(parse_double_c_locale("9.75"), 9.75);
}

TEST(ParseDouble, IgnoresCommaDecimalLocale)
{
    ottest::LocaleGuard guard;
    if(!guard.set_comma_decimal_locale())
        GTEST_SKIP() << "no comma-decimal locale installed";
    ASSERT_EQ(std::atof("0.5"), 0.0) << "locale switch did not take effect";
    EXPECT_EQ(parse_double_c_locale("0.644238"), 0.644238);
    EXPECT_EQ(parse_double_c_locale("1700.000000"), 1700.0);
}

TEST(ParseDouble, ConcurrentCallsAreIndependent)
{
    std::vector<std::thread> threads;
    std::vector<int> failures(8, 0);
    for(int t = 0; t < 8; t++)
        threads.emplace_back([t, &failures]() {
            for(int i = 0; i < 2000; i++)
            {
                const double expected = t * 1000.0 + i + 0.25;
                const std::string text = std::to_string(t * 1000 + i) + ".25";
                if(parse_double_c_locale(text.c_str()) != expected)
                    failures[t]++;
            }
        });
    for(auto& th : threads)
        th.join();
    for(int t = 0; t < 8; t++)
        EXPECT_EQ(failures[t], 0) << "thread " << t;
}
