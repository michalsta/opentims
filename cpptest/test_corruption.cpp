/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Damaged datasets must fail with an exception: no crash, no out-of-bounds
// access (the sanitizer builds check that part), and the handle stays usable.

#include <cstdint>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

FrameSpec good_frame(uint32_t id)
{
    FrameSpec f;
    f.id = id;
    f.time = id;
    f.scans = {{{10, 1}, {20, 2}}, {}, {{30, 3}}, {{40, 4}, {50, 5}, {60, 6}}};
    return f;
}

// Extracts frame `id` into right-sized buffers; returns the exception message, or "" on success.
std::string try_extract(TimsDataHandle& h, uint32_t id)
{
    TimsFrame& f = h.get_frame(id);
    const size_t n = f.num_peaks;
    std::vector<uint32_t> fr(n), sc(n), tf(n), in(n);
    std::vector<double> mz(n), im(n), rt(n);
    try
    {
        f.save_to_buffs(fr.data(), sc.data(), tf.data(), in.data(), mz.data(), im.data(), rt.data());
    }
    catch(const std::runtime_error& e)
    {
        return e.what();
    }
    return "";
}

void expect_good_frame_extracts(TimsDataHandle& h, const DatasetSpec& spec, uint32_t id)
{
    const Columns exp = expected_columns(spec, {id});
    std::vector<uint32_t> tof(exp.size());
    h.extract_frames(std::vector<uint32_t>{id}, nullptr, nullptr, tof.data(), nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(tof, exp.tof);
}

// A dataset of good frame 1, the frame under test as 2, good frame 3.
struct Sandwich
{
    TempDir tmp;
    DatasetSpec spec;
    std::unique_ptr<TimsDataHandle> h;

    explicit Sandwich(FrameSpec bad)
    {
        bad.id = 2;
        spec.frames = {good_frame(1), bad, good_frame(3)};
        write_dataset(spec, tmp.path());
        h = std::make_unique<TimsDataHandle>(tmp.path().string());
    }
    ~Sandwich() { h.reset(); }

    void expect_frame2_fails(const std::string& message_part)
    {
        const std::string msg = try_extract(*h, 2);
        EXPECT_NE(msg, "") << "corrupted frame extracted without error";
        EXPECT_NE(msg.find(message_part), std::string::npos) << msg;
        EXPECT_NE(msg.find("Frame 2"), std::string::npos) << msg;
        // Neighbours, and the shared decompression state, are unaffected.
        expect_good_frame_extracts(*h, spec, 1);
        expect_good_frame_extracts(*h, spec, 3);
    }
};

} // anonymous namespace

TEST(Corruption, OffsetBeyondEndOfFile)
{
    FrameSpec f = good_frame(2);
    f.sql_tims_id = "1000000";
    Sandwich s(f);
    s.expect_frame2_fails("beyond the end");
}

TEST(Corruption, DecompressedWordCountOverflow)
{
    for(const auto& counts : {std::pair{"4294967294", "1"},
                              std::pair{"1", "2147483648"}})
    {
        TempDir tmp;
        DatasetSpec spec;
        FrameSpec f;
        f.sql_num_scans = counts.first;
        f.sql_num_peaks = counts.second;
        f.packet = frame_packet(static_cast<uint32_t>(std::stoull(counts.first)), zstd_raw_frame(""));
        spec.frames = {f};
        write_dataset(spec, tmp.path());
        try
        {
            TimsDataHandle h(tmp.path().string());
            FAIL() << "overflowing metadata accepted";
        }
        catch(const std::runtime_error& e)
        {
            EXPECT_NE(std::string(e.what()).find("decompressed size exceeds"), std::string::npos);
        }
    }
}

TEST(Corruption, NegativeOffset)
{
    FrameSpec f = good_frame(2);
    f.sql_tims_id = "-1";
    Sandwich s(f);
    s.expect_frame2_fails("beyond the end");
}

