/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include "tof2mz_converter.h"

#ifndef OPENTIMS_BUILDING_R
#include "sqlite_helper.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#endif

std::unique_ptr<Tof2MzConverterFactory> DefaultTof2MzConverterFactory::fac_instance;

Tof2MzConverter::~Tof2MzConverter() {}

std::string Tof2MzConverter::description() { return "Tof2MzConverter default"; }

/*
 * ErrorTof2MzConverter implementation
 */
ErrorTof2MzConverter::ErrorTof2MzConverter(TimsDataHandle&, pressure_compensation_strategy) {}

void ErrorTof2MzConverter::convert(uint32_t, double*, const double*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

void ErrorTof2MzConverter::convert(uint32_t, double*, const uint32_t*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

void ErrorTof2MzConverter::inverse_convert(uint32_t, uint32_t*, const double*, uint32_t)
{
    throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
}

std::string ErrorTof2MzConverter::description() { return "ErrorTof2MzConverter default"; }

/*
 * BrukerTof2MzConverter implementation
 */

std::string BrukerTof2MzConverter::get_tims_error()
{
    const size_t buf_size = 10000;
    std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_size);
    tims_get_last_error_string(buf.get(), buf_size-1);
    buf[buf_size-1] = '\0';
    return std::string(buf.get());
}

BrukerTof2MzConverter::BrukerTof2MzConverter(TimsDataHandle& TDH, const std::string& lib_path, pressure_compensation_strategy pcs) : lib_handle(lib_path), bruker_file_handle(0)
{
    tims_open = lib_handle.symbol_lookup<tims_open_v2_fun_t>("tims_open_v2");
    tims_get_last_error_string = lib_handle.symbol_lookup<tims_get_last_error_string_fun_t>("tims_get_last_error_string");
    tims_close = lib_handle.symbol_lookup<tims_close_fun_t>("tims_close");
    tims_index_to_mz = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_index_to_mz");
    tims_mz_to_index = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_mz_to_index");

    bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 1, pcs);

    if(bruker_file_handle == 0)
        throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
}

BrukerTof2MzConverter::~BrukerTof2MzConverter()
{
    if(bruker_file_handle != 0) tims_close(bruker_file_handle);
}

void BrukerTof2MzConverter::convert(uint32_t frame_id, double* mzs, const double* tofs, uint32_t size)
{
    tims_index_to_mz(bruker_file_handle, frame_id, tofs, mzs, size);
}

void BrukerTof2MzConverter::convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size)
{
    std::unique_ptr<double[]> dbl_tofs = std::make_unique<double[]>(size);
    for(uint32_t idx = 0; idx < size; idx++)
        dbl_tofs[idx] = static_cast<double>(tofs[idx]);
    tims_index_to_mz(bruker_file_handle, frame_id, dbl_tofs.get(), mzs, size);
}

void BrukerTof2MzConverter::inverse_convert(uint32_t frame_id, uint32_t* tofs, const double* mzs, uint32_t size)
{
    std::unique_ptr<double[]> dbl_tofs = std::make_unique<double[]>(size);
    tims_mz_to_index(bruker_file_handle, frame_id, mzs, dbl_tofs.get(), size);
    for(uint32_t idx = 0; idx < size; idx++)
        tofs[idx] = static_cast<uint32_t>(dbl_tofs[idx]);
}

std::string BrukerTof2MzConverter::description()
{
    return "BrukerTof2MzConverter";
}

/*
 * Tof2MzConverterFactory implementation
 */

Tof2MzConverterFactory::~Tof2MzConverterFactory() {}

/*
 * ErrorTof2MzConverterFactory
 */
std::unique_ptr<Tof2MzConverter> ErrorTof2MzConverterFactory::produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    return std::make_unique<ErrorTof2MzConverter>(TDH, pcs);
}

/*
 * BrukerTof2MzConverterFactory implementation
 */

BrukerTof2MzConverterFactory::BrukerTof2MzConverterFactory(const char* _dll_path) :
    dll_path(_dll_path),
    lib_hndl(_dll_path)
    {}

BrukerTof2MzConverterFactory::BrukerTof2MzConverterFactory(const std::string& _dll_path) :
    dll_path(_dll_path),
    lib_hndl(_dll_path)
    {}

std::unique_ptr<Tof2MzConverter> BrukerTof2MzConverterFactory::produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    return std::make_unique<BrukerTof2MzConverter>(TDH, dll_path.c_str(), pcs);
}

BrukerTof2MzConverterFactory& BrukerTof2MzConverterFactory::instance(const std::string& path)
{
    static BrukerTof2MzConverterFactory inst(path);
    if (inst.dll_path != path)
        throw std::runtime_error("BrukerTof2MzConverterFactory: already initialized with '" + inst.dll_path + "', cannot reinitialize with '" + path + "'");
    return inst;
}

