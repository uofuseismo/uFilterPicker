#ifndef UFILTER_PICKER_THRESHOLD_TRIGGER_HPP
#define UFILTER_PICKER_THRESHOLD_TRIGGER_HPP
#include <memory>
#include <vector>
#include <chrono>
namespace UFilterPicker
{
/// @class ThresholdTrigger
/// @brief Defines a threshold trigger.  The trigger is activated when
///        the characteristic signal exceeds the on threshold and is
///        deactivated when the threshold drops below the off threshold.
/// @copyright Ben Baker (University of Utah) disdtributed under the
///            MIT NO AI license. 
class ThresholdTrigger
{
public:
    /// @brief Constructs the threshold trigger.
    /// @param[in] onAndOffThresold  The trigger is activitated when the
    ///                              characterstic signal exceeds
    ///                              onAndOffThreshold.first.  No further picks
    ///                              can be made until the trigger window is
    ///                              deactivated when the characteristic
    ///                              function falls below
    ///                              onAndOffThreshold.second.
    explicit ThresholdTrigger(const std::pair<double, double> &onAndOffThreshold);
    /// @result True indicates that the window is activated.
    [[nodiscard]] bool isInitialized() const noexcept;

    [[nodiscard]] std::vector<std::chrono::microseconds>
         apply(std::vector<double> &&x,
               const std::chrono::microseconds &packetStartTime,
               double packetSamplingRate);
    [[nodiscard]] std::vector<std::chrono::microseconds>
         apply(const std::vector<double> &x,
               const std::chrono::microseconds &packetStartTime,
               double packetSamplingRate);
    /// @result A binary signal corresponding to the input packet.  When
    ///         a sample is 0 the trigger is unlocked and when a sample is 1
    ///         the trigger is locked.  Picks occur when the signal changes
    ///         from 0 to 1.  This corresponds to the input characteristic
    ///         function exceeding the on threshold (0 to 1) then dropping
    ///         below the off threshold (1 to 0). 
    /// @note This is for debugging and should be used after \c apply().
    [[nodiscard]] std::vector<uint8_t> getOutput() const;
    /// @result The trigger is activated when the signal is 1.  Otherwise,
    ///         the trigger is waiting to be activated.  Picks should occur
    ///         when the result switches state from 0 to 1. 
    //[[nodiscard]] std::vector<uint8_t> apply(std::vector<double> &&x);
    //[[nodiscard]] std::vector<uint8_t> apply(const std::vector<double> &x);

    /// @brief Resets threshold trigger to an unarmed state.
    void resetInitialConditions();

    /// @brief Destructor.
    ~ThresholdTrigger();
    ThresholdTrigger() = delete;
    ThresholdTrigger(const ThresholdTrigger &trigger);
    ThresholdTrigger& operator=(const ThresholdTrigger &trigger) = delete;
    /// @brief Move constructor.
    ThresholdTrigger(ThresholdTrigger &&trigger) noexcept;
    /// @brief Move assignment.
    ThresholdTrigger& operator=(ThresholdTrigger &&trigger) noexcept;
    
private:
    class ThresholdTriggerImpl;
    std::unique_ptr<ThresholdTriggerImpl> pImpl;
};
[[nodiscard]] std::vector<std::chrono::microseconds> 
    triggerSignalToPicks(const std::vector<uint8_t> &triggerSignal,
                         const std::chrono::microseconds &packetStartTime, 
                         double packetSamplingRate,
                         uint8_t lastTriggerSignalSample = 0);
}
#endif