TEST(Corruption, OffsetTooCloseToEndForHeader)
{
    TempDir tmp;
    DatasetSpec spec;
    spec.frames = {good_frame(1), good_frame(2)};
    write_dataset(spec, tmp.path());
    const auto size = fs::file_size(tmp / "analysis.tdf_bin");
    for(uint64_t back = 1; back < 8; back++)
    {
        exec_sql(tmp / "analysis.tdf", "UPDATE Frames SET TimsId = " + std::to_string(size - back) + " WHERE Id = 2");
        TimsDataHandle h(tmp.path().string());
        EXPECT_NE(try_extract(h, 2).find("beyond the end"), std::string::npos) << back;
    }
}

TEST(Corruption, PacketSizeTooSmall)
{
    for(uint32_t size : {0u, 1u, 7u})
    {
        FrameSpec f = good_frame(2);
        std::string packet = frame_packet(f.num_scans(), zstd_raw_frame(byte_planes(frame_words(f))));
        packet[0] = char(size);
        packet[1] = packet[2] = packet[3] = 0;
        f.packet = packet;
        Sandwich s(f);
        s.expect_frame2_fails("is invalid");
    }
}

TEST(Corruption, PacketSizeBeyondEndOfFile)
{
    FrameSpec f = good_frame(2);
    std::string packet = frame_packet(f.num_scans(), zstd_raw_frame(byte_planes(frame_words(f))));
    packet[3] = char(0x7F); // ~2 GiB
    f.packet = packet;
    Sandwich s(f);
    s.expect_frame2_fails("is invalid");
}

TEST(Corruption, NotZstd)
{
    FrameSpec f = good_frame(2);
    f.packet = frame_packet(f.num_scans(), std::string(64, 'Z'));
    Sandwich s(f);
    s.expect_frame2_fails("error uncompressing");
}

TEST(Corruption, EmptyZstdPayload)
{
    FrameSpec f = good_frame(2);
    f.packet = frame_packet(f.num_scans(), "");
    Sandwich s(f);
    s.expect_frame2_fails("decompressed to 0 bytes");
}

TEST(Corruption, TruncatedZstdFrame)
{
    FrameSpec f = good_frame(2);
    std::string z = zstd_raw_frame(byte_planes(frame_words(f)));
    z.resize(z.size() - 5);
    f.packet = frame_packet(f.num_scans(), z);
    Sandwich s(f);
    s.expect_frame2_fails("error uncompressing");
}

TEST(Corruption, DecompressesToFewerBytesThanNumPeaksImply)
{
    FrameSpec f = good_frame(2);
    f.sql_num_peaks = std::to_string(f.num_peaks() + 3);
    Sandwich s(f);
    s.expect_frame2_fails("decompressed to");
}

TEST(Corruption, DecompressesToMoreBytesThanNumPeaksImply)
{
    FrameSpec f = good_frame(2);
    f.sql_num_peaks = std::to_string(f.num_peaks() - 2);
    Sandwich s(f);
    s.expect_frame2_fails("error uncompressing"); // does not fit the destination
}

TEST(Corruption, ZstdContentSizeLiesLow)
{
    // Header claims less than the blocks hold.
    FrameSpec f = good_frame(2);
    std::string z = zstd_raw_frame(byte_planes(frame_words(f)));
    z[5] = char(uint8_t(z[5]) - 4);
    f.packet = frame_packet(f.num_scans(), z);
    Sandwich s(f);
    s.expect_frame2_fails("error uncompressing");
}

TEST(Corruption, ScanHeadersClaimTooManyPeaks)
{
    FrameSpec f = good_frame(2);
    std::vector<uint32_t> w = frame_words(f);
    w[1] = 2 * 1000; // scan 0 claims 1000 peaks
    f.words = w;
    Sandwich s(f);
    s.expect_frame2_fails("more peaks than NumPeaks");
}

