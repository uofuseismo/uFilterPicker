#ifndef UFILTER_PICKER_DETECTOR_HPP
#define UFILTER_PICKER_DETECTOR_HPP
#include <memory>
#include <vector>
#include <chrono>
namespace UFilterPicker
{
 class Pipeline;
}
namespace UFilterPicker
{
/// @class Detector
/// @brief Transforms an input signal into a characteristic function.
///        This works by a series of pipelines where each pipeline
///        applies a narrow band filter, computes an envelope, and
///        then computes the characteristic function.  The characteristic
///        function from each pipeline is then aggregated into a single
///        characteristic function.  The resulting characteristic function
///        can then be used by a triggering algorithm.
/// @copyright Ben Baker (University of Utah) distributed under the MIT license.
class Detector
{
public:
    enum class Strategy
    {
        Max,
        Average
    }; 
public:
    explicit Detector(std::vector<std::unique_ptr<Pipeline>> &&pipeline,
                      Strategy strategy = Strategy::Max);
    /// @result The nominal sampling rate of the input signal in Hz.
    [[nodiscard]] double getNominalSamplingRate() const; 
    /// @result True indicates the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;
    /// @result The approximate group delay introduced by filtering.
    [[nodiscard]] std::chrono::microseconds getGroupDelay() const;
    /// @brief Computes the characteristic function according to the
    ///        input signal. 
    [[nodiscard]] std::vector<double> apply(const std::vector<double> &x);
    [[nodiscard]] std::vector<double> apply(const std::vector<int> &x);
    /// @brief Resets the filters in case of a gap.
    void resetInitialConditions();

    /// @brief Creates the standard 100 Hz broadband detector.
    static std::unique_ptr<UFilterPicker::Detector> create100HzBroadband();
    /// @brief Creates the standard 40 Hz broadband detector.
    static std::unique_ptr<UFilterPicker::Detector> create40HzBroadband();

    /// @brief Destructor.
    ~Detector();

    Detector(const Detector &) = delete;
    Detector& operator=(const Detector &) = delete;
    Detector() = delete;
private:
    class DetectorImpl;
    std::unique_ptr<DetectorImpl> pImpl;
};
}
#endif
