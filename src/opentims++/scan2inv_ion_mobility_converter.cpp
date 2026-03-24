/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include "scan2inv_ion_mobility_converter.h"

#ifndef OPENTIMS_BUILDING_R
#include "sqlite_helper.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#endif

std::unique_ptr<Scan2InvIonMobilityConverterFactory> DefaultScan2InvIonMobilityConverterFactory::fac_instance;

Scan2InvIonMobilityConverter::~Scan2InvIonMobilityConverter() {}

std::string Scan2InvIonMobilityConverter::description() const
{
    return "Scan2InvIonMobilityConverter default";
}

/*
 * ErrorScan2InvIonMobilityConverter implementation
 */

ErrorScan2InvIonMobilityConverter::ErrorScan2InvIonMobilityConverter(TimsDataHandle&) {}

void ErrorScan2InvIonMobilityConverter::convert(uint32_t, double*, const double*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

void ErrorScan2InvIonMobilityConverter::convert(uint32_t, double*, const uint32_t*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

void ErrorScan2InvIonMobilityConverter::inverse_convert(uint32_t, uint32_t*, const double*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

std::string ErrorScan2InvIonMobilityConverter::description() const
{
    return "ErrorScan2InvIonMobilityConverter default";
}

/*
 * BrukerScan2InvIonMobilityConverter implementation
 */

std::string BrukerScan2InvIonMobilityConverter::get_tims_error()
{
    const size_t buf_size = 10000;
    std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_size);
    tims_get_last_error_string(buf.get(), buf_size-1);
    buf[buf_size-1] = '\0';
    return std::string(buf.get());
}

BrukerScan2InvIonMobilityConverter::BrukerScan2InvIonMobilityConverter(TimsDataHandle& TDH, const std::string& lib_path, pressure_compensation_strategy pcs) : lib_handle(lib_path), bruker_file_handle(0)
{
    tims_open = lib_handle.symbol_lookup<tims_open_v2_fun_t>("tims_open_v2");
    tims_get_last_error_string = lib_handle.symbol_lookup<tims_get_last_error_string_fun_t>("tims_get_last_error_string");
    tims_close = lib_handle.symbol_lookup<tims_close_fun_t>("tims_close");
    tims_scannum_to_inv_ion_mobility = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_scannum_to_oneoverk0");
    tims_inv_ion_mobility_to_scannum = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_oneoverk0_to_scannum");

    bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 1, pcs);

    if(bruker_file_handle == 0)
        throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
}

BrukerScan2InvIonMobilityConverter::~BrukerScan2InvIonMobilityConverter()
{
    if(bruker_file_handle != 0) tims_close(bruker_file_handle);
}

void BrukerScan2InvIonMobilityConverter::convert(uint32_t frame_id,
             double* inv_ion_mobilities,
             const double* scans,
             uint32_t size)
{
    tims_scannum_to_inv_ion_mobility(bruker_file_handle, frame_id, scans, inv_ion_mobilities, size);
}


void BrukerScan2InvIonMobilityConverter::convert(uint32_t frame_id,
             double* inv_ion_mobilities,
             const uint32_t* scans,
             uint32_t size)
{
    std::unique_ptr<double[]> dbl_scans = std::make_unique<double[]>(size);
    for(uint32_t idx = 0; idx < size; idx++)
        dbl_scans[idx] = static_cast<double>(scans[idx]);
    tims_scannum_to_inv_ion_mobility(bruker_file_handle, frame_id, dbl_scans.get(), inv_ion_mobilities, size);
}

void BrukerScan2InvIonMobilityConverter::inverse_convert(uint32_t frame_id,
             uint32_t* scans,
             const double* inv_ion_mobilities,
             uint32_t size)
{
    std::unique_ptr<double[]> dbl_scans = std::make_unique<double[]>(size);
    tims_inv_ion_mobility_to_scannum(bruker_file_handle, frame_id, inv_ion_mobilities, dbl_scans.get(), size);
    for(uint32_t idx = 0; idx < size; idx++)
        scans[idx] = static_cast<double>(dbl_scans[idx]);
}

std::string BrukerScan2InvIonMobilityConverter::description() const { return "BrukerScan2InvIonMobilityConverter"; }

/*
 * Scan2InvIonMobilityConverterFactory implementation
 */

Scan2InvIonMobilityConverterFactory::~Scan2InvIonMobilityConverterFactory() {}

/*
 * ErrorScan2InvIonMobilityConverterFactory implementation
 */

std::unique_ptr<Scan2InvIonMobilityConverter> ErrorScan2InvIonMobilityConverterFactory::produce(TimsDataHandle& TDH, pressure_compensation_strategy)
{
    return std::make_unique<ErrorScan2InvIonMobilityConverter>(TDH);
}

/*
 * BrukerScan2InvIonMobilityConverterFactory implementation
 */

BrukerScan2InvIonMobilityConverterFactory::BrukerScan2InvIonMobilityConverterFactory(const char* _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {}

BrukerScan2InvIonMobilityConverterFactory::BrukerScan2InvIonMobilityConverterFactory(const std::string& _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {}

std::unique_ptr<Scan2InvIonMobilityConverter> BrukerScan2InvIonMobilityConverterFactory::produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    return std::make_unique<BrukerScan2InvIonMobilityConverter>(TDH, dll_path.c_str(), pcs);
}

/*
 * DefaultScan2InvIonMobilityConverterFactory implementation
 */
std::unique_ptr<Scan2InvIonMobilityConverter> DefaultScan2InvIonMobilityConverterFactory::produceDefaultConverterInstance(TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    if(!fac_instance)
        fac_instance = std::make_unique<ErrorScan2InvIonMobilityConverterFactory>();

    return fac_instance->produce(TDH, pcs);
}

#ifndef OPENTIMS_BUILDING_R

/*
 * OpenSourceScan2ImConverter implementation
 */

OpenSourceScan2ImConverter::OpenSourceScan2ImConverter(
    double im_min, double im_max, uint32_t scan_max_index)
{
    intercept_ = im_max;
    slope_ = (im_min - im_max) / static_cast<double>(scan_max_index);
}

void OpenSourceScan2ImConverter::convert(uint32_t, double* inv_ion_mobilities,
    const double* scans, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        inv_ion_mobilities[i] = intercept_ + slope_ * scans[i];
}

void OpenSourceScan2ImConverter::convert(uint32_t, double* inv_ion_mobilities,
    const uint32_t* scans, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        inv_ion_mobilities[i] = intercept_ + slope_ * static_cast<double>(scans[i]);
}

void OpenSourceScan2ImConverter::inverse_convert(uint32_t, uint32_t* scans,
    const double* inv_ion_mobilities, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        double val = (inv_ion_mobilities[i] - intercept_) / slope_;
        scans[i] = val > 0.0 ? static_cast<uint32_t>(val + 0.5) : 0;
    }
}

std::string OpenSourceScan2ImConverter::description() const
{
    return "OpenSourceScan2ImConverter (linear)";
}

/*
 * OpenSourceScan2ImConverterFactory implementation
 */

namespace {
struct Scan2ImMetadata
{
    double im_min = 0;
    double im_max = 0;
};

int scan2im_metadata_callback(void* out, int cols, char** row, char** colnames)
{
    (void)cols;
    (void)colnames;
    Scan2ImMetadata* meta = reinterpret_cast<Scan2ImMetadata*>(out);
    if (row[0] == nullptr || row[1] == nullptr)
        return 0;
    const char* key = row[0];
    const char* val = row[1];
    if (std::strcmp(key, "OneOverK0AcqRangeLower") == 0)
        meta->im_min = std::atof(val);
    else if (std::strcmp(key, "OneOverK0AcqRangeUpper") == 0)
        meta->im_max = std::atof(val);
    return 0;
}

struct ScanMaxResult
{
    uint32_t scan_max = 0;
};

int scan_max_callback(void* out, int cols, char** row, char** colnames)
{
    (void)cols;
    (void)colnames;
    ScanMaxResult* res = reinterpret_cast<ScanMaxResult*>(out);
    if (row[0] != nullptr)
        res->scan_max = static_cast<uint32_t>(std::atol(row[0]));
    return 0;
}
} // anonymous namespace

std::unique_ptr<Scan2InvIonMobilityConverter> OpenSourceScan2ImConverterFactory::produce(
    TimsDataHandle& TDH, pressure_compensation_strategy)
{
    std::string tdf_path = TDH.get_tims_dir_path() + "/analysis.tdf";
    RAIISqlite db(tdf_path);

    Scan2ImMetadata meta;
    db.query(
        "SELECT Key, Value FROM GlobalMetadata "
        "WHERE Key IN ('OneOverK0AcqRangeLower','OneOverK0AcqRangeUpper')",
        scan2im_metadata_callback, &meta);

    ScanMaxResult scan_res;
    db.query("SELECT MAX(NumScans) FROM Frames", scan_max_callback, &scan_res);

    if (meta.im_min <= 0 || meta.im_max <= meta.im_min || scan_res.scan_max == 0)
        throw std::runtime_error(
            "OpenSourceScan2ImConverterFactory: invalid calibration metadata in " + tdf_path);

    return std::make_unique<OpenSourceScan2ImConverter>(meta.im_min, meta.im_max, scan_res.scan_max);
}

#endif // OPENTIMS_BUILDING_R
