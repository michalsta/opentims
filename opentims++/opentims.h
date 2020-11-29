/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020 Michał Startek and Mateusz Łącki
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License, version 3 only,
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>

#include "sqlite/sqlite3.h"
#include "zstd/zstd.h"

#include "platform.h"

#ifdef OPENTIMS_BUILDING_R
#define STRICT_R_HEADERS
#include "mio.h"
#else
#include "mio.hpp"
#endif



class TimsDataHandle;

int tims_sql_callback(void* out, int cols, char** row, char** colnames);

class TimsFrame
{
    TimsFrame(uint32_t _id,
              uint32_t _num_scans,
              uint32_t _num_peaks,
              uint32_t _msms_type,
              double _intensity_correction,
              double _time,
              const char* frame_ptr,
              TimsDataHandle& parent_hndl
            );

    std::unique_ptr<char[]> back_buffer;

    char* bytes0;
    char* bytes1;
    char* bytes2;
    char* bytes3;

    inline uint32_t back_data(size_t index)
    {
        uint32_t ret;
        char* bytes = reinterpret_cast<char*>(&ret);

        bytes[0] = bytes0[index];
        bytes[1] = bytes1[index];
        bytes[2] = bytes2[index];
        bytes[3] = bytes3[index];

        return ret;
    }

    const char * const tims_bin_frame;

    friend class TimsDataHandle;
    friend int tims_sql_callback(void* out, int cols, char** row, char** colnames);

    TimsDataHandle& parent_tdh;

    /* TODO: implement the below one day.
    template void save_to_buffs_impl<bool frame_ids_present,
                                     bool scan_ids_present,
                                     bool intensities_present,
                                     bool mzs_present,
                                     bool drift_times_present,
                                     bool retention_times_present,
                                     > (uint32_t* frame_ids,
                                        uint32_t* scan_ids,
                                        uint32_t* tofs,
                                        uint32_t* intensities,
                                        double* mzs,
                                        double* drift_times,
                                        double* retention_times,
                                        ZSTD_DCtx* decomp_ctx = nullptr);
    */

public:
    static TimsFrame TimsFrameFromSql(char** sql_row, TimsDataHandle& parent_handle);

    const uint32_t id;
    const uint32_t num_scans;
    const uint32_t num_peaks;
    const uint32_t msms_type;
    const double intensity_correction;
    const double time;

    void print() const;

    void decompress(char* decompression_buffer = nullptr, ZSTD_DCtx* decomp_ctx = nullptr);

    void close();

    inline size_t data_size_ints() const { return num_scans + num_peaks + num_peaks; };

    inline size_t data_size_bytes() const { return data_size_ints() * 4; };

    void save_to_buffs(uint32_t* frame_ids, uint32_t* scan_ids, uint32_t* tofs, uint32_t* intensities, double* mzs, double* drift_times, double* retention_times, ZSTD_DCtx* decomp_ctx = nullptr);

    void save_to_matrix_buffer(uint32_t* buf, ZSTD_DCtx* decomp_ctx = nullptr) { save_to_buffs(buf, buf+num_peaks, buf+2*num_peaks, buf+3*num_peaks, nullptr, nullptr, nullptr, decomp_ctx); };
};

struct Peak
{
    uint32_t frame_id;
    uint32_t scan_id;
    uint32_t tof;
    uint32_t intensity;
    void print();
};

class BrukerTof2MzConverter;
class Tof2MzConverter;
class BrukerScan2DriftConverter;
class Scan2DriftConverter;

class TimsDataHandle
{
friend class BrukerTof2MzConverter;
friend class BrukerScan2DriftConverter;

private:
    const std::string tims_dir_path;
    mio::mmap_source tims_data_bin;
    std::unordered_map<uint32_t, TimsFrame> frame_descs;
    void read_sql(const std::string& tims_tdf_path);
    uint32_t _min_frame_id;
    uint32_t _max_frame_id;

    std::unique_ptr<char[]> decompression_buffer;

    std::unique_ptr<uint32_t[]> _scan_ids_buffer;
    std::unique_ptr<uint32_t[]> _tofs_buffer;
    std::unique_ptr<uint32_t[]> _intensities_buffer;

    ZSTD_DCtx* zstd_dctx;

    sqlite3* db_conn;

    std::unique_ptr<Tof2MzConverter> tof2mz_converter;
    std::unique_ptr<Scan2DriftConverter> scan2drift_converter;

public:
    TimsDataHandle(const std::string& tims_tdf_bin_path, const std::string& tims_tdf_path, const std::string& tims_data_dir);

    TimsDataHandle(const std::string& tims_data_dir);

    ~TimsDataHandle();

    TimsFrame& get_frame(uint32_t frame_no);

    const std::unordered_map<uint32_t, TimsFrame>& get_frame_descs() const;

    size_t no_peaks_total() const;

    size_t no_peaks_in_frames(const uint32_t* indexes, size_t no_indexes);

    size_t no_peaks_in_frames(const std::vector<uint32_t>& indexes) { return no_peaks_in_frames(indexes.data(), indexes.size()); };

    size_t no_peaks_in_slice(uint32_t start, uint32_t end, uint32_t step);

    uint32_t min_frame_id() const { return _min_frame_id; };

    uint32_t max_frame_id() const { return _max_frame_id; };

    bool has_frame(uint32_t frame_id) const { return frame_descs.count(frame_id) > 0; };

    void set_converter(std::unique_ptr<Tof2MzConverter>&& converter);
    void set_converter(std::unique_ptr<Scan2DriftConverter>&& converter);

    void extract_frames(const uint32_t* indexes, size_t no_indexes, uint32_t* result);

    void extract_frames(const std::vector<uint32_t>& indexes, uint32_t* result) { extract_frames(indexes.data(), indexes.size(), result); };

    void extract_frames_slice(uint32_t start, uint32_t end, uint32_t step, uint32_t* result);

    void extract_frames(const uint32_t* indexes, size_t no_indexes, uint32_t* frame_ids, uint32_t* scan_ids, uint32_t* tofs, uint32_t* intensities, double* mzs, double* drift_times, double* retention_times);
    void extract_frames(const std::vector<uint32_t>& indexes, uint32_t* frame_ids, uint32_t* scan_ids, uint32_t* tofs, uint32_t* intensities, double* mzs, double* drift_times, double* retention_times)
                                { extract_frames(indexes.data(), indexes.size(), frame_ids, scan_ids, tofs, intensities, mzs, drift_times, retention_times); };

    void extract_frames_slice(uint32_t start, uint32_t end, uint32_t step, uint32_t* frame_ids, uint32_t* scan_ids, uint32_t* tofs, uint32_t* intensities, double* mzs, double* drift_times, double* retention_times);

    void allocate_buffers();

    inline void ensure_buffers_allocated() { if(_scan_ids_buffer) return; allocate_buffers(); };

    void free_buffers();

    size_t max_peaks_in_frame();

    size_t expose_frame(size_t frame_id);

    const std::unique_ptr<uint32_t[]>& scan_ids_buffer() { return _scan_ids_buffer; };

    const std::unique_ptr<uint32_t[]>& tofs_buffer() { return _tofs_buffer; };

    const std::unique_ptr<uint32_t[]>& intensities_buffer() { return _intensities_buffer; };

    const sqlite3* db_connection() { return db_conn; };

    friend int tims_sql_callback(void* out, int cols, char** row, char** colnames);

    friend class TimsFrame;
};
