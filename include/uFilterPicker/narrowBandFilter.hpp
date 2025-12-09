#ifndef UFILTER_PICKER_NARROW_BAND_FILTER_HPP
#define UFILTER_PICKER_NARROW_BAND_FILTER_HPP
#include <vector>
#include <memory>
#include <chrono>
namespace UFilterPicker
{
/// @class NarrowBandFilter
/// @brief Defines a narrow bandpass Butterworth filter.
/// @copyright Ben Baker (University of Utah) disdtributed under the
///            MIT NO AI license. 
class NarrowBandFilter
{
public:
    /// @brief Constructs the narrow bandpass filter.
    /// @param[in] order     The filter order.
    /// @param[in] passBand  The passband of the filter.  passBand.first
    ///                      is the low corner frequency in Hz and
    ///                      passBand.second is the high corner frequency
    ///                      in Hz.
    /// @param[in] samplingRate  The system sampling rate in Hz.
    NarrowBandFilter(int order,
                     const std::pair<double, double> &passBand,
                     double samplingRate);

    /// @result True indicates the filter is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    /// @brief Applies the filter.
    /// @param[in] x   The filter to signal.
    [[nodiscard]] std::vector<double> apply(const std::vector<double> &x);
    /// @brief Applies the filter.
    [[nodiscard]] std::vector<double> apply(std::vector<double> &&x);
    /// @result The output of the filter.  This is for debugging.
    [[nodiscard]] std::vector<double> getOutput() const;

    /// @brief Resets the filter in case of a gap.
    void resetInitialConditions();

    /// @result The filter order.
    [[nodiscard]] int getOrder() const;

    /// @result The number of second order sections.
    [[nodiscard]] int getNumberOfCascades() const;

    /// @result The nominal sampling rate in Hz.
    [[nodiscard]] double getNominalSamplingRate() const; 

    /// @result The group delay.
    //[[nodiscard]] std::chrono::microseconds getGroupDelay() const;

    /// @result The filter passband.
    [[nodiscard]] std::pair<double, double> getPassBand() const;

    /// @result Gets the filter's frequency repsonse.
    [[nodiscard]] std::pair<std::vector<double>, std::vector<double>> computeResponse(const int nFrequencies = 200) const;
    /// @result Gets the filter's frequency response at the selected frequencies.
    [[nodiscard]] std::vector<double> computeResponse(const std::vector<double> &frequencies) const;

    /// @brief 
    ~NarrowBandFilter();


    NarrowBandFilter() = delete;
    NarrowBandFilter& operator=(const NarrowBandFilter &f);
    NarrowBandFilter& operator=(NarrowBandFilter &&f) noexcept;
private:
    class NarrowBandFilterImpl; 
    std::unique_ptr<NarrowBandFilterImpl> pImpl;
};
}
#endif
