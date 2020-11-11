#include <memory>
#include "opentims.h"

class Tof2MZConverter
{
 public:
    virtual void convert(double* mzs, const uint32_t* tofs, uint32_t frame_id) = 0;
};

class ErrorTof2MZConverter : public Tof2MZConverter
{
 public:
    ErrorTof2MZConverter(TimsDataHandle& TDH) {};
    void convert(double* mzs, const uint32_t* tofs, uint32_t frame_id) override final
    {
        throw std::logic_error("Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor");
    }
};

class ConverterFactory
{
 public:
    virtual std::unique_ptr<Tof2MZConverter> produce(TimsDataHandle& TDH) = 0;
};

class ErrorConverterFactory : public ConverterFactory
{
 public:
    virtual std::unique_ptr<Tof2MZConverter> produce(TimsDataHandle& TDH) override final { return std::make_unique<ErrorTof2MZConverter>(); };
};

class DefaultConverterFactory
{
    static std::unique_ptr<ConverterFactory> fac_instance;
 public:
    static std::unique_ptr<Tof2MZConverter> produceDefaultConverterInstance(TimsDataHandle& TDH)
    {
        if(!fac_instance)
            fac_instance = std::make_unique<ErrorConverterFactory>();

        return fac_instance->produce(TDH);
    }

    template<class FactoryType, class... Args> void setAsDefault(Args&& ... args)
    {
        static_assert(std::is_base_of<ConverterFactory, FactoryType>::value);
        fac_instance = std::make_unique<FactoryType>(std::forward(args...));
    }
};

