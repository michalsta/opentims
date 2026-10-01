/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Opening datasets and the handle's metadata accessors.

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "tof2mz_converter.h"
#include "scan2inv_ion_mobility_converter.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

DatasetSpec small_spec()
{
    DatasetSpec spec;
    FrameSpec a;
    a.id = 1;
    a.time = 0.5;
    a.scans = {{{10, 5}, {11, 6}}, {}, {{20, 7}}};
    FrameSpec b;
    b.id = 2;
    b.time = 1.5;
    b.msms_type = 9;
    b.scans = {{}, {{30, 8}}};
    spec.frames = {a, b};
    return spec;
}

} // anonymous namespace

TEST(Open, BundledTestDataset)
{
    TimsDataHandle h(test_d().string());
    EXPECT_EQ(h.min_frame_id(), 1u);
    EXPECT_EQ(h.max_frame_id(), 2u);
    EXPECT_EQ(h.get_frame_descs().size(), 2u);
    EXPECT_TRUE(h.has_frame(1));
    EXPECT_TRUE(h.has_frame(2));
    EXPECT_FALSE(h.has_frame(0));
    EXPECT_FALSE(h.has_frame(3));
    EXPECT_EQ(h.no_peaks_total(), 10u);
    EXPECT_EQ(h.max_peaks_in_frame(), 5u);
    EXPECT_EQ(h.get_tims_dir_path(), test_d().string());

    const TimsFrame& f1 = h.get_frame(1);
    EXPECT_EQ(f1.id, 1u);
    EXPECT_EQ(f1.num_scans, 918u);
    EXPECT_EQ(f1.num_peaks, 5u);
    EXPECT_EQ(f1.msms_type, 0u);
    EXPECT_DOUBLE_EQ(f1.time, 0.644238);
    EXPECT_DOUBLE_EQ(f1.intensity_correction, 1.0);
    EXPECT_EQ(f1.data_size_bytes(), 4u * (918 + 2 * 5));
    EXPECT_EQ(h.get_frame(2).msms_type, 9u);
    EXPECT_EQ(h.get_decomp_buffer_size(), 4u * (918 + 2 * 5));
}

TEST(Open, SyntheticDatasetMetadata)
{
    const DatasetSpec spec = small_spec();
    TempDir tmp;
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());
    EXPECT_EQ(h.min_frame_id(), 1u);
    EXPECT_EQ(h.max_frame_id(), 2u);
    EXPECT_EQ(h.no_peaks_total(), 4u);
    EXPECT_EQ(h.max_peaks_in_frame(), 3u);
    EXPECT_EQ(h.get_frame(1).num_scans, 3u);
    EXPECT_EQ(h.get_frame(1).num_peaks, 3u);
    EXPECT_EQ(h.get_frame(2).num_scans, 2u);
    EXPECT_EQ(h.get_frame(2).msms_type, 9u);
    EXPECT_DOUBLE_EQ(h.get_frame(2).time, 1.5);
    EXPECT_EQ(h.get_decomp_buffer_size(), 4u * (3 + 2 * 3));
}

TEST(Open, PeakCounts)
{
    TempDir tmp;
    write_dataset(random_dataset(11, 20, 30, 10), tmp.path());
    TimsDataHandle h(tmp.path().string());
    size_t total = 0;
    size_t biggest = 0;
    for(uint32_t id = 1; id <= 20; id++)
    {
        total += h.get_frame(id).num_peaks;
        biggest = std::max<size_t>(biggest, h.get_frame(id).num_peaks);
    }
    EXPECT_EQ(h.no_peaks_total(), total);
    EXPECT_EQ(h.max_peaks_in_frame(), biggest);
    std::vector<uint32_t> ids = {3, 3, 7, 1};
    EXPECT_EQ(h.no_peaks_in_frames(ids), 2 * h.get_frame(3).num_peaks + h.get_frame(7).num_peaks + h.get_frame(1).num_peaks);
    EXPECT_EQ(h.no_peaks_in_frames(std::vector<uint32_t>{}), 0u);
    EXPECT_EQ(h.no_peaks_in_slice(1, 21, 1), total);
    EXPECT_EQ(h.no_peaks_in_slice(2, 12, 5), h.get_frame(2).num_peaks + h.get_frame(7).num_peaks);
    EXPECT_EQ(h.no_peaks_in_slice(5, 5, 1), 0u);
    EXPECT_EQ(h.no_peaks_in_slice(9, 2, 1), 0u);
    EXPECT_THROW(h.no_peaks_in_slice(1, 5, 0), std::runtime_error);
}

