module;
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <chrono>
#include <cmath>
#include <bit>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include <google/protobuf/util/time_util.h>
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"

export module Utilities;

namespace
{

template<typename T>
std::vector<double>
    unpackToDouble(const std::string &data, const uint32_t nSamples,
                   const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::vector<double> result;
    if (nSamples < 1){return result;}
    if (static_cast<size_t> (nSamples)*dataTypeSize != data.size())
    {   
        throw std::invalid_argument("Unexpected data size");
    }   
    result.resize(nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        unsigned char cArray[dataTypeSize];
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::copy(data.data() + i1, data.data() + i2, 
                      cvUnion.cArray);
            result[i] = static_cast<double> (cvUnion.value);
        }
    }   
    else
    {   
        for (uint32_t i = 0; i < nSamples; ++i)
        {
            cvUnion.value = static_cast<unsigned char> (data[i]);
            auto i1 = i*dataTypeSize;
            auto i2 = i1 + dataTypeSize;
            std::reverse_copy(data.data() + i1, data.data() + i2,
                              cvUnion.cArray);
            result[i] = static_cast<double> (cvUnion.value);
        }
    }
    return result;
}

}

namespace UFilterPicker::Utilities
{

export
std::vector<double> toDoubleVector(
    const UDataPacketServiceAPI::V1::Packet &packet,
    const bool swapBytes = (std::endian::native == std::endian::little ? false : true))
{
    auto nSamples = packet.number_of_samples();
    std::vector<double> result;
    if (nSamples == 0){return result;}
    namespace UDP = UDataPacketServiceAPI::V1;
    auto dataType = packet.data_type();
    if (dataType == UDP::DataType::DATA_TYPE_INTEGER_32)
    {
        result = ::unpackToDouble<int32_t> (packet.data(), nSamples, swapBytes);
    }
    else if (dataType == UDP::DataType::DATA_TYPE_INTEGER_64)
    {
        result = ::unpackToDouble<int64_t> (packet.data(), nSamples, swapBytes);
    }
    else if (dataType == UDP::DataType::DATA_TYPE_FLOAT)
    {
        result = ::unpackToDouble<float> (packet.data(), nSamples, swapBytes);
    }
    else if (dataType == UDP::DataType::DATA_TYPE_DOUBLE)
    {
        result = ::unpackToDouble<double> (packet.data(), nSamples, swapBytes);
    }
    else if (dataType == UDP::DataType::DATA_TYPE_TEXT)
    {
        throw std::invalid_argument("Cannot use text data");
    }
    else if (dataType == UDP::DataType::DATA_TYPE_UNKNOWN)
    {
        throw std::invalid_argument("Data type must be specified");
    }
    else
    {
        throw std::runtime_error("Unhandled data type");
    }
    return result;
    
}

export
[[nodiscard]] 
std::string toString(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
    auto name = identifier.network() 
              + "." + identifier.station()
              + "." + identifier.channel()
              + "." + identifier.location_code();
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    return name;
}

export
[[nodiscard]]
std::chrono::microseconds getStartTime(const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(packet.start_time());
    return std::chrono::microseconds {startTime};
}

export
[[nodiscard]]
std::chrono::microseconds getEndTime(const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto nSamples = packet.number_of_samples();
    if (nSamples < 1){throw std::invalid_argument("No samples");}
    const auto samplingRate = packet.sampling_rate();
    if (samplingRate <= 0){throw std::invalid_argument("Sampling rate not positive");}
    constexpr double SECONDS_TO_NANOSECONDS{1000000000};
    const double samplingPeriodNanoSeconds
        = SECONDS_TO_NANOSECONDS/samplingRate;
    const auto iEndTimeNanoSeconds
        = static_cast<int64_t> (
            std::round( (nSamples - 1)*samplingPeriodNanoSeconds ) );
    const auto endTimeMuS
        = google::protobuf::util::TimeUtil::NanosecondsToTimestamp(
             iEndTimeNanoSeconds);
    const std::chrono::microseconds endTime
        = getStartTime(packet)
        + std::chrono::microseconds {iEndTimeNanoSeconds};
    return endTime;
}

}
