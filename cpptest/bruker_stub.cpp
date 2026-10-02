/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// A minimal timsdata library for exercising the real dlopen/conversion path.
// Frame 1 succeeds; all other frames fail without touching the output.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "bruker_api.h"

#ifdef _WIN32
#define STUB_EXPORT extern "C" __declspec(dllexport)
#else
#define STUB_EXPORT extern "C" __attribute__((visibility("default")))
#endif

STUB_EXPORT uint64_t tims_open_v2(const char*, uint32_t, pressure_compensation_strategy)
{
    return 1;
}

STUB_EXPORT void tims_close(uint64_t) {}
STUB_EXPORT void tims_set_num_threads(uint32_t) {}

STUB_EXPORT uint32_t tims_get_last_error_string(char* target, uint32_t length)
{
    constexpr char error[] = "Calibration unavailable for requested frame";
    if(target && length)
    {
        const auto n = (std::min)(size_t(length - 1), sizeof(error) - 1);
        std::memcpy(target, error, n);
        target[n] = '\0';
    }
    return sizeof(error);
}

static uint32_t convert(int64_t frame, const double* input, double* output, uint32_t size)
{
    if(frame != 1)
        return 0;
    for(uint32_t i = 0; i < size; ++i)
        output[i] = input[i] + 1;
    return 1;
}

STUB_EXPORT uint32_t tims_index_to_mz(uint64_t, int64_t frame, const double* in, double* out, uint32_t n)
{
    return convert(frame, in, out, n);
}

STUB_EXPORT uint32_t tims_mz_to_index(uint64_t, int64_t frame, const double* in, double* out, uint32_t n)
{
    return convert(frame, in, out, n);
}

STUB_EXPORT uint32_t tims_scannum_to_oneoverk0(uint64_t, int64_t frame, const double* in, double* out, uint32_t n)
{
    return convert(frame, in, out, n);
}

STUB_EXPORT uint32_t tims_oneoverk0_to_scannum(uint64_t, int64_t frame, const double* in, double* out, uint32_t n)
{
    return convert(frame, in, out, n);
}