/*
 * DefaultTof2MzConverterFactory implementation
 */

std::unique_ptr<Tof2MzConverter> DefaultTof2MzConverterFactory::produceDefaultConverterInstance(TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    if(!fac_instance)
        fac_instance = std::make_unique<ErrorTof2MzConverterFactory>();

    return fac_instance->produce(TDH, pcs);
}

#ifndef OPENTIMS_BUILDING_R

/*
 * OpenSourceTof2MzConverter implementation
 */

OpenSourceTof2MzConverter::OpenSourceTof2MzConverter(
    double mz_min, double mz_max, uint32_t tof_max_index, bool is_otof_control)
{
    if (is_otof_control)
    {
        mz_min -= 5.0;
        mz_max += 5.0;
    }
    intercept_ = std::sqrt(mz_min);
    slope_ = (std::sqrt(mz_max) - std::sqrt(mz_min)) / static_cast<double>(tof_max_index);
}

void OpenSourceTof2MzConverter::convert(uint32_t, double* mzs, const double* tofs, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        double val = intercept_ + slope_ * tofs[i];
        mzs[i] = val * val;
    }
}

void OpenSourceTof2MzConverter::convert(uint32_t, double* mzs, const uint32_t* tofs, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        double val = intercept_ + slope_ * static_cast<double>(tofs[i]);
        mzs[i] = val * val;
    }
}

void OpenSourceTof2MzConverter::inverse_convert(uint32_t, uint32_t* tofs, const double* mzs, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        double val = (std::sqrt(mzs[i]) - intercept_) / slope_;
        tofs[i] = val > 0.0 ? static_cast<uint32_t>(val + 0.5) : 0;
    }
}

std::string OpenSourceTof2MzConverter::description()
{
    return "OpenSourceTof2MzConverter (linear-in-sqrt)";
}

void OpenSourceTof2MzConverter::updateCalibration(double new_intercept, double new_slope)
{
    intercept_ = new_intercept;
    slope_ = new_slope;
}

/*
 * OpenSourceTof2MzConverterFactory implementation
 */

namespace {
struct Tof2MzMetadata
{
    double mz_min = 0;
    double mz_max = 0;
    uint32_t tof_max = 0;
    bool is_otof = false;
};

int tof2mz_metadata_callback(void* out, int cols, char** row, char** colnames)
{
    (void)cols;
    (void)colnames;
    Tof2MzMetadata* meta = reinterpret_cast<Tof2MzMetadata*>(out);
    if (row[0] == nullptr || row[1] == nullptr)
        return 0;
    const char* key = row[0];
    const char* val = row[1];
    if (std::strcmp(key, "MzAcqRangeLower") == 0)
        meta->mz_min = std::atof(val);
    else if (std::strcmp(key, "MzAcqRangeUpper") == 0)
        meta->mz_max = std::atof(val);
    else if (std::strcmp(key, "DigitizerNumSamples") == 0)
        meta->tof_max = static_cast<uint32_t>(std::atol(val));
    else if (std::strcmp(key, "AcquisitionSoftware") == 0)
        meta->is_otof = (std::strcmp(val, "Bruker otofControl") == 0);
    return 0;
}
} // anonymous namespace

std::unique_ptr<Tof2MzConverter> OpenSourceTof2MzConverterFactory::produce(
    TimsDataHandle& TDH, pressure_compensation_strategy pcs)
{
    if (pcs != NoPressureCompensation)
        throw std::runtime_error(
            "Pressure compensation is not supported by the open-source m/z converter. "
            "Use Bruker's proprietary library for pressure compensation, or disable it.");

    std::string tdf_path = TDH.get_tims_dir_path() + "/analysis.tdf";
    RAIISqlite db(tdf_path);

    Tof2MzMetadata meta;
    db.query(
        "SELECT Key, Value FROM GlobalMetadata "
        "WHERE Key IN ('MzAcqRangeLower','MzAcqRangeUpper','DigitizerNumSamples','AcquisitionSoftware')",
        tof2mz_metadata_callback, &meta);

    if (meta.mz_min <= 0 || meta.mz_max <= meta.mz_min || meta.tof_max == 0)
        throw std::runtime_error(
            "OpenSourceTof2MzConverterFactory: invalid calibration metadata in " + tdf_path);

    return std::make_unique<OpenSourceTof2MzConverter>(meta.mz_min, meta.mz_max, meta.tof_max, meta.is_otof);
}

#endif // OPENTIMS_BUILDING_R
