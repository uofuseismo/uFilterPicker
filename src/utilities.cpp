#include <cstddef>
#include <cstdint>
#include <cctype>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include <google/protobuf/util/time_util.h>
#include "uFilterPicker/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"

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

std::vector<double> UFilterPicker::Utilities::toDoubleVector(
    const UDataPacketServiceAPI::V1::Packet &packet,
    const bool swapBytes)
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

std::string 
UFilterPicker::Utilities::toString(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
    auto name = identifier.network() 
              + "." + identifier.station()
              + "." + identifier.channel()
              + "." + identifier.location_code();
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    return name;
}

std::string 
UFilterPicker::Utilities::toString(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    if (!packet.has_stream_identifier())
    {
        throw std::invalid_argument("Packet identifier not set");
    }
    return UFilterPicker::Utilities::toString(packet.stream_identifier());
}

template<>
std::chrono::microseconds 
UFilterPicker::Utilities::getStartTime(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(packet.start_time());
    return std::chrono::microseconds {startTime};
}

template<>
std::chrono::nanoseconds 
UFilterPicker::Utilities::getStartTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(packet.start_time());
    return std::chrono::nanoseconds {startTime};
}

template<>
std::chrono::microseconds 
UFilterPicker::Utilities::getEndTime<std::chrono::microseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
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
        = getStartTime<std::chrono::microseconds> (packet)
        + std::chrono::microseconds {iEndTimeNanoSeconds};
    return endTime;
}

template<>
std::chrono::nanoseconds 
UFilterPicker::Utilities::getEndTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
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
    const std::chrono::nanoseconds endTime
        = getStartTime<std::chrono::nanoseconds> (packet)
        + std::chrono::nanoseconds {iEndTimeNanoSeconds};
    return endTime;
}

bool UFilterPicker::Utilities::consistentSamplingRate(
    const double nominalSamplingRate,
    const double packetSamplingRate)
{
    if (std::abs(nominalSamplingRate - 100.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.01/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 200.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.005/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 250.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.004/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 500.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.002/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 1000.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.0001/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 40.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.025/2)
        {
            return false;
        }
        return true;
    }
    else
    {
        throw std::runtime_error("Unhandled nominal sampling rate "
                               + std::to_string(nominalSamplingRate));
    }
    return std::abs(nominalSamplingRate - packetSamplingRate) < 1.e-4;
}


