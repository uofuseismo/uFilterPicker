#ifndef UFILTER_PICKER_PICKER_HPP
#define UFILTER_PICKER_PICKER_HPP
#include <vector>
#include <memory>
#include <string>
#include <spdlog/logger.h>

namespace UDataPacketServiceAPI
{
 namespace V1
 { 
  class Packet;
  class StreamIdentifier;
 }
}
namespace UFilterPicker
{
 class Detector;
 class ThresholdTrigger; 
}
namespace UFilterPicker
{
/// @class Picker
/// @brief The picker is a utility that combines detection and triggering into
///        the high-level action of creating a pick.
/// @copyright Ben Baker (University of Utah) distributed under the MIT NO AI
///            license.
class Picker
{
public:
    /// @brief Constructs the picker from the stream identifier,
    ///        a detector, trigger, logger, and nominal sampling rate. 
    Picker(const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier,
           std::unique_ptr<UFilterPicker::Detector> &&detector,
           std::unique_ptr<UFilterPicker::ThresholdTrigger> &&trigger,
           std::shared_ptr<spdlog::logger> logger,
           double nominalSamplingRate);

    /// @result True indicates the picker is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    void apply(const UDataPacketServiceAPI::V1::Packet &packet);
    void apply(UDataPacketServiceAPI::V1::Packet &&packet);

    /// @result The picker's identifier string.
    [[nodiscard]] std::string getIdentifierString() const;

    ~Picker();
    Picker() = delete;
    Picker(const Picker &) = delete;
    Picker(Picker &&) noexcept;
private:
    class PickerImpl;
    std::unique_ptr<PickerImpl> pImpl;
};
}
#endif
