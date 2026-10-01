/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Builds synthetic TDF datasets (analysis.tdf + analysis.tdf_bin) at test time,
// so the C++ tests can cover shapes the bundled pytest/test.d does not have
// without shipping more data files.
//
// Frames are stored as zstd frames made of raw (stored) blocks. That is valid
// zstd which any decoder must accept, and needs no compressor; the real
// compressed path is covered by pytest/test.d.

#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace ottest {

struct Peak
{
    uint32_t tof;
    uint32_t intensity; // raw, before the AccumulationTime correction
};

struct FrameSpec
{
    uint32_t id = 1;
    double time = 1.0;
    uint32_t msms_type = 0;
    double accumulation_time = 100.0; // 100 makes the intensity correction exactly 1
    std::vector<std::vector<Peak>> scans; // scans.size() is NumScans

    // Corruption knobs; each replaces what would be written for this frame.
    std::optional<std::vector<uint32_t>> words;  // decompressed uint32 words
    std::optional<std::string> packet;           // whole tdf_bin packet, verbatim
    std::optional<std::string> sql_num_scans;    // Frames.NumScans, as an SQL literal
    std::optional<std::string> sql_num_peaks;    // Frames.NumPeaks, as an SQL literal
    std::optional<std::string> sql_tims_id;      // Frames.TimsId, as an SQL literal
    std::optional<std::string> sql_accumulation_time;

    uint32_t num_scans() const { return static_cast<uint32_t>(scans.size()); }
    uint32_t num_peaks() const;
};

struct DatasetSpec
{
    std::vector<FrameSpec> frames;
    std::map<std::string, std::string> metadata = default_metadata();
    std::vector<std::string> extra_sql; // run after the tables are filled
    bool write_tdf = true;
    bool write_tdf_bin = true;

    static std::map<std::string, std::string> default_metadata();
};

// Calibration that default_metadata() describes.
constexpr double kMzMin = 50.0;
constexpr double kMzMax = 1700.0;
constexpr uint32_t kTofMax = 434064;
constexpr double kImMin = 0.6;
constexpr double kImMax = 1.6;

// Decompressed frame layout: NumScans header words (word s+1 holds twice the
// peak count of scan s; word 0 and the last scan's count are implicit), then a
// (tof delta, intensity) word pair per peak.
std::vector<uint32_t> frame_words(const FrameSpec& frame);

// The four byte planes the format stores a frame's words as.
std::string byte_planes(const std::vector<uint32_t>& words);

// A zstd frame with the given content, made of raw blocks.
std::string zstd_raw_frame(const std::string& content);

// A tdf_bin packet: u32 packet size, u32 NumScans, zstd frame.
std::string frame_packet(uint32_t num_scans, const std::string& zstd_frame);

std::string sql_double(double x);

// Writes `spec` into `dir` (created if needed) and returns `dir`.
fs::path write_dataset(const DatasetSpec& spec, const fs::path& dir);

// Executes SQL against an existing analysis.tdf (read-write).
void exec_sql(const fs::path& tdf, const std::string& sql);

class TempDir
{
    fs::path path_;
 public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }
    fs::path operator/(const std::string& sub) const { return path_ / sub; }
};

// What extraction must produce, column by column.
struct Columns
{
    std::vector<uint32_t> frame, scan, tof, intensity;
    std::vector<double> mz, inv_ion_mobility, retention_time;

    size_t size() const { return frame.size(); }
    void resize(size_t n);
};

uint32_t corrected_intensity(uint32_t raw, double accumulation_time);
double expected_mz(uint32_t tof);
double expected_inv_ion_mobility(uint32_t scan, uint32_t max_num_scans);

// Expected columns for the given frame ids (in that order), assuming the
// open-source converters with default_metadata() calibration.
Columns expected_columns(const DatasetSpec& spec, const std::vector<uint32_t>& ids);

// A dataset of `n_frames` frames with seeded random contents, ids starting at
// `first_id` and advancing by `id_stride`. Includes empty scans, and every
// `empty_every`-th frame has no peaks at all (0 disables).
DatasetSpec random_dataset(uint32_t seed, uint32_t n_frames, uint32_t max_scans,
                           uint32_t max_peaks_per_scan, uint32_t first_id = 1,
                           uint32_t id_stride = 1, uint32_t empty_every = 5);

// Path of the bundled pytest/test.d.
fs::path test_d();

} // namespace ottest
