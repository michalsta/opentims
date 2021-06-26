/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2021 Michał Startek and Mateusz Łącki
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

#include <memory>
#include "bruker.h"
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
    virtual ~Scan2InvIonMobilityConverter() {};
    virtual std::string description() { return "Scan2InvIonMobilityConverter default"; };
};

class ErrorScan2InvIonMobilityConverter : public Scan2InvIonMobilityConverter
{
 public:
    ErrorScan2InvIonMobilityConverter(TimsDataHandle&) {};
    void convert(uint32_t, double*, const double*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    void convert(uint32_t, double*, const uint32_t*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    std::string description() override final { return "ErrorScan2InvIonMobilityConverter default"; };
};

class BrukerScan2InvIonMobilityConverter final : public Scan2InvIonMobilityConverter
{
    LoadedLibraryHandle lib_handle;
    uint64_t bruker_file_handle;

    tims_open_fun_t* tims_open;
    tims_get_last_error_string_fun_t* tims_get_last_error_string;
    tims_close_fun_t* tims_close;
    tims_convert_fun_t* tims_scannum_to_inv_ion_mobility;

    std::string get_tims_error()
    {
        const size_t buf_size = 10000;
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_size);
        tims_get_last_error_string(buf.get(), buf_size-1);
        buf[buf_size-1] = '\0';
        return std::string(buf.get());
    }

 public:
    BrukerScan2InvIonMobilityConverter(TimsDataHandle& TDH, const std::string& lib_path) : lib_handle(lib_path), bruker_file_handle(0)
    {
        tims_open = lib_handle.symbol_lookup<tims_open_fun_t>("tims_open");
        tims_get_last_error_string = lib_handle.symbol_lookup<tims_get_last_error_string_fun_t>("tims_get_last_error_string");
        tims_close = lib_handle.symbol_lookup<tims_close_fun_t>("tims_close");
        tims_scannum_to_inv_ion_mobility = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_scannum_to_oneoverk0");

        bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 0); // Recalibrated states not supported

        if(bruker_file_handle == 0)
            throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
    }

    ~BrukerScan2InvIonMobilityConverter() { if(bruker_file_handle != 0) tims_close(bruker_file_handle); }

    void convert(uint32_t frame_id,
                 double* inv_ion_mobilities,
                 const double* scans,
                 uint32_t size) override final
    {
        tims_scannum_to_inv_ion_mobility(bruker_file_handle, frame_id, scans, inv_ion_mobilities, size);
    }


    void convert(uint32_t frame_id,
                 double* inv_ion_mobilities,
                 const uint32_t* scans,
                 uint32_t size) override final
    {
        std::unique_ptr<double[]> dbl_scans = std::make_unique<double[]>(size);
        for(uint32_t idx = 0; idx < size; idx++)
            dbl_scans[idx] = static_cast<double>(scans[idx]);
        tims_scannum_to_inv_ion_mobility(bruker_file_handle, frame_id, dbl_scans.get(), inv_ion_mobilities, size);
    }


    std::string description() override final { return "BrukerScan2InvIonMobilityConverter"; };
};

class Scan2InvIonMobilityConverterFactory
{
 public:
    virtual std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH) = 0;
    virtual ~Scan2InvIonMobilityConverterFactory() {};
};

class ErrorScan2InvIonMobilityConverterFactory final : public Scan2InvIonMobilityConverterFactory
{
 public:
    std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH) override final
    { return std::make_unique<ErrorScan2InvIonMobilityConverter>(TDH); };
};

class BrukerScan2InvIonMobilityConverterFactory final : public Scan2InvIonMobilityConverterFactory
{
    const std::string dll_path;
    LoadedLibraryHandle lib_hndl;
 public:
    BrukerScan2InvIonMobilityConverterFactory(const char* _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {};
    BrukerScan2InvIonMobilityConverterFactory(const std::string& _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {};
    std::unique_ptr<Scan2InvIonMobilityConverter> produce(TimsDataHandle& TDH) override final
    { return std::make_unique<BrukerScan2InvIonMobilityConverter>(TDH, dll_path.c_str()); };
};

class DefaultScan2InvIonMobilityConverterFactory final
{
    static std::unique_ptr<Scan2InvIonMobilityConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Scan2InvIonMobilityConverter> produceDefaultConverterInstance(TimsDataHandle& TDH)
    {
        if(!fac_instance)
            fac_instance = std::make_unique<ErrorScan2InvIonMobilityConverterFactory>();

        return fac_instance->produce(TDH);
    }

    template<class FactoryType, class... Args> static void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<Scan2InvIonMobilityConverterFactory, FactoryType>::value, "FactoryType must be a subclass of Scan2InvIonMobilityConverterFactory");
        fac_instance = std::make_unique<FactoryType>(std::forward<Args...>(args...));
    }
};
