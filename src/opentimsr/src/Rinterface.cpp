/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <limits>
#include <stdexcept>
#include <vector>
#define STRICT_R_HEADERS
#include <Rcpp.h>

#include "opentims.h"

// adding default converters.
#include "converters.h"
#include "thread_mgr.h"


// [[Rcpp::export(.setup_bruker_so)]]
void setup_bruker_so(const Rcpp::String& path)
{
    setup_bruker(std::string(path.get_cstring()));
}


// [[Rcpp::export(.setup_opensource)]]
void setup_opensource_r() { setup_opensource(); }


// [[Rcpp::export]]
Rcpp::XPtr<TimsDataHandle> tdf_open(const Rcpp::String& path_d,
                                     const Rcpp::List& sql_res,
                                     const Rcpp::CharacterVector& metadata_keys,
                                     const Rcpp::CharacterVector& metadata_values)
{
    TimsDataHandle* p; 
    p = new TimsDataHandle(path_d, sql_res, metadata_keys, metadata_values);
    return Rcpp::XPtr<TimsDataHandle>(p, true);
}


// [[Rcpp::export]]
void tdf_close(Rcpp::XPtr<TimsDataHandle> tdf)
{
    tdf.release();
}


// [[Rcpp::export]]
uint32_t tdf_min_frame_id(Rcpp::XPtr<TimsDataHandle> tdf)
{
    return tdf->min_frame_id();
}


// [[Rcpp::export]]
uint32_t tdf_max_frame_id(Rcpp::XPtr<TimsDataHandle> tdf)
{
    return tdf->max_frame_id();
}


// [[Rcpp::export]]
size_t tdf_no_peaks_total(Rcpp::XPtr<TimsDataHandle> tdf)
{
    return tdf->no_peaks_total();
}


