#ifndef UFILTER_PICKER_OPTIONS_HPP
#define UFILTER_PICKER_OPTIONS_HPP
#include <chrono>
#include <memory>
namespace UFilterPicker
{
class Options
{
public:
    Options();

    void setNominalSamplingRate(double samplingRate);
    [[nodiscard]] double getNominalSamplingRate() const; 
    [[nodiscard]] bool haveNominalSamplingRate() const noexcept;

    ~Options();
private:
    class OptionsImpl;
    std::unique_ptr<OptionsImpl> pImpl;
};
}
#endif
