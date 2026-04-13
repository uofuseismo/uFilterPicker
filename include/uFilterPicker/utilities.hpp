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

[[nodiscard]]
std::vector<double> toDoubleVector(
    const UDataPacketServiceAPI::V1::Packet &packet,
    bool swapBytes = (std::endian::native == std::endian::little ? false : true));

[[nodiscard]]
std::string toString(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifier);
[[nodiscard]]
std::string toString(const UDataPacketServiceAPI::V1::Packet &packet);

template<typename T>
T getStartTime(const UDataPacketServiceAPI::V1::Packet &packet);

template<typename T>
T getEndTime(const UDataPacketServiceAPI::V1::Packet &packet);

[[nodiscard]]
bool consistentSamplingRate(double nominalSamplingRate,
                            double packetSamplingRate);


}

#endif
