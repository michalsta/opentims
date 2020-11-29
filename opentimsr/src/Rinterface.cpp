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

#include <limits>
#define STRICT_R_HEADERS
#include <Rcpp.h>

#include "opentimsr_types.h"

// adding default converters.
#include "scan2drift_converter.h"
#include "tof2mz_converter.h"


// [[Rcpp::export(.setup_bruker_so)]]
void setup_bruker_so(const Rcpp::String& path)
{
    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(path.get_cstring());
    DefaultScan2DriftConverterFactory::setAsDefault<BrukerScan2DriftConverterFactory, const char*>(path.get_cstring());
}


// [[Rcpp::export]]
Rcpp::XPtr<TimsDataHandle> tdf_open(const Rcpp::String& path_d)
{
    TimsDataHandle* p; 
    p = new TimsDataHandle(path_d);
    return Rcpp::XPtr<TimsDataHandle>(p, true);
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


// [[Rcpp::export]]
Rcpp::IntegerVector tdf_get_msms_type(Rcpp::XPtr<TimsDataHandle> tdf)
{
    using namespace Rcpp;
    size_t start = tdf->min_frame_id();
    size_t end = tdf->min_frame_id();
    int n = end - start;

    TimsDataHandle& tdh = *tdf;
    IntegerVector msms_types(n);

    for(size_t idx = start; idx < end; idx += 1)
    {

    }

    return msms_types;
} 

// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_range(Rcpp::XPtr<TimsDataHandle> tdf, size_t start, size_t end, int32_t step = 1)
{
    using namespace Rcpp;
    
    TimsDataHandle& tdh = *tdf;
    if(end > tdh.max_frame_id())
        end = tdh.max_frame_id()+1;

    std::vector<uint32_t> frame_ids;
    std::vector<uint32_t> scan_ids;
    std::vector<uint32_t> tofs;
    std::vector<uint32_t> intensities;

    for(size_t idx = start; idx < end; idx += step)
    {
        size_t frame_size = tdh.expose_frame(idx);

        // std::cerr << "frame_size" << std::endl;

        for(size_t ii=0; ii<frame_size; ii++)
        {
            frame_ids.push_back(idx);
            scan_ids.push_back(tdh.scan_ids_buffer()[ii]);
            tofs.push_back(tdh.tofs_buffer()[ii]);
            intensities.push_back(tdh.intensities_buffer()[ii]);
        }
    }

    DataFrame result = DataFrame::create( Named("frame")     = frame_ids,
                                          Named("scan")      = scan_ids,
                                          Named("tof")       = tofs,
                                          Named("intensity") = intensities );

    return result;
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_indexes(Rcpp::XPtr<TimsDataHandle> tdf, Rcpp::IntegerVector indexes)
{
    using namespace Rcpp;

    TimsDataHandle& tdh = *tdf;

    std::vector<uint32_t> frame_ids;
    std::vector<uint32_t> scan_ids;
    std::vector<uint32_t> tofs;
    std::vector<uint32_t> intensities;

    for(auto idx = indexes.cbegin(); idx != indexes.cend(); idx++)
    {
        size_t frame_size = tdh.expose_frame(*idx);

        // std::cerr << "frame_size" << std::endl;

        for(size_t ii=0; ii<frame_size; ii++)
        {
            frame_ids.push_back(*idx);
            scan_ids.push_back(tdh.scan_ids_buffer()[ii]);
            tofs.push_back(tdh.tofs_buffer()[ii]);
            intensities.push_back(tdh.intensities_buffer()[ii]);
        }
    }

    DataFrame result = DataFrame::create( Named("frame") = frame_ids,
                                          Named("scan")  = scan_ids,
                                          Named("tof")   = tofs,
                                          Named("intensity") = intensities );

    return result;
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_get_range_noend(Rcpp::XPtr<TimsDataHandle> tdf, size_t start, int32_t step = 1)
{
    return tdf_get_range(tdf, start, std::numeric_limits<size_t>::max(), step);
}


template<typename T> std::unique_ptr<T[]> R_get_ptr(const size_t size,
                                                    const bool really)
{
    if(really)
        return std::make_unique<T[]>(size);
    else
        return std::unique_ptr<T[]>();
}


template<typename T, typename U> void set_frame(Rcpp::DataFrame& df,
                                                const std::string& name,
                                                const std::unique_ptr<T[]>& tbl,
                                                size_t size)
{
    if(tbl){
        U vec = U::import(tbl.get(), tbl.get() + size);
        df[name] = vec;
    } 
}


// [[Rcpp::export]]
Rcpp::DataFrame tdf_extract_frames(
    const Rcpp::XPtr<TimsDataHandle> tdf,
    const Rcpp::IntegerVector indexes,
    const bool get_frames = true,
    const bool get_scans = true,
    const bool get_tofs = true,
    const bool get_intensities = true,
    const bool get_mzs = true,
    const bool get_dts = true,
    const bool get_rts = true)
{
    using namespace Rcpp;

    TimsDataHandle& tdh = *tdf;

    std::unique_ptr<uint32_t[]> v = std::make_unique<uint32_t[]>(indexes.size());

    for(size_t ii=0; ii < indexes.size(); ++ii) v[ii] = indexes[ii];

    const size_t peaks_no = tdh.no_peaks_in_frames(v.get(), indexes.size()); // conts for compiler optimization.

    std::unique_ptr<uint32_t[]> frames = R_get_ptr<uint32_t>(peaks_no, get_frames);
    std::unique_ptr<uint32_t[]> scans = R_get_ptr<uint32_t>(peaks_no, true);
    std::unique_ptr<uint32_t[]> tofs = R_get_ptr<uint32_t>(peaks_no, true);
    std::unique_ptr<uint32_t[]> intensities = R_get_ptr<uint32_t>(peaks_no, true);
    std::unique_ptr<double[]> mzs = R_get_ptr<double>(peaks_no, get_mzs);
    std::unique_ptr<double[]> dts = R_get_ptr<double>(peaks_no, get_dts);
    std::unique_ptr<double[]> rts = R_get_ptr<double>(peaks_no, get_rts);

    tdh.extract_frames(
        v.get(),
        indexes.size(),
        frames.get(),
        scans.get(),
        tofs.get(),
        intensities.get(),
        mzs.get(),
        dts.get(),
        rts.get()
    );

    DataFrame result = DataFrame::create();

    set_frame<uint32_t, Rcpp::IntegerVector>(result, "frame", frames, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "scan", scans, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "tof", tofs, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "intensity", intensities, peaks_no);
    
    set_frame<double, Rcpp::NumericVector>(result, "mz", mzs, peaks_no);
    set_frame<double, Rcpp::NumericVector>(result, "dt", dts, peaks_no);
    set_frame<double, Rcpp::NumericVector>(result, "rt", rts, peaks_no);

    return result;
}



// [[Rcpp::export]]
Rcpp::DataFrame tdf_extract_frames_slice(
    const Rcpp::XPtr<TimsDataHandle> tdf,
    const size_t start,
    const size_t end,
    const int32_t step = 1,
    const bool get_frames = true,
    const bool get_scans = true,
    const bool get_tofs = true,
    const bool get_intensities = true,
    const bool get_mzs = true,
    const bool get_dts = true,
    const bool get_rts = true)
{
    using namespace Rcpp;

    TimsDataHandle& tdh = *tdf;

    const size_t peaks_no = tdh.no_peaks_in_slice(start, end, step);

    //scan tof intensity always returned and only sometimes cut away
    std::unique_ptr<uint32_t[]> frames = R_get_ptr<uint32_t>(peaks_no, get_frames);
    std::unique_ptr<uint32_t[]> scans = R_get_ptr<uint32_t>(peaks_no, true);  
    std::unique_ptr<uint32_t[]> tofs = R_get_ptr<uint32_t>(peaks_no, true);
    std::unique_ptr<uint32_t[]> intensities = R_get_ptr<uint32_t>(peaks_no, true);
    std::unique_ptr<double[]> mzs = R_get_ptr<double>(peaks_no, get_mzs);
    std::unique_ptr<double[]> dts = R_get_ptr<double>(peaks_no, get_dts);
    std::unique_ptr<double[]> rts = R_get_ptr<double>(peaks_no, get_rts);

    tdh.extract_frames_slice(
        start, 
        end,
        step,
        frames.get(),
        scans.get(),
        tofs.get(),
        intensities.get(),
        mzs.get(),
        dts.get(),
        rts.get()
    );

    DataFrame result = DataFrame::create();

    set_frame<uint32_t, Rcpp::IntegerVector>(result, "frame", frames, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "scan", scans, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "tof", tofs, peaks_no);
    set_frame<uint32_t, Rcpp::IntegerVector>(result, "intensity", intensities, peaks_no);
    
    set_frame<double, Rcpp::NumericVector>(result, "mz", mzs, peaks_no);
    set_frame<double, Rcpp::NumericVector>(result, "dt", dts, peaks_no);
    set_frame<double, Rcpp::NumericVector>(result, "rt", rts, peaks_no);

    return result;
}