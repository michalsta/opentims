/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <locale>
#include <sstream>
#include <string>

// Parse a decimal number written with '.' as the separator, as SQLite prints
// REAL values, regardless of the process locale (std::atof honours LC_NUMERIC,
// so e.g. "0.6" parses as 0 under pl_PL). Like atof, returns 0.0 on failure
// and ignores anything after the longest valid prefix.
inline double parse_double_c_locale(const char* s)
{
    // Only the numeric prefix is handed to the stream: libc++'s num_get
    // swallows every character that could belong to a hex float (x, a-f, p,
    // ...) and then fails the whole parse, so "3.5xyz" would come out as 0.
    while(*s == ' ' || (*s >= '\t' && *s <= '\r'))
        s++;
    auto is_digit = [](char c) { return c >= '0' && c <= '9'; };
    const char* p = s;
    if(*p == '+' || *p == '-')
        p++;
    const char* digits = p;
    while(is_digit(*p))
        p++;
    bool any_digits = p != digits;
    if(*p == '.')
    {
        p++;
        const char* frac = p;
        while(is_digit(*p))
            p++;
        any_digits = any_digits || p != frac;
    }
    if(!any_digits)
        return 0.0;
    if(*p == 'e' || *p == 'E')
    {
        const char* e = p + 1;
        if(*e == '+' || *e == '-')
            e++;
        if(is_digit(*e))
        {
            while(is_digit(*e))
                e++;
            p = e;
        }
    }

    thread_local std::istringstream ss = []{
        std::istringstream ret;
        ret.imbue(std::locale::classic());
        return ret;
    }();
    ss.clear();
    ss.str(std::string(s, p));
    double ret = 0.0;
    ss >> ret;
    return ss.fail() ? 0.0 : ret;
}
