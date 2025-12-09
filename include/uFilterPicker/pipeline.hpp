#ifndef UFILTER_PICKER_PIPELINE_HPP
#define UFILTER_PICKER_PIPELINE_HPP
#include <memory>
#include <vector>
#include <chrono>
namespace UFilterPicker
{
 class NarrowBandFilter;
 class Envelope;
 class CharacteristicFunction;
 class ThresholdTrigger;
}
namespace UFilterPicker
{
/// @class Pipeline
/// @brief A processing pipeline computes the narrow band filter of the input
///        signal, then computes the envelope, and lastly computes the
///        characteristic function.
/// @copyright Ben Baker (University of Utah) distributed under the MIT NO AI
///            license.
class Pipeline
{
    enum class ProcessingStage
    {
        Filter = 1,
        Envelope = 2,
        CharacteristicFunction = 3
    };
public:
    /// @brief Constructs the processing pipeline from the filter, envelope, and
    ///        characteristic function.
    Pipeline(std::unique_ptr<NarrowBandFilter> &&filter,
             std::unique_ptr<Envelope> &&envelope,
             std::unique_ptr<CharacteristicFunction> &&characteristicFunction);
    /// @result Gets the nominal signal sampling rate.
    [[nodiscard]] double getNominalSamplingRate() const; 
    /// @result Gets the approximate startup duration.
    [[nodiscard]] std::chrono::microseconds getStartUpDuration() const;
    /// @result The approximate delay introduced by the filtering.
    [[nodiscard]] std::chrono::microseconds getGroupDelay() const;
    /// @result True indicates the pipeline is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;
    /// @brief Computes the characteristic function from the input signal.
    [[nodiscard]] std::vector<double> apply(const std::vector<double> &x);
    [[nodiscard]] std::vector<double> apply(std::vector<double> &&x);
    /// @result Gets the output at the given stage.
    [[nodiscard]] std::vector<double> getStageOutput(const ProcessingStage stage) const;
    /// @brief Resets the filters on account of a gap. 
    void resetInitialConditions();
   
    /// @brief Destructor.
    ~Pipeline();
    Pipeline() = delete;
private:
    class PipelineImpl;
    std::unique_ptr<PipelineImpl> pImpl;
};
}
#endif
