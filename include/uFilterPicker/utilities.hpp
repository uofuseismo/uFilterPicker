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

}

#endif
