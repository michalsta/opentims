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

#include "opentims_types.h"

// adding default converters.
#include "scan2drift_converter.h"
#include "tof2mz_converter.h"


// [[Rcpp::export]]
void setup_bruker_so(const Rcpp::String& path)
{
    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(path.get_cstring());
    DefaultScan2DriftConverterFactory::setAsDefault<BrukerScan2DriftConverterFactory, const char*>(path.get_cstring());
}


// [[Rcpp::export]]
Rcpp::XPtr<TimsDataHandle> tdf_open(const Rcpp::String& tdf_bin_path,
                                    const Rcpp::String& path,
                                    const Rcpp::String& db_path)
{
    TimsDataHandle* p;
    p = new TimsDataHandle(tdf_bin_path, path, db_path);
    return Rcpp::XPtr<TimsDataHandle>(p, true);
}


// [[Rcpp::export]]
Rcpp::XPtr<TimsDataHandle> tdf_open_dir(const Rcpp::String& path)
{
    TimsDataHandle* p;
    p = new TimsDataHandle(path);
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