TEST(Open, GappedIdsAreKeptAsIs)
{
    TempDir tmp;
    const DatasetSpec spec = random_dataset(3, 6, 8, 5, /*first_id=*/1000, /*id_stride=*/7);
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());
    EXPECT_EQ(h.min_frame_id(), 1000u);
    EXPECT_EQ(h.max_frame_id(), 1035u);
    EXPECT_TRUE(h.has_frame(1007));
    EXPECT_FALSE(h.has_frame(1001));
    EXPECT_THROW(h.get_frame(1001), std::out_of_range);
    EXPECT_THROW(h.no_peaks_in_frames(std::vector<uint32_t>{1000, 1001}), std::out_of_range);
    EXPECT_THROW(h.no_peaks_in_slice(1000, 1036, 1), std::out_of_range);
    EXPECT_NO_THROW(h.no_peaks_in_slice(1000, 1036, 7));
}

TEST(Open, EmptyFramesTable)
{
    TempDir tmp;
    write_dataset(DatasetSpec{}, tmp.path());
    // An empty analysis.tdf_bin cannot be mapped (see EmptyTdfBin below).
    {
        std::ofstream out(tmp / "analysis.tdf_bin", std::ios::binary);
        out << "padding";
    }
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation,
                     &ErrorTof2MzConverterFactory::instance(), &ErrorScan2InvIonMobilityConverterFactory::instance());
    EXPECT_EQ(h.no_peaks_total(), 0u);
    EXPECT_EQ(h.max_peaks_in_frame(), 0u);
    EXPECT_EQ(h.get_frame_descs().size(), 0u);
    EXPECT_GT(h.min_frame_id(), h.max_frame_id()) << "no valid range";
    uint32_t sentinel = 0xDEADBEEF;
    h.per_frame_TIC(&sentinel);
    EXPECT_EQ(sentinel, 0xDEADBEEFu) << "nothing to write for an empty dataset";
}

TEST(Open, MissingDirectoryThrows)
{
    TempDir tmp;
    EXPECT_THROW(TimsDataHandle((tmp / "nope.d").string()), std::runtime_error);
    EXPECT_FALSE(fs::exists(tmp / "nope.d"));
}

TEST(Open, MissingTdfThrowsAndDoesNotCreateIt)
{
    DatasetSpec spec = small_spec();
    spec.write_tdf = false;
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::runtime_error);
    EXPECT_FALSE(fs::exists(tmp / "analysis.tdf"));
}

TEST(Open, MissingTdfBinThrows)
{
    DatasetSpec spec = small_spec();
    spec.write_tdf_bin = false;
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::runtime_error);
}

TEST(Open, EmptyTdfBinThrows)
{
    // mio cannot map an empty file, so this fails at open time, with the OS's
    // terse "Invalid argument". A dataset without frames has an empty
    // tdf_bin, so it cannot be opened either.
    TempDir tmp;
    write_dataset(small_spec(), tmp.path());
    fs::resize_file(tmp / "analysis.tdf_bin", 0);
    EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::system_error);
}

TEST(Open, UnsupportedCompressionTypeThrows)
{
    DatasetSpec spec = small_spec();
    spec.metadata["TimsCompressionType"] = "1";
    TempDir tmp;
    write_dataset(spec, tmp.path());
    try
    {
        TimsDataHandle h(tmp.path().string());
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("Compression algorithm"), std::string::npos) << e.what();
    }
}