TEST(Corruption, ScanHeadersSumBeyondNumPeaks)
{
    FrameSpec f = good_frame(2);
    std::vector<uint32_t> w = frame_words(f);
    for(uint32_t s = 1; s < f.num_scans(); s++)
        w[s] = 2 * 3; // each of scans 0..2 claims 3 of the 6 peaks
    f.words = w;
    Sandwich s(f);
    s.expect_frame2_fails("more peaks than NumPeaks");
}

TEST(Corruption, ScanHeaderWrapAround)
{
    // Counts that would overflow a naive 32-bit running sum.
    FrameSpec f = good_frame(2);
    std::vector<uint32_t> w = frame_words(f);
    w[1] = 0xFFFFFFFEu;
    w[2] = 0xFFFFFFFEu;
    f.words = w;
    Sandwich s(f);
    s.expect_frame2_fails("more peaks than NumPeaks");
}

TEST(Corruption, PacketNumScansDisagreesWithFramesTable)
{
    FrameSpec f = good_frame(2);
    std::string packet = frame_packet(f.num_scans(), zstd_raw_frame(byte_planes(frame_words(f))));
    packet[4] = char(f.num_scans() + 1);
    f.packet = packet;
    Sandwich s(f);
    s.expect_frame2_fails("holds 5 scans, but the Frames table says 4");
}

TEST(Corruption, PeaksButNoScans)
{
    FrameSpec f;
    f.sql_num_scans = "0";
    f.sql_num_peaks = "3";
    f.words = std::vector<uint32_t>{1, 1, 2, 2, 3, 3};
    Sandwich s(f);
    s.expect_frame2_fails("no scans");
}

TEST(Corruption, OddScanHeadersAreTolerated)
{
    // Header words are halved; odd values round down and the remainder lands in
    // the last scan. Nothing out of bounds.
    FrameSpec f = good_frame(2);
    std::vector<uint32_t> w = frame_words(f);
    w[1] = 3;
    f.words = w;
    Sandwich s(f);
    EXPECT_EQ(try_extract(*s.h, 2), "");
}

TEST(Corruption, ThreadedExtractionRethrows)
{
    FrameSpec f = good_frame(2);
    f.sql_num_peaks = std::to_string(f.num_peaks() + 1);
    Sandwich s(f);
    const std::vector<uint32_t> ids = {1, 2, 3, 1, 3};
    std::vector<std::vector<uint32_t>> tofs(ids.size());
    std::vector<uint32_t*> tf(ids.size());
    for(size_t i = 0; i < ids.size(); i++)
    {
        tofs[i].resize(s.h->get_frame(ids[i]).num_peaks);
        tf[i] = tofs[i].data();
    }
    std::vector<uint32_t*> nul32(ids.size(), nullptr);
    std::vector<double*> nuld(ids.size(), nullptr);
    for(int round = 0; round < 5; round++)
        EXPECT_THROW(s.h->extract_frames(ids, nul32.data(), nul32.data(), tf.data(), nul32.data(),
                                         nuld.data(), nuld.data(), nuld.data()),
                     std::runtime_error);
    // Without the bad frame, the same call works.
    const std::vector<uint32_t> good = {1, 3};
    EXPECT_NO_THROW(s.h->extract_frames(good, nul32.data(), nul32.data(), tf.data(), nul32.data(),
                                        nuld.data(), nuld.data(), nuld.data()));
}

TEST(Corruption, PerFrameTICRethrows)
{
    FrameSpec f = good_frame(2);
    f.sql_tims_id = "999999999";
    Sandwich s(f);
    std::vector<uint32_t> tic(3);
    EXPECT_THROW(s.h->per_frame_TIC(tic.data()), std::runtime_error);
}

