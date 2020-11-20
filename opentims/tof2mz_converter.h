#pragma once

#include <memory>
#include <dlfcn.h>
#include "opentims.h"
#include "bruker.h"

class Tof2MzConverter
{
 public:
    virtual void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) = 0;
    virtual ~Tof2MzConverter() {};
    virtual std::string description() { return "Tof2MzConverter default"; };
};

class ErrorTof2MzConverter : public Tof2MzConverter
{
 public:
    ErrorTof2MzConverter([[maybe_unused]] TimsDataHandle& TDH) {};
    void convert([[maybe_unused]] uint32_t frame_id, [[maybe_unused]] double* mzs, [[maybe_unused]] const uint32_t* tofs, [[maybe_unused]] uint32_t size) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
    std::string description() override final { return "ErrorTof2MzConverter default"; };
};

class BrukerTof2MzConverter final : public Tof2MzConverter
{
    void* dllhandle;
    uint64_t bruker_file_handle;
    const std::string so_path;

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
    BrukerTof2MzConverter(TimsDataHandle& TDH, const char* dll_path) : dllhandle(dlopen(dll_path, RTLD_LAZY)), bruker_file_handle(0), so_path(dll_path)
    {
        std::cerr << "Start\n";
        // Re-dlopening the dll_path to increase refcount, so nothing horrible happens even if factory is deleted
        if(dllhandle == nullptr)
            throw std::runtime_error(std::string("dlopen(") + dll_path + ") failed, reason: " + dlerror());

        dlerror(); // Clear errors
        tims_open = reinterpret_cast<tims_open_fun_t*>(symbol_lookup("tims_open"));
        tims_get_last_error_string = reinterpret_cast<tims_get_last_error_string_fun_t*>(symbol_lookup("tims_get_last_error_string"));
        tims_close = reinterpret_cast<tims_close_fun_t*>(symbol_lookup("tims_close"));
        tims_index_to_mz = reinterpret_cast<tims_convert_fun_t*>(symbol_lookup("tims_index_to_mz"));

        std::cerr << "1\n";
        bruker_file_handle = (*tims_open)(TDH.tims_dir_path.c_str(), 0); // Recalibrated states not supported
        std::cerr << "2\n";

        if(bruker_file_handle == 0)
            throw std::runtime_error("tims_open(" + TDH.tims_dir_path + ") failed. Reason: " + get_tims_error());
    }

    ~BrukerTof2MzConverter() { dlclose(dllhandle); if(bruker_file_handle != 0) tims_close(bruker_file_handle); }

    void convert(uint32_t frame_id, double* mzs, const uint32_t* tofs, uint32_t size) override final
    {
        std::unique_ptr<double[]> dbl_tofs = std::make_unique<double[]>(size);
        for(uint32_t idx = 0; idx < size; idx++)
            dbl_tofs[idx] = static_cast<double>(tofs[idx]);
        tims_index_to_mz(bruker_file_handle, frame_id, dbl_tofs.get(), mzs, size);
    }

    std::string description() override final { return "BrukerTof2MzConverter, shared lib path:" + so_path; };
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
 public:
    BrukerTof2MzConverterFactory(const char* _dll_path) : dll_path(_dll_path) {};
    BrukerTof2MzConverterFactory(const std::string& _dll_path) : dll_path(_dll_path) {};
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