TEST(Open, NullCompressionTypeThrows)
{
    DatasetSpec spec = small_spec();
    spec.extra_sql = {"UPDATE GlobalMetadata SET Value = NULL WHERE Key = 'TimsCompressionType'"};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    try
    {
        TimsDataHandle h(tmp.path().string());
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("TimsCompressionType"), std::string::npos) << e.what();
    }
}

TEST(Open, AbsentCompressionTypeIsAccepted)
{
    DatasetSpec spec = small_spec();
    spec.metadata.erase("TimsCompressionType");
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_NO_THROW(TimsDataHandle(tmp.path().string()));
}

TEST(Open, MissingFramesTableThrows)
{
    DatasetSpec spec = small_spec();
    spec.extra_sql = {"DROP TABLE Frames"};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::runtime_error);
}

TEST(Open, MissingFramesColumnThrows)
{
    DatasetSpec spec = small_spec();
    spec.extra_sql = {"ALTER TABLE Frames RENAME COLUMN TimsId TO Something"};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::runtime_error);
}

TEST(Open, MissingGlobalMetadataTableThrows)
{
    DatasetSpec spec = small_spec();
    spec.extra_sql = {"DROP TABLE GlobalMetadata"};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(TimsDataHandle(tmp.path().string(), NoPressureCompensation,
                                &ErrorTof2MzConverterFactory::instance(),
                                &ErrorScan2InvIonMobilityConverterFactory::instance()),
                 std::runtime_error);
}

class NullFramesColumn : public ::testing::TestWithParam<std::string> {};

TEST_P(NullFramesColumn, Throws)
{
    DatasetSpec spec = small_spec();
    spec.extra_sql = {"UPDATE Frames SET " + GetParam() + " = NULL WHERE Id = 2"};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    try
    {
        TimsDataHandle h(tmp.path().string());
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("null value"), std::string::npos) << e.what();
    }
}

INSTANTIATE_TEST_SUITE_P(Columns, NullFramesColumn,
                         ::testing::Values("NumScans", "NumPeaks", "MsMsType", "AccumulationTime", "Time", "TimsId"));

TEST(Open, IntensityCorrectionFromAccumulationTime)
{
    DatasetSpec spec;
    FrameSpec f;
    f.id = 1;
    f.accumulation_time = 50.0;
    f.scans = {{{1, 1}}};
    spec.frames = {f};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());
    EXPECT_DOUBLE_EQ(h.get_frame(1).intensity_correction, 2.0);
}

TEST(Open, DatasetPathWithSpacesAndUnicode)
{
#if defined(OPENTIMS_WINDOWS)
    GTEST_SKIP() << "the handle takes a narrow, code-page std::string path";
#endif
    TempDir tmp;
    const fs::path dir = tmp.path() / fs::path(u8"my data éł.d");
    write_dataset(small_spec(), dir);
    TimsDataHandle h(dir.string());
    EXPECT_EQ(h.no_peaks_total(), 4u);
}

TEST(Open, ManyHandlesOpenAndClose)
{
    TempDir tmp;
    write_dataset(small_spec(), tmp.path());
    std::vector<std::unique_ptr<TimsDataHandle>> handles;
    for(int i = 0; i < 64; i++)
        handles.push_back(std::make_unique<TimsDataHandle>(tmp.path().string()));
    for(auto& h : handles)
        EXPECT_EQ(h->no_peaks_total(), 4u);
}

TEST(Open, FailedOpensDoNotLeak)
{
    // LeakSanitizer/Valgrind would flag connections, maps or frames left behind.
    DatasetSpec spec = small_spec();
    spec.metadata["TimsCompressionType"] = "3";
    TempDir tmp;
    write_dataset(spec, tmp.path());
    for(int i = 0; i < 100; i++)
        EXPECT_THROW(TimsDataHandle(tmp.path().string()), std::runtime_error);
}
