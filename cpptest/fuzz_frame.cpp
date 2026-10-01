/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// libFuzzer target for frame decoding: zstd decompression, the size checks
// around it, and the scan-header/delta decoding of the decompressed words.
//
// Input: [mode][num_scans][num_peaks][payload...]
//   mode even: payload is the decompressed words, wrapped here in valid zstd,
//              so the fuzzer reaches the decoder past the decompressor;
//   mode odd:  payload is the tdf_bin packet's zstd frame, verbatim.
// The 8-byte packet header (size, NumScans) is always written consistently.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "opentims.h"
#include "converters.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

// One dataset directory per (NumScans, NumPeaks); only analysis.tdf_bin
// changes between inputs.
const fs::path& dataset_dir(uint32_t num_scans, uint32_t num_peaks)
{
    static TempDir root;
    static std::map<std::pair<uint32_t, uint32_t>, fs::path> dirs;
    auto it = dirs.find({num_scans, num_peaks});
    if(it != dirs.end())
        return it->second;
    DatasetSpec spec;
    FrameSpec f;
    f.id = 1;
    f.sql_num_scans = std::to_string(num_scans);
    f.sql_num_peaks = std::to_string(num_peaks);
    f.sql_tims_id = "0";
    f.packet = "";
    spec.frames = {f};
    const fs::path dir = root / (std::to_string(num_scans) + "_" + std::to_string(num_peaks));
    write_dataset(spec, dir);
    return dirs.emplace(std::make_pair(num_scans, num_peaks), dir).first->second;
}

} // anonymous namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
    setup_opensource();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if(size < 3)
        return 0;
    const bool wrap = data[0] % 2 == 0;
    const uint32_t num_scans = 1 + data[1] % 16;
    const uint32_t num_peaks = data[2] % 32;
    const std::string payload(reinterpret_cast<const char*>(data + 3), size - 3);

    std::string zstd;
    if(wrap)
    {
        std::vector<uint32_t> words(payload.size() / 4);
        for(size_t i = 0; i < words.size(); i++)
            words[i] = uint32_t(uint8_t(payload[4 * i])) | uint32_t(uint8_t(payload[4 * i + 1])) << 8 |
                       uint32_t(uint8_t(payload[4 * i + 2])) << 16 | uint32_t(uint8_t(payload[4 * i + 3])) << 24;
        zstd = zstd_raw_frame(byte_planes(words));
    }
    else
        zstd = payload;

    const fs::path& dir = dataset_dir(num_scans, num_peaks);
    {
        const std::string packet = frame_packet(num_scans, zstd);
        std::ofstream out(dir / "analysis.tdf_bin", std::ios::binary | std::ios::trunc);
        out.write(packet.data(), std::streamsize(packet.size()));
    }

    TimsDataHandle h(dir.string());
    TimsFrame& f = h.get_frame(1);
    const size_t n = f.num_peaks;
    std::vector<uint32_t> fr(n), sc(n), tf(n), in(n);
    std::vector<double> mz(n), im(n), rt(n);
    try
    {
        f.save_to_buffs(fr.data(), sc.data(), tf.data(), in.data(), mz.data(), im.data(), rt.data());
    }
    catch(const std::runtime_error&) {}
    try
    {
        const TimsFrame& cf = f;
        cf.save_to_buffs(nullptr, nullptr, nullptr, in.data(), nullptr, im.data(), nullptr, nullptr, nullptr);
    }
    catch(const std::runtime_error&) {}
    try
    {
        f.decompress();
        f.save_to_buffs(nullptr, sc.data(), tf.data(), nullptr, nullptr, nullptr, nullptr);
    }
    catch(const std::runtime_error&) {}
    f.close();
    return 0;
}