TEST(Corruption, DecompressThrowsAndLeavesFrameUncached)
{
    FrameSpec f = good_frame(2);
    f.packet = frame_packet(f.num_scans(), std::string(64, 'Z'));
    Sandwich s(f);
    TimsFrame& fr = s.h->get_frame(2);
    EXPECT_THROW(fr.decompress(), std::runtime_error);
    EXPECT_THROW(fr.decompress(), std::runtime_error);
    std::vector<char> buf(fr.data_size_bytes());
    EXPECT_THROW(fr.decompress(buf.data()), std::runtime_error);
    EXPECT_NE(try_extract(*s.h, 2), "") << "a failed decompress() must not leave a cache behind";
    fr.close();
}

// Every truncation of analysis.tdf_bin: extraction succeeds or throws, never crashes.
TEST(Corruption, EveryTruncationOfTdfBin)
{
    TempDir tmp;
    DatasetSpec spec;
    spec.frames = {good_frame(1), good_frame(2), good_frame(3)};
    write_dataset(spec, tmp.path());
    const auto full = fs::file_size(tmp / "analysis.tdf_bin");
    std::string bin(full, '\0');
    {
        std::ifstream in(tmp / "analysis.tdf_bin", std::ios::binary);
        in.read(&bin[0], std::streamsize(full));
    }
    for(uint64_t len = 1; len < full; len++)
    {
        SCOPED_TRACE(len);
        {
            std::ofstream out(tmp / "analysis.tdf_bin", std::ios::binary | std::ios::trunc);
            out.write(bin.data(), std::streamsize(len));
        }
        TimsDataHandle h(tmp.path().string());
        size_t ok = 0;
        for(uint32_t id = 1; id <= 3; id++)
            ok += try_extract(h, id).empty();
        EXPECT_LT(ok, 3u) << "the last frame cannot be complete";
    }
}

// Random byte flips in the packets: the reader may return garbage values for
// flipped payload bytes, but must not read or write out of bounds.
TEST(Corruption, RandomByteFlips)
{
    TempDir tmp;
    DatasetSpec spec = random_dataset(77, 6, 12, 8, 1, 1, 0);
    write_dataset(spec, tmp.path());
    std::string bin;
    {
        std::ifstream in(tmp / "analysis.tdf_bin", std::ios::binary);
        bin.assign(std::istreambuf_iterator<char>(in), {});
    }
    std::mt19937 rng(1234);
    for(int round = 0; round < 300; round++)
    {
        std::string damaged = bin;
        const int flips = 1 + round % 4;
        for(int k = 0; k < flips; k++)
            damaged[rng() % damaged.size()] ^= char(1u << (rng() % 8));
        {
            std::ofstream out(tmp / "analysis.tdf_bin", std::ios::binary | std::ios::trunc);
            out.write(damaged.data(), std::streamsize(damaged.size()));
        }
        TimsDataHandle h(tmp.path().string());
        for(const FrameSpec& f : spec.frames)
            try_extract(h, f.id);
    }
}

// Random decompressed words, wrapped in valid zstd: exercises the scan-header
// and delta decoding on arbitrary input.
TEST(Corruption, RandomFrameWords)
{
    std::mt19937 rng(4321);
    for(int round = 0; round < 200; round++)
    {
        const uint32_t num_scans = 1 + rng() % 20;
        const uint32_t num_peaks = rng() % 30;
        std::vector<uint32_t> w(num_scans + 2 * num_peaks);
        for(uint32_t& x : w)
        {
            switch(rng() % 4)
            {
                case 0: x = 0; break;
                case 1: x = rng() % 8; break;
                case 2: x = 2 * (rng() % (num_peaks + 1)); break;
                default: x = rng(); break;
            }
        }
        FrameSpec f;
        f.words = w;
        f.sql_num_scans = std::to_string(num_scans);
        f.sql_num_peaks = std::to_string(num_peaks);
        Sandwich s(f);
        try_extract(*s.h, 2);
        expect_good_frame_extracts(*s.h, s.spec, 3);
    }
}
