/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <gtest/gtest.h>

#include "converters.h"
#include "sqlite_helper.h"

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

#if !defined(OPENTIMS_LINK_SQLITE_STATICALLY)
    // The dlopen backend, as the Python module uses it, loading the bundled
    // sqlite built as a separate module.
    ot_sqlite::sqlite_so_handle.emplace(OPENTIMS_TEST_SQLITE_MODULE);
#endif

    // Tests that need other converters pass factories explicitly, or restore
    // this default when they change it.
    setup_opensource();

    return RUN_ALL_TESTS();
}
