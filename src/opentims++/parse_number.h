/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <locale>
#include <sstream>

// Parse a decimal number written with '.' as the separator, as SQLite prints
// REAL values, regardless of the process locale (std::atof honours LC_NUMERIC,
// so e.g. "0.6" parses as 0 under pl_PL). Like atof, returns 0.0 on failure.
inline double parse_double_c_locale(const char* s)
{
    thread_local std::istringstream ss = []{
        std::istringstream ret;
        ret.imbue(std::locale::classic());
        return ret;
    }();
    ss.clear();
    ss.str(s);
    double ret = 0.0;
    ss >> ret;
    return ss.fail() ? 0.0 : ret;
}
