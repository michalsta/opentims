/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "bruker_api.h"
#include "platform.h"

#ifdef OPENTIMS_BUILDING_R
#include "opentimsr_types.h"
#else
#include "opentims.h"
#endif

#include "so_manager.h"

class Scan2InvIonMobilityConverter
{
 public:
    virtual void convert(uint32_t frame_id,
                         double* inv_ion_mobilities,
                         const double* scans,
                         uint32_t size) = 0;

    virtual void convert(uint32_t frame_id,
                         double* inv_ion_mobilities,
                         const uint32_t* scans,
                         uint32_t size) = 0;

    virtual void inverse_convert(uint32_t frame_id,
                                 uint32_t* scans,
                                 const double* inv_ion_mobilities,
                                 uint32_t size) = 0;

    virtual ~Scan2InvIonMobilityConverter();
    virtual std::string description() const;
};

class ErrorScan2InvIonMobilityConverter : public Scan2InvIonMobilityConverter
{
 public:
    ErrorScan2InvIonMobilityConverter(TimsDataHandle&);
    void convert(uint32_t, double*, const double*, uint32_t) override final;
    void convert(uint32_t, double*, const uint32_t*, uint32_t) override final;
    void inverse_convert(uint32_t frame_id, uint32_t* scans, const double* inv_ion_mobilities, uint32_t size) override final;
    std::string description() const override final;
};

class BrukerScan2InvIonMobilityConverter final : public Scan2InvIonMobilityConverter
{
    const LoadedLibraryHandle lib_handle;
    uint64_t bruker_file_handle;

    tims_open_v2_fun_t* tims_open;
    tims_get_last_error_string_fun_t* tims_get_last_error_string;
    tims_close_fun_t* tims_close;
    tims_convert_fun_t* tims_scannum_to_inv_ion_mobility;
    tims_convert_fun_t* tims_inv_ion_mobility_to_scannum;

    std::vector<double> lookup_table;  // inverse ion mobility of every scan, see set_lookup_frame()
    bool use_lookup_table = false;

    std::string get_tims_error();

 public:
    //! Convert with a lookup table instead of calling Bruker's library for every frame.
    /**
     * The table holds the value of every scan (0 to the largest NumScans - 1) for `frame`, as computed by Bruker's
     * library, and is then used for all frames: exact for `frame`, approximate for the
     * others. The table's memory is allocated on first use and reused when it is filled
     * for another frame. std::nullopt returns to exact conversion. Must not be called
     * while frames are being extracted.
     */
    void set_lookup_frame(TimsDataHandle& TDH, std::optional<uint32_t> frame);

    //! Whether conversion uses the lookup table (and so does not call Bruker's library).
    bool uses_lookup_table() const { return use_lookup_table; }

    BrukerScan2InvIonMobilityConverter(TimsDataHandle& TDH, const std::string& lib_path, pressure_compensation_strategy pcs = NoPressureCompensation);
    ~BrukerScan2InvIonMobilityConverter();

    void convert(uint32_t frame_id, double* inv_ion_mobilities, const double* scans, uint32_t size) override final;
    void convert(uint32_t frame_id, double* inv_ion_mobilities, const uint32_t* scans, uint32_t size) override final;
    void inverse_convert(uint32_t frame_id, uint32_t* scans, const double* inv_ion_mobilities, uint32_t size) override final;

    std::string description() const override final;
};


/* ============================================================================================= */

class Scan2InvIonMobilityConverterFactory
{
 public:
    virtual std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs = NoPressureCompensation) = 0;
    virtual ~Scan2InvIonMobilityConverterFactory();
};

class ErrorScan2InvIonMobilityConverterFactory final : public Scan2InvIonMobilityConverterFactory
{
 public:
    static ErrorScan2InvIonMobilityConverterFactory& instance()
    {
        static ErrorScan2InvIonMobilityConverterFactory inst;
        return inst;
    }

    std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs = NoPressureCompensation) override final;
};

class BrukerScan2InvIonMobilityConverterFactory final : public Scan2InvIonMobilityConverterFactory
{
    const std::string dll_path;
    const LoadedLibraryHandle lib_hndl;
 public:
    BrukerScan2InvIonMobilityConverterFactory(const char* _dll_path);
    BrukerScan2InvIonMobilityConverterFactory(const std::string& _dll_path);
    static BrukerScan2InvIonMobilityConverterFactory& instance(const std::string& path);
    std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH, pressure_compensation_strategy pcs = NoPressureCompensation) override final;
};

class DefaultScan2InvIonMobilityConverterFactory final
{
    static std::unique_ptr<Scan2InvIonMobilityConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Scan2InvIonMobilityConverter> produceDefaultConverterInstance(TimsDataHandle& TDH, pressure_compensation_strategy pcs = NoPressureCompensation);

    template<class FactoryType, class... Args> static void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<Scan2InvIonMobilityConverterFactory, FactoryType>::value, "FactoryType must be a subclass of Scan2InvIonMobilityConverterFactory");
        fac_instance = std::make_unique<FactoryType>(std::forward<Args>(args)...);
    }
};

/**
 * Open-source scan-to-inverse-ion-mobility converter (linear model).
 *
 * 1/K0 = intercept + slope * scan_index
 *
 * where intercept = OneOverK0AcqRangeUpper and slope is derived from
 * the acquisition range and maximum number of scans per frame.
 */
class OpenSourceScan2ImConverter : public Scan2InvIonMobilityConverter
{
public:
    OpenSourceScan2ImConverter(double im_min, double im_max, uint32_t scan_max_index);

    void convert(uint32_t frame_id, double* inv_ion_mobilities, const double* scans, uint32_t size) override;
    void convert(uint32_t frame_id, double* inv_ion_mobilities, const uint32_t* scans, uint32_t size) override;
    void inverse_convert(uint32_t frame_id, uint32_t* scans, const double* inv_ion_mobilities, uint32_t size) override;
    std::string description() const override;

private:
    double intercept_;
    double slope_;
};

class OpenSourceScan2ImConverterFactory : public Scan2InvIonMobilityConverterFactory
{
public:
    static OpenSourceScan2ImConverterFactory& instance()
    {
        static OpenSourceScan2ImConverterFactory inst;
        return inst;
    }

    std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH,
        pressure_compensation_strategy pcs = NoPressureCompensation) override;
};
