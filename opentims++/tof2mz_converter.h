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

class Tof2MzConverter
{
 public:
    virtual void convert(uint32_t frame_id, double* mzs, const double* tofs, uint32_t size) = 0;
    virtual void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) = 0;
    virtual ~Tof2MzConverter() {};
    virtual std::string description() { return "Tof2MzConverter default"; };
};

class ErrorTof2MzConverter : public Tof2MzConverter
{
 public:
    ErrorTof2MzConverter(TimsDataHandle&) {};
    void convert(uint32_t, double*, const double*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }

    void convert(uint32_t, double*, const uint32_t*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    std::string description() override final { return "ErrorTof2MzConverter default"; };
};

class BrukerTof2MzConverter final : public Tof2MzConverter
{
    LoadedLibraryHandle lib_handle;
    uint64_t bruker_file_handle;

    tims_open_fun_t* tims_open;
    tims_get_last_error_string_fun_t* tims_get_last_error_string;
    tims_close_fun_t* tims_close;
    tims_convert_fun_t* tims_index_to_mz;

    std::string get_tims_error()
    {
        const size_t buf_size = 10000;
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_size);
        tims_get_last_error_string(buf.get(), buf_size-1);
        buf[buf_size-1] = '\0';
        return std::string(buf.get());
    }

 public:
    BrukerTof2MzConverter(TimsDataHandle& TDH, const std::string& lib_path) : lib_handle(lib_path), bruker_file_handle(0)
    {
        tims_open = lib_handle.symbol_lookup<tims_open_fun_t>("tims_open");
        tims_get_last_error_string = lib_handle.symbol_lookup<tims_get_last_error_string_fun_t>("tims_get_last_error_string");
        tims_close = lib_handle.symbol_lookup<tims_close_fun_t>("tims_close");
        tims_index_to_mz = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_index_to_mz");

        bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 0); // Recalibrated states not supported

        if(bruker_file_handle == 0)
            throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
    }

    ~BrukerTof2MzConverter() { if(bruker_file_handle != 0) tims_close(bruker_file_handle); }

    void convert(uint32_t frame_id, double* mzs, const double* tofs, uint32_t size) override final
    {
        tims_index_to_mz(bruker_file_handle, frame_id, tofs, mzs, size);
    }

    void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) override final
    {
        std::unique_ptr<double[]> dbl_tofs = std::make_unique<double[]>(size);
        for(uint32_t idx = 0; idx < size; idx++)
            dbl_tofs[idx] = static_cast<double>(tofs[idx]);
        tims_index_to_mz(bruker_file_handle, frame_id, dbl_tofs.get(), mzs, size);
    }

    std::string description() override final { return "BrukerTof2MzConverter"; };
};

class Tof2MzConverterFactory
{
 public:
    virtual std::unique_ptr<Tof2MzConverter> produce(TimsDataHandle& TDH) = 0;
    virtual ~Tof2MzConverterFactory() {};
};

class ErrorTof2MzConverterFactory final : public Tof2MzConverterFactory
{
 public:
    std::unique_ptr<Tof2MzConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<ErrorTof2MzConverter>(TDH); };
};

class BrukerTof2MzConverterFactory final : public Tof2MzConverterFactory
{
    const std::string dll_path;
    LoadedLibraryHandle lib_hndl;
 public:
    BrukerTof2MzConverterFactory(const char* _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {};
    BrukerTof2MzConverterFactory(const std::string& _dll_path) : dll_path(_dll_path), lib_hndl(_dll_path) {};
    std::unique_ptr<Tof2MzConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<BrukerTof2MzConverter>(TDH, dll_path.c_str()); };
};

class DefaultTof2MzConverterFactory final
{
    static std::unique_ptr<Tof2MzConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Tof2MzConverter> produceDefaultConverterInstance(TimsDataHandle& TDH)
    {
        if(!fac_instance)
            fac_instance = std::make_unique<ErrorTof2MzConverterFactory>();

        return fac_instance->produce(TDH);
    }

    template<class FactoryType, class... Args> static void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<Tof2MzConverterFactory, FactoryType>::value, "FactoryType must be a subclass of Tof2MzConverterFactory");
        fac_instance = std::make_unique<FactoryType>(std::forward<Args...>(args...));
    }
};