// Decode the raw columns (frame, scan, tof, intensity) of the given frames and
// return them as a data.frame of doubles.
Rcpp::DataFrame raw_columns_df(TimsDataHandle& tdh, const std::vector<uint32_t>& frame_ids)
{
    using namespace Rcpp;

    const size_t peaks_no = tdh.no_peaks_in_frames(frame_ids.data(), frame_ids.size());

    // Uninitialised buffer, filled by the core: 4 consecutive columns of peaks_no values.
    std::unique_ptr<uint32_t[]> raw(new uint32_t[4 * peaks_no]);
    tdh.extract_frames(frame_ids.data(), frame_ids.size(), raw.get());

    NumericVector columns[4];
    for(size_t col = 0; col < 4; col++)
    {
        columns[col] = NumericVector(Rf_allocVector(REALSXP, peaks_no));
        const uint32_t* src = raw.get() + col * peaks_no;
        double* dst = REAL(columns[col]);
        for(size_t ii = 0; ii < peaks_no; ii++)
            dst[ii] = src[ii];
    }

    return DataFrame::create( Named("frame")     = columns[0],
                              Named("scan")      = columns[1],
                              Named("tof")       = columns[2],
                              Named("intensity") = columns[3] );
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_range(Rcpp::XPtr<TimsDataHandle> tdf, size_t start, size_t end, int32_t step = 1)
{
    TimsDataHandle& tdh = *tdf;
    if(step <= 0)
        throw std::invalid_argument("step must be positive");
    if(end > tdh.max_frame_id())
        end = tdh.max_frame_id()+1;

    std::vector<uint32_t> frame_ids;
    for(size_t idx = start; idx < end; idx += step)
        frame_ids.push_back(idx);

    return raw_columns_df(tdh, frame_ids);
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_indexes(Rcpp::XPtr<TimsDataHandle> tdf, Rcpp::IntegerVector indexes)
{
    std::vector<uint32_t> frame_ids(indexes.cbegin(), indexes.cend());
    return raw_columns_df(*tdf, frame_ids);
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_range_noend(Rcpp::XPtr<TimsDataHandle> tdf, size_t start, int32_t step = 1)
{
    return tdf_get_range(tdf, start, std::numeric_limits<size_t>::max(), step);
}


// Named list of R vectors that the core decodes into directly, avoiding a
// zero-filled intermediate buffer and a copy. The core's uint32 values are
// stored bit for bit in R's integer storage.
class RColumns
{
    Rcpp::List columns;
    std::vector<std::string> names;

    void add(const char* name, SEXP column)
    {
        columns.push_back(column);
        names.push_back(name);
    }

 public:
    uint32_t* add_uint32(const char* name, size_t size, bool wanted)
    {
        if(!wanted)
            return nullptr;
        Rcpp::IntegerVector column(Rf_allocVector(INTSXP, size));
        add(name, column);
        return reinterpret_cast<uint32_t*>(INTEGER(column));
    }

    double* add_double(const char* name, size_t size, bool wanted)
    {
        if(!wanted)
            return nullptr;
        Rcpp::NumericVector column(Rf_allocVector(REALSXP, size));
        add(name, column);
        return REAL(column);
    }

    Rcpp::List list()
    {
        columns.names() = names;
        return columns;
    }
};


// [[Rcpp::export]]
Rcpp::List tdf_extract_frames(
    const Rcpp::XPtr<TimsDataHandle> tdf,
    const Rcpp::IntegerVector indexes,
    const bool get_frames = true,
    const bool get_scans = true,
    const bool get_tofs = true,
    const bool get_intensities = true,
    const bool get_mzs = true,
    const bool get_inv_ion_mobilities = true,
    const bool get_retention_times = true)
{
    TimsDataHandle& tdh = *tdf;

    std::vector<uint32_t> v(indexes.cbegin(), indexes.cend());

    const size_t peaks_no = tdh.no_peaks_in_frames(v.data(), v.size());

    // scan, tof and intensity are always returned; R code drops unwanted columns.
    RColumns out;
    uint32_t* frames = out.add_uint32("frame", peaks_no, get_frames);
    uint32_t* scans = out.add_uint32("scan", peaks_no, true);
    uint32_t* tofs = out.add_uint32("tof", peaks_no, true);
    uint32_t* intensities = out.add_uint32("intensity", peaks_no, true);
    double* mzs = out.add_double("mz", peaks_no, get_mzs);
    double* inv_ion_mobilities = out.add_double("inv_ion_mobility", peaks_no, get_inv_ion_mobilities);
    double* retention_times = out.add_double("retention_time", peaks_no, get_retention_times);

    tdh.extract_frames(
        v.data(),
        v.size(),
        frames,
        scans,
        tofs,
        intensities,
        mzs,
        inv_ion_mobilities,
        retention_times
    );

    return out.list();
}



// [[Rcpp::export]]
Rcpp::List tdf_extract_frames_slice(
    const Rcpp::XPtr<TimsDataHandle> tdf,
    const size_t start,
    const size_t end,
    const int32_t step = 1,
    const bool get_frames = true,
    const bool get_scans = true,
    const bool get_tofs = true,
    const bool get_intensities = true,
    const bool get_mzs = true,
    const bool get_inv_ion_mobilities = true,
    const bool get_retention_times = true)
{
    TimsDataHandle& tdh = *tdf;

    const size_t peaks_no = tdh.no_peaks_in_slice(start, end, step);

    // scan, tof and intensity are always returned; R code drops unwanted columns.
    RColumns out;
    uint32_t* frames = out.add_uint32("frame", peaks_no, get_frames);
    uint32_t* scans = out.add_uint32("scan", peaks_no, true);
    uint32_t* tofs = out.add_uint32("tof", peaks_no, true);
    uint32_t* intensities = out.add_uint32("intensity", peaks_no, true);
    double* mzs = out.add_double("mz", peaks_no, get_mzs);
    double* inv_ion_mobilities = out.add_double("inv_ion_mobility", peaks_no, get_inv_ion_mobilities);
    double* retention_times = out.add_double("retention_time", peaks_no, get_retention_times);

    tdh.extract_frames_slice(
        start,
        end,
        step,
        frames,
        scans,
        tofs,
        intensities,
        mzs,
        inv_ion_mobilities,
        retention_times
    );

    return out.list();
}

// [[Rcpp::export]]
void tdf_set_num_threads(const size_t n)
{
    ThreadingManager::get_instance().set_num_threads(n);
}
