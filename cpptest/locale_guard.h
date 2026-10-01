/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <clocale>
#include <string>

namespace ottest {

// Switches LC_NUMERIC to a locale whose decimal separator is ',' and restores
// the previous one on destruction.
class LocaleGuard
{
    std::string saved;
 public:
    LocaleGuard()
    {
        const char* cur = std::setlocale(LC_NUMERIC, nullptr);
        saved = cur != nullptr ? cur : "C";
    }
    ~LocaleGuard() { std::setlocale(LC_NUMERIC, saved.c_str()); }
    LocaleGuard(const LocaleGuard&) = delete;
    LocaleGuard& operator=(const LocaleGuard&) = delete;

    bool set_comma_decimal_locale()
    {
        for(const char* name : {"pl_PL.UTF-8", "pl_PL.utf8", "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8",
                                "fr_FR.utf8", "German_Germany.1252", "Polish_Poland.1250", "de-DE", "pl-PL"})
            if(std::setlocale(LC_NUMERIC, name) != nullptr)
            {
                const std::lconv* lc = std::localeconv();
                if(lc != nullptr && lc->decimal_point != nullptr && std::string(lc->decimal_point) == ",")
                    return true;
            }
        std::setlocale(LC_NUMERIC, saved.c_str());
        return false;
    }
};

} // namespace ottest
