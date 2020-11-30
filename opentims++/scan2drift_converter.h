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

class Scan2DriftConverter
{
 public:
    virtual void convert(uint32_t frame_id, double* drifts, const double* scans, uint32_t size) = 0;
    virtual void convert(uint32_t frame_id, double* drifts, const uint32_t* scans, uint32_t size) = 0;
    virtual ~Scan2DriftConverter() {};
    virtual std::string description() { return "Scan2DriftConverter default"; };
};

class ErrorScan2DriftConverter : public Scan2DriftConverter
{
 public:
    ErrorScan2DriftConverter(TimsDataHandle&) {};
    void convert(uint32_t, double*, const double*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    void convert(uint32_t, double*, const uint32_t*, uint32_t) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    std::string description() override final { return "ErrorScan2DriftConverter default"; };
};

class BrukerScan2DriftConverter final : public Scan2DriftConverter
{
    LoadedLibraryHandle lib_handle;
    uint64_t bruker_file_handle;

    tims_open_fun_t* tims_open;
    tims_get_last_error_string_fun_t* tims_get_last_error_string;
    tims_close_fun_t* tims_close;
    tims_convert_fun_t* tims_scannum_to_drift;

    std::string get_tims_error()
    {
        const size_t buf_size = 10000;
        std::unique_ptr<char[]> buf = std::make_unique<char[]>(buf_size);
        tims_get_last_error_string(buf.get(), buf_size-1);
        buf[buf_size-1] = '\0';
        return std::string(buf.get());
    }

 public:
    BrukerScan2DriftConverter(TimsDataHandle& TDH, const std::string& lib_path) : lib_handle(lib_path), bruker_file_handle(0)
    {
        tims_open = lib_handle.symbol_lookup<tims_open_fun_t>("tims_open");
        tims_get_last_error_string = lib_handle.symbol_lookup<tims_get_last_error_string_fun_t>("tims_get_last_error_string");
        tims_close = lib_handle.symbol_lookup<tims_close_fun_t>("tims_close");
        tims_scannum_to_drift = lib_handle.symbol_lookup<tims_convert_fun_t>("tims_scannum_to_oneoverk0");

        bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 0); // Recalibrated states not supported

        if(bruker_file_handle == 0)
            throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
    }

    ~BrukerScan2DriftConverter() { if(bruker_file_handle != 0) tims_close(bruker_file_handle); }

    void convert(uint32_t frame_id, double* drifts, const double* scans, uint32_t size) override final
    {
        tims_scannum_to_drift(bruker_file_handle, frame_id, scans, drifts, size);
    }
    void convert(uint32_t frame_id, double* drifts, const uint32_t* scans, uint32_t size) override final
    {
        std::unique_ptr<double[]> dbl_scans = std::make_unique<double[]>(size);
        for(uint32_t idx = 0; idx < size; idx++)
            dbl_scans[idx] = static_cast<double>(scans[idx]);
        tims_scannum_to_drift(bruker_file_handle, frame_id, dbl_scans.get(), drifts, size);
    }

    std::string description() override final { return "BrukerScan2DriftConverter"; };
};

class Scan2DriftConverterFactory
{
 public:
    virtual std::unique_ptr<Scan2DriftConverter> produce(TimsDataHandle& TDH) = 0;
    virtual ~Scan2DriftConverterFactory() {};
};

class ErrorScan2DriftConverterFactory final : public Scan2DriftConverterFactory
{
 public:
    std::unique_ptr<Scan2DriftConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<ErrorScan2DriftConverter>(TDH); };
};

class BrukerScan2DriftConverterFactory final : public Scan2DriftConverterFactory
{
    const std::string dll_path;
 public:
    BrukerScan2DriftConverterFactory(const char* _dll_path) : dll_path(_dll_path) {};
    BrukerScan2DriftConverterFactory(const std::string& _dll_path) : dll_path(_dll_path) {};
    std::unique_ptr<Scan2DriftConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<BrukerScan2DriftConverter>(TDH, dll_path.c_str()); };
};

class DefaultScan2DriftConverterFactory final
{
    static std::unique_ptr<Scan2DriftConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Scan2DriftConverter> produceDefaultConverterInstance(TimsDataHandle& TDH)
    {
        if(!fac_instance)
            fac_instance = std::make_unique<ErrorScan2DriftConverterFactory>();

        return fac_instance->produce(TDH);
    }

    template<class FactoryType, class... Args> static void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<Scan2DriftConverterFactory, FactoryType>::value, "FactoryType must be a subclass of Scan2DriftConverterFactory");
        fac_instance = std::make_unique<FactoryType>(std::forward<Args...>(args...));
    }
};
