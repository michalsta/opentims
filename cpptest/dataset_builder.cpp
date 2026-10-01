/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include "dataset_builder.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <locale>
#include <random>
#include <sstream>
#include <stdexcept>

#include "sqlite_helper.h"

namespace ottest {

uint32_t FrameSpec::num_peaks() const
{
    size_t n = 0;
    for(const auto& scan : scans)
        n += scan.size();
    return static_cast<uint32_t>(n);
}

std::map<std::string, std::string> DatasetSpec::default_metadata()
{
    return {
        {"SchemaType", "TDF"},
        {"TimsCompressionType", "2"},
        {"MzAcqRangeLower", "50.000000"},
        {"MzAcqRangeUpper", "1700.000000"},
        {"DigitizerNumSamples", "434064"},
        {"OneOverK0AcqRangeLower", "0.600000"},
        {"OneOverK0AcqRangeUpper", "1.600000"},
        {"AcquisitionSoftware", "timsTOF"},
    };
}

std::vector<uint32_t> frame_words(const FrameSpec& frame)
{
    if(frame.words)
        return *frame.words;

    const uint32_t num_scans = frame.num_scans();
    std::vector<uint32_t> words(num_scans, 0);
    for(uint32_t s = 0; s + 1 < num_scans; s++)
        words[s + 1] = 2 * static_cast<uint32_t>(frame.scans[s].size());
    for(const auto& scan : frame.scans)
    {
        uint32_t prev = static_cast<uint32_t>(-1); // the format's 1-based deltas
        for(const Peak& p : scan)
        {
            words.push_back(p.tof - prev);
            words.push_back(p.intensity);
            prev = p.tof;
        }
    }
    return words;
}

std::string byte_planes(const std::vector<uint32_t>& words)
{
    const size_t n = words.size();
    std::string out(4 * n, '\0');
    for(size_t i = 0; i < n; i++)
        for(size_t b = 0; b < 4; b++)
            out[b * n + i] = static_cast<char>((words[i] >> (8 * b)) & 0xFF);
    return out;
}

static void put_le(std::string& out, uint64_t value, size_t bytes)
{
    for(size_t i = 0; i < bytes; i++)
        out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
}

std::string zstd_raw_frame(const std::string& content)
{
    std::string out;
    put_le(out, 0xFD2FB528u, 4);     // magic
    out.push_back(char(0xA0));       // FCS field: 4 bytes; single segment; no checksum
    put_le(out, content.size(), 4);  // Frame_Content_Size
    const size_t max_block = 128 * 1024;
    size_t pos = 0;
    do
    {
        const size_t len = std::min(max_block, content.size() - pos);
        const bool last = pos + len == content.size();
        put_le(out, (uint32_t(len) << 3) | (0u << 1) | (last ? 1u : 0u), 3); // Raw_Block
        out.append(content, pos, len);
        pos += len;
    } while(pos < content.size());
    return out;
}

std::string frame_packet(uint32_t num_scans, const std::string& zstd_frame)
{
    std::string out;
    put_le(out, 8 + zstd_frame.size(), 4);
    put_le(out, num_scans, 4);
    out += zstd_frame;
    return out;
}

std::string sql_double(double x)
{
    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    ss.precision(17);
    ss << x;
    return ss.str();
}

static std::string sql_quote(const std::string& s)
{
    std::string out = "'";
    for(char c : s)
    {
        if(c == '\'')
            out += "''";
        else
            out += c;
    }
    return out + "'";
}

namespace {
class WritableDb
{
    sqlite3* db = nullptr;
 public:
    explicit WritableDb(const fs::path& path)
    {
        if(ot_sqlite::sqlite3_open_v2(path.string().c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        {
            if(db != nullptr)
                ot_sqlite::sqlite3_close(db);
            throw std::runtime_error("cannot create " + path.string());
        }
    }
    ~WritableDb() { ot_sqlite::sqlite3_close(db); }
    void exec(const std::string& sql)
    {
        char* err = nullptr;
        if(ot_sqlite::sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK)
        {
            std::string msg = "SQL failed: " + sql + ": " + (err != nullptr ? err : "?");
            ot_sqlite::sqlite3_free(err);
            throw std::runtime_error(msg);
        }
    }
};
} // anonymous namespace

void exec_sql(const fs::path& tdf, const std::string& sql)
{
    WritableDb(tdf).exec(sql);
}

// The packet header repeats NumScans; keep it consistent with what the Frames
// table claims, since the reader rejects packets where the two disagree.
static uint32_t packet_num_scans(const FrameSpec& frame)
{
    if(frame.sql_num_scans)
        try { return static_cast<uint32_t>(std::stoul(*frame.sql_num_scans)); }
        catch(const std::exception&) {}
    return frame.num_scans();
}

fs::path write_dataset(const DatasetSpec& spec, const fs::path& dir)
{
    fs::create_directories(dir);

    std::string bin;
    std::vector<uint64_t> offsets;
    for(const FrameSpec& frame : spec.frames)
    {
        offsets.push_back(bin.size());
        bin += frame.packet ? *frame.packet
                            : frame_packet(packet_num_scans(frame), zstd_raw_frame(byte_planes(frame_words(frame))));
    }

    if(spec.write_tdf_bin)
    {
        std::ofstream out(dir / "analysis.tdf_bin", std::ios::binary | std::ios::trunc);
        out.write(bin.data(), static_cast<std::streamsize>(bin.size()));
        if(!out)
            throw std::runtime_error("cannot write analysis.tdf_bin");
    }

    if(spec.write_tdf)
    {
        fs::remove(dir / "analysis.tdf");
        WritableDb db(dir / "analysis.tdf");
        db.exec("BEGIN");
        db.exec("CREATE TABLE GlobalMetadata (Key TEXT PRIMARY KEY, Value TEXT)");
        db.exec("CREATE TABLE Frames (Id INTEGER PRIMARY KEY, Time REAL, MsMsType INTEGER, "
                "TimsId INTEGER, NumScans INTEGER, NumPeaks INTEGER, AccumulationTime REAL)");
        for(const auto& [key, value] : spec.metadata)
            db.exec("INSERT INTO GlobalMetadata VALUES (" + sql_quote(key) + ", " + sql_quote(value) + ")");
        for(size_t i = 0; i < spec.frames.size(); i++)
        {
            const FrameSpec& f = spec.frames[i];
            db.exec("INSERT INTO Frames (Id, Time, MsMsType, TimsId, NumScans, NumPeaks, AccumulationTime) VALUES (" +
                    std::to_string(f.id) + ", " +
                    sql_double(f.time) + ", " +
                    std::to_string(f.msms_type) + ", " +
                    f.sql_tims_id.value_or(std::to_string(offsets[i])) + ", " +
                    f.sql_num_scans.value_or(std::to_string(f.num_scans())) + ", " +
                    f.sql_num_peaks.value_or(std::to_string(f.num_peaks())) + ", " +
                    f.sql_accumulation_time.value_or(sql_double(f.accumulation_time)) + ")");
        }
        for(const std::string& sql : spec.extra_sql)
            db.exec(sql);
        db.exec("COMMIT");
    }
    return dir;
}

TempDir::TempDir()
{
    static std::atomic<uint64_t> counter{0};
    std::random_device rd;
    const auto base = fs::temp_directory_path();
    for(int attempt = 0; attempt < 100; attempt++)
    {
        const uint64_t tag = (uint64_t(rd()) << 32) ^ uint64_t(rd())
                           ^ uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = base / ("opentims-cpptest-" + std::to_string(tag) + "-" + std::to_string(counter++));
        if(fs::create_directory(path_))
            return;
    }
    throw std::runtime_error("cannot create a temporary directory");
}

TempDir::~TempDir()
{
    std::error_code ec;
    fs::remove_all(path_, ec);
}

void Columns::resize(size_t n)
{
    frame.resize(n);
    scan.resize(n);
    tof.resize(n);
    intensity.resize(n);
    mz.resize(n);
    inv_ion_mobility.resize(n);
    retention_time.resize(n);
}

uint32_t corrected_intensity(uint32_t raw, double accumulation_time)
{
    return static_cast<uint32_t>(static_cast<double>(raw) * (100.0 / accumulation_time) + 0.5);
}

double expected_mz(uint32_t tof)
{
    const double intercept = std::sqrt(kMzMin);
    const double slope = (std::sqrt(kMzMax) - std::sqrt(kMzMin)) / static_cast<double>(kTofMax);
    const double v = intercept + slope * static_cast<double>(tof);
    return v * v;
}

double expected_inv_ion_mobility(uint32_t scan, uint32_t max_num_scans)
{
    return kImMax + (kImMin - kImMax) / static_cast<double>(max_num_scans) * static_cast<double>(scan);
}

Columns expected_columns(const DatasetSpec& spec, const std::vector<uint32_t>& ids)
{
    uint32_t max_num_scans = 0;
    for(const FrameSpec& f : spec.frames)
        max_num_scans = std::max(max_num_scans, f.num_scans());

    Columns c;
    for(uint32_t id : ids)
    {
        auto it = std::find_if(spec.frames.begin(), spec.frames.end(), [id](const FrameSpec& f) { return f.id == id; });
        if(it == spec.frames.end())
            throw std::logic_error("expected_columns: no frame " + std::to_string(id));
        for(uint32_t s = 0; s < it->num_scans(); s++)
            for(const Peak& p : it->scans[s])
            {
                c.frame.push_back(id);
                c.scan.push_back(s);
                c.tof.push_back(p.tof);
                c.intensity.push_back(corrected_intensity(p.intensity, it->accumulation_time));
                c.mz.push_back(expected_mz(p.tof));
                c.inv_ion_mobility.push_back(expected_inv_ion_mobility(s, max_num_scans));
                c.retention_time.push_back(it->time);
            }
    }
    return c;
}

DatasetSpec random_dataset(uint32_t seed, uint32_t n_frames, uint32_t max_scans,
                           uint32_t max_peaks_per_scan, uint32_t first_id,
                           uint32_t id_stride, uint32_t empty_every)
{
    std::mt19937 rng(seed);
    auto uniform = [&rng](uint32_t lo, uint32_t hi) { return std::uniform_int_distribution<uint32_t>(lo, hi)(rng); };

    DatasetSpec spec;
    for(uint32_t i = 0; i < n_frames; i++)
    {
        FrameSpec f;
        f.id = first_id + i * id_stride;
        f.time = 0.5 + 1.25 * i; // short decimals: sqlite hands REALs over as 15-digit text
        f.msms_type = (i % 3 == 0) ? 0 : 9;
        f.scans.resize(uniform(1, max_scans));
        const bool empty_frame = empty_every != 0 && i % empty_every == empty_every - 1;
        if(!empty_frame)
            for(auto& scan : f.scans)
            {
                if(uniform(0, 3) == 0)
                    continue; // empty scan
                const uint32_t n = uniform(0, max_peaks_per_scan);
                std::vector<uint32_t> tofs;
                for(uint32_t k = 0; k < n; k++)
                    tofs.push_back(uniform(0, kTofMax));
                std::sort(tofs.begin(), tofs.end());
                for(uint32_t tof : tofs)
                    scan.push_back({tof, uniform(1, 5000)});
            }
        spec.frames.push_back(std::move(f));
    }
    return spec;
}

fs::path test_d()
{
    return fs::path(OPENTIMS_TEST_D);
}

} // namespace ottest
