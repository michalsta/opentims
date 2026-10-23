/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Per-frame extraction of repeated frames, where each request asks for a
// different subset of columns, on pytest/test.d (real compressed frames).

#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "thread_mgr.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

struct ThreadCountGuard
{
    ~ThreadCountGuard() { ThreadingManager::get_instance().set_num_threads(0); }
};

// Output buffers for one request. Bit col of mask selects column col
// (frame, scan, tof, intensity, mz, inv_ion_mobility, retention_time).
struct RequestColumns
{
    std::array<std::vector<uint32_t>, 4> raw;
    std::array<std::vector<double>, 3> converted;

    RequestColumns(size_t size, unsigned mask = 127)
    {
        for(size_t col = 0; col < raw.size(); ++col)
            if(mask & (1u << col))
                raw[col].assign(size, 0xdeadbeef);
        for(size_t col = 0; col < converted.size(); ++col)
            if(mask & (1u << (col + raw.size())))
                converted[col].assign(size, -1.0);
    }

    bool matches(const RequestColumns& expected) const
    {
        for(size_t col = 0; col < raw.size(); ++col)
            if(!raw[col].empty() && raw[col] != expected.raw[col])
                return false;
        for(size_t col = 0; col < converted.size(); ++col)
            if(!converted[col].empty() && converted[col] != expected.converted[col])
                return false;
        return true;
    }
};

template<typename T> T* data_or_null(std::vector<T>& column)
{
    return column.empty() ? nullptr : column.data();
}

// Interleave two frames to exercise both task ordering and parallel decoding.
// Later requests supply the columns omitted by the first requests, followed
// by full requests that must receive copies from both sets of buffers.
bool requests_match(TimsDataHandle& data, const std::array<RequestColumns, 2>& expected, unsigned first_mask)
{
    const std::vector<uint32_t> ids{2, 1, 2, 1, 2, 1};
    std::vector<RequestColumns> outputs;
    for(size_t ii = 0; ii < ids.size(); ++ii)
        outputs.emplace_back(data.get_frame(ids[ii]).num_peaks,
                             ii < 2 ? first_mask : ii < 4 ? (127 ^ first_mask) : 127);

    std::array<std::vector<uint32_t*>, 4> raw;
    std::array<std::vector<double*>, 3> converted;
    for(auto& output : outputs)
    {
        for(size_t col = 0; col < raw.size(); ++col)
            raw[col].push_back(data_or_null(output.raw[col]));
        for(size_t col = 0; col < converted.size(); ++col)
            converted[col].push_back(data_or_null(output.converted[col]));
    }

    data.extract_frames(ids, raw[0].data(), raw[1].data(), raw[2].data(), raw[3].data(),
                        converted[0].data(), converted[1].data(), converted[2].data());
    for(size_t ii = 0; ii < ids.size(); ++ii)
        if(!outputs[ii].matches(expected[ids[ii] - 1]))
            return false;
    return true;
}

} // anonymous namespace

TEST(RepeatedFrames, EveryColumnSplitAcrossRepeatedRequests)
{
    ThreadCountGuard guard;
    TimsDataHandle data(test_d().string());
    std::array<RequestColumns, 2> expected{RequestColumns(data.get_frame(1).num_peaks),
                                           RequestColumns(data.get_frame(2).num_peaks)};
    for(size_t ii = 0; ii < expected.size(); ++ii)
    {
        auto& columns = expected[ii];
        data.get_frame(ii + 1).save_to_buffs(
            columns.raw[0].data(), columns.raw[1].data(), columns.raw[2].data(),
            columns.raw[3].data(), columns.converted[0].data(),
            columns.converted[1].data(), columns.converted[2].data());
    }
    for(size_t threads : {1, 4})
    {
        ThreadingManager::get_instance().set_num_threads(threads);
        for(unsigned mask = 0; mask < 128; ++mask)
            EXPECT_TRUE(requests_match(data, expected, mask)) << "threads " << threads << ", mask " << mask;
    }
}
