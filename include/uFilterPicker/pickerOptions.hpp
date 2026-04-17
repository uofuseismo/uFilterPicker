#ifndef UFILTER_PICKER_PICKER_OPTIONS_HPP
#define UFILTER_PICKER_PICKER_OPTIONS_HPP
#include <memory>
#include <chrono>
namespace UFilterPicker
{
/// @class PickerOptions
/// @brief Defines the general options for the filter picker.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class PickerOptions
{
public:
    /// @brief Constructor.
    PickerOptions();
    /// @brief Copy constructor.
    PickerOptions(const PickerOptions &options);
    /// @brief Move constructor.
    PickerOptions(PickerOptions &&options) noexcept;

    /// @brief The detector relies on online filtering techniques.  
    ///        If the gap between successive packets exceeds this many samples
    ///        then the detector will be restarted.  Otherwise, if the gap
    ///        is less than this then the acquired signal will be considered
    ///        continuous.
    /// @param[in] gapSize  The maximum gap size in samples.
    void setGapTolerance(int gapSize);
    /// @result The maximum gap size in samples.
    [[nodiscard]] int getGapTolerance() const noexcept;

    /// @brief If packets have samples greater than now + maximumFutureTime
    ///        then the packet will be ignored.  This is indicative of a 
    ///        clock error. 
    /// @param[in] maximumFutureTime  The maximum future time in seconds.
    void setMaximumFutureTime(const std::chrono::microseconds &maximumFutureTime) noexcept;
    /// @result The maximum future time such that all packets are required
    ///         to have data before now + maximumFutureTime.
    [[nodiscard]] std::chrono::microseconds getMaximumFutureTime() const noexcept;

    /// @brief Sets the maximum latency.
    /// @param[in] maximumLatency  If the latency exceeds this value then
    ///                            the packet is ignored.     
    /// @throws std::invalid_argument if maximumLatency is not positive.
    void setMaximumLatency(const std::chrono::microseconds &maximumLatency);
    /// @result Packets more latent than this time will not be processed.
    /// @note The default is 15 minutes after which point the data is likely
    ///       of no value for (near) real-time processing.
    [[nodiscard]] std::chrono::microseconds getMaximumLatency() const noexcept; 

    /// @brief Sets the picker burn-in time factor.  The picker will be able
    ///        to begin making picks after this factor x filter group delay.
    ///        This helps suppress spurious artifacts on start-up.
    /// @param[in] burnInFactor   The burn in factor.
    void setBurnInFactor(int burnInFactor); 
    /// @result The burn-in factor.  
    /// @note By default this is 3. 
    [[nodiscard]] int getBurnInFactor() const noexcept;

    /// @brief Copy assignment.
    PickerOptions &operator=(const PickerOptions &);
    /// @brief Move assignment.
    PickerOptions &operator=(PickerOptions &&options) noexcept;
    /// @brief Destructor.
    ~PickerOptions();
private:
    class PickerOptionsImpl;
    std::unique_ptr<PickerOptionsImpl> pImpl;
};

}
#endif
