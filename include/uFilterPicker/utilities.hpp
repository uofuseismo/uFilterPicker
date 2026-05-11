#ifndef UFILTER_PICKER_UTILITIES_HPP
#define UFILTER_PICKER_UTILITIES_HPP
#include <bit>
#include <vector>
#include <string>
#include <chrono>

namespace UDataPacketServiceAPI::V1
{
 class Packet;
 class StreamIdentifier;
}
namespace UFilterPickerPickBrokerAPI::V1
{
 class Algorithm;
 class Pick;
 class StreamIdentifier;
}

namespace UFilterPicker::Utilities
{

/// @result A convenience function to convert the data on the packet to
///         a double vector for processing.
[[nodiscard]]
std::vector<double> toDoubleVector(
    const UDataPacketServiceAPI::V1::Packet &packet,
    bool swapBytes = (std::endian::native == std::endian::little ? false : true));

[[nodiscard]]
std::string toString(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifier);
[[nodiscard]]
std::string toString(const UDataPacketServiceAPI::V1::Packet &packet);

/// @result The current time (microseconds or nanoseconds since the epoch).
template<typename T> T getNow();

/// @result The packet start time (microseconds or nanoseconds).
template<typename T>
T getStartTime(const UDataPacketServiceAPI::V1::Packet &packet);

/// @result The packet end time (microseconds or nanoseconds).
template<typename T>
T getEndTime(const UDataPacketServiceAPI::V1::Packet &packet);

/// @param[in] nominalSamplingRate  The sampling rate from the metadata in Hz.
/// @param[in] packetSamplingRate   The sampling rate estimated from the
///                                 digitizer's clock.  This usually wobbles
///                                 a tiny amount around the nominal sampling
///                                 rate.
/// @result True indicates the sampling rates are consistent.
[[nodiscard]]
bool consistentSamplingRate(double nominalSamplingRate,
                            double packetSamplingRate);

[[nodiscard]]
int getGapSizeInSamples(const std::chrono::microseconds &packetStartTime,
                        const double packetSamplingRateHZ,
                        const std::chrono::microseconds &latestSampleTime);

/// @brief Utility routine for trimming data so that it starts after a certain
///        amount of time.
/// @param[in] desiredStartTime      The desired start time.
/// @param[in] inputData             The data to trim.
/// @param[in] startTime             The start time of the input data.
/// @param[in] packetSamplingRateHz  The packet sampling rate in Hz.
/// @result result.first is the trimmed data adn result.second is the 
///         corresponding start time of the data.  The start time will
///         be at least the desiredStartTime.
[[nodiscard]]
std::pair<std::vector<double>, std::chrono::microseconds> 
leftTrim(const std::chrono::microseconds &desiredStartTime,
         const std::vector<double> &inputData,
         const std::chrono::microseconds &startTime,
         const double packetSamplingRateHz);

/// @brief Creates a P pick message.
[[nodiscard]] 
UFilterPickerPickBrokerAPI::V1::Pick 
    toPPick(const std::chrono::microseconds &pickTime,
            const UFilterPickerPickBrokerAPI::V1::StreamIdentifier &identifier,
            const UFilterPickerPickBrokerAPI::V1::Algorithm &algorithm);
[[nodiscard]]
UFilterPickerPickBrokerAPI::V1::StreamIdentifier convertIdentifier(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifierIn);

}

#endif
