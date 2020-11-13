#pragma once

#include <memory>
#include <dlfcn.h>
#include "opentims.h"
#include "bruker.h"

class Tof2MZConverter
{
 public:
    virtual void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) = 0;
    virtual ~Tof2MZConverter() {};
};

class ErrorTof2MZConverter : public Tof2MZConverter
{
 public:
    ErrorTof2MZConverter([[maybe_unused]] TimsDataHandle& TDH) {};
    void convert([[maybe_unused]] uint32_t frame_id, [[maybe_unused]] double* mzs, [[maybe_unused]] const uint32_t* tofs, [[maybe_unused]] uint32_t size) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
};

class BrukerTof2MZConverter final : public Tof2MZConverter
{
    void* dllhandle;
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

    void* symbol_lookup(const char* symbol_name)
    {
        void* ret = dlsym(dllhandle, symbol_name); // nullptr might be a valid result here, got to check dlerror...
        const char* errmsg = dlerror();
        if(errmsg != nullptr)
            throw std::runtime_error(std::string("Symbol lookup failed for ") + symbol_name + ", reason: " + errmsg);
        return ret;
    };

 public:
    BrukerTof2MZConverter(TimsDataHandle& TDH, const char* dll_path) : dllhandle(dlopen(dll_path, RTLD_LAZY)), bruker_file_handle(0)
    {
        // Re-dlopening the dll_path to increase refcount, so nothing horrible happens even if factory is deleted
        if(dllhandle == nullptr)
            throw std::runtime_error(std::string("dlopen(") + dll_path + ") failed, reason: " + dlerror());

        dlerror(); // Clear errors
        tims_open = reinterpret_cast<tims_open_fun_t*>(symbol_lookup("tims_open"));
        tims_get_last_error_string = reinterpret_cast<tims_get_last_error_string_fun_t*>(symbol_lookup("tims_get_last_error_string"));
        tims_close = reinterpret_cast<tims_close_fun_t*>(symbol_lookup("tims_close"));
        tims_index_to_mz = reinterpret_cast<tims_convert_fun_t*>(symbol_lookup("tims_index_to_mz"));

        bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 0); // Recalibrated states not supported

        if(bruker_file_handle == 0)
            throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
    }

    ~BrukerTof2MZConverter() { dlclose(dllhandle); if(bruker_file_handle != 0) tims_close(bruker_file_handle); }

    void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) override final
    {
        std::unique_ptr<double[]> dbl_tofs = std::make_unique<double[]>(size);
        for(uint32_t idx = 0; idx < size; idx++)
            dbl_tofs[idx] = static_cast<double>(tofs[idx]);
        tims_index_to_mz(bruker_file_handle, frame_id, dbl_tofs.get(), mzs, size);
    }
};

class ConverterFactory
{
 public:
    virtual std::unique_ptr<Tof2MZConverter> produce(TimsDataHandle& TDH) = 0;
    virtual ~ConverterFactory() {};
};

class ErrorConverterFactory final : public ConverterFactory
{
 public:
    std::unique_ptr<Tof2MZConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<ErrorTof2MZConverter>(TDH); };
};

class BrukerConverterFactory final : public ConverterFactory
{
    const std::string dll_path;
 public:
    BrukerConverterFactory(const char* _dll_path) : dll_path(_dll_path) {};
    BrukerConverterFactory(const std::string& _dll_path) : dll_path(_dll_path) {};
    std::unique_ptr<Tof2MZConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<BrukerTof2MZConverter>(TDH, dll_path.c_str()); };
};

class DefaultConverterFactory final
{
    static std::unique_ptr<ConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Tof2MZConverter> produceDefaultConverterInstance(TimsDataHandle& TDH)
    {
        if(!fac_instance)
            fac_instance = std::make_unique<ErrorConverterFactory>();

        return fac_instance->produce(TDH);
    }

    template<class FactoryType, class... Args> static void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<ConverterFactory, FactoryType>::value, "FactoryType must be a subclass of ConverterFactory");
        fac_instance = std::make_unique<FactoryType>(std::forward<Args...>(args...));
    }
};
