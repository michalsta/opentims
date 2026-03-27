/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include "converters.h"
#include "tof2mz_converter.h"
#include "scan2inv_ion_mobility_converter.h"
#include "thread_mgr.h"

void setup_bruker(const std::string& path)
{
    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(path.c_str());
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(path.c_str());
    BrukerThreadingManager::SetupBrukerThreading(path);
}

void setup_opensource()
{
    DefaultTof2MzConverterFactory::setAsDefault<OpenSourceTof2MzConverterFactory>();
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<OpenSourceScan2ImConverterFactory>();
}

extern "C"
{
void setup_bruker_c(const char* path)
{
    return setup_bruker(std::string(path));
}

void setup_opensource_c()
{
    setup_opensource();
}
}
