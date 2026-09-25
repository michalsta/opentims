/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <limits>
#include <stdexcept>
#include <optional>
#include <algorithm>
#include <string>
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

// Turn a named list of equally long columns into a data.frame, keeping the
// columns listed in `columns`, in that order.
Rcpp::List as_data_frame(const Rcpp::List& all_columns, const Rcpp::CharacterVector& columns, size_t rows)
{
    Rcpp::List df(columns.size());
    for(R_xlen_t ii = 0; ii < columns.size(); ii++)
        df[ii] = all_columns[Rcpp::as<std::string>(columns[ii])];
    df.names() = columns;
    df.attr("class") = "data.frame";
    if(rows == 0)
        df.attr("row.names") = Rcpp::IntegerVector(0);
    else
        df.attr("row.names") = Rcpp::IntegerVector::create(NA_INTEGER, -static_cast<int>(rows));
    return df;
}


// [[Rcpp::export]]
Rcpp::List tdf_extract_separate_frames(
    const Rcpp::XPtr<TimsDataHandle> tdf,
    const Rcpp::IntegerVector indexes,
    const Rcpp::CharacterVector columns)
{
    TimsDataHandle& tdh = *tdf;

    const std::vector<std::string> column_names = Rcpp::as<std::vector<std::string>>(columns);
    auto wanted = [&](const char* name)
    {
        return std::find(column_names.begin(), column_names.end(), name) != column_names.end();
    };
    const bool get_frames = wanted("frame");
    const bool get_scans = wanted("scan");
    const bool get_tofs = wanted("tof");
    const bool get_intensities = wanted("intensity");
    const bool get_mzs = wanted("mz");
    const bool get_inv_ion_mobilities = wanted("inv_ion_mobility");
    const bool get_retention_times = wanted("retention_time");

    const std::vector<uint32_t> ids(indexes.cbegin(), indexes.cend());
    const size_t no_frames = ids.size();

    // One set of R vectors per frame, which the core decodes into directly.
    std::vector<uint32_t*> frame_ids(no_frames), scan_ids(no_frames), tofs(no_frames), intensities(no_frames);
    std::vector<double*> mzs(no_frames), inv_ion_mobilities(no_frames), retention_times(no_frames);
    std::vector<size_t> sizes(no_frames);
    Rcpp::List frames_columns(no_frames);

    for(size_t ii = 0; ii < no_frames; ii++)
    {
        sizes[ii] = tdh.get_frame(ids[ii]).num_peaks;
        RColumns out;
        frame_ids[ii] = out.add_uint32("frame", sizes[ii], get_frames);
        scan_ids[ii] = out.add_uint32("scan", sizes[ii], get_scans);
        tofs[ii] = out.add_uint32("tof", sizes[ii], get_tofs);
        intensities[ii] = out.add_uint32("intensity", sizes[ii], get_intensities);
        mzs[ii] = out.add_double("mz", sizes[ii], get_mzs);
        inv_ion_mobilities[ii] = out.add_double("inv_ion_mobility", sizes[ii], get_inv_ion_mobilities);
        retention_times[ii] = out.add_double("retention_time", sizes[ii], get_retention_times);
        frames_columns[ii] = out.list();
    }

    tdh.extract_frames(ids, frame_ids.data(), scan_ids.data(), tofs.data(), intensities.data(),
                       mzs.data(), inv_ion_mobilities.data(), retention_times.data());

    Rcpp::List result(no_frames);
    for(size_t ii = 0; ii < no_frames; ii++)
        result[ii] = as_data_frame(frames_columns[ii], columns, sizes[ii]);
    return result;
}


std::optional<uint32_t> optional_frame(const Rcpp::Nullable<Rcpp::IntegerVector>& frame)
{
    if(frame.isNull())
        return std::nullopt;
    return static_cast<uint32_t>(Rcpp::IntegerVector(frame.get())[0]);
}


// [[Rcpp::export]]
void tdf_set_mz_lookup_frame(const Rcpp::XPtr<TimsDataHandle> tdf, const Rcpp::Nullable<Rcpp::IntegerVector> frame)
{
    tdf->set_mz_lookup_frame(optional_frame(frame));
}


// [[Rcpp::export]]
void tdf_set_inv_ion_mobility_lookup_frame(const Rcpp::XPtr<TimsDataHandle> tdf, const Rcpp::Nullable<Rcpp::IntegerVector> frame)
{
    tdf->set_inv_ion_mobility_lookup_frame(optional_frame(frame));
}


// [[Rcpp::export]]
void tdf_set_num_threads(const size_t n)
{
    ThreadingManager::get_instance().set_num_threads(n);
}
