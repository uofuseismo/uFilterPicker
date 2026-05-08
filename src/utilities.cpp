#include <iostream>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <chrono>
#include <utility>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#ifndef NDEBUG
#include <cassert>
#endif
#include <google/protobuf/util/time_util.h>
#include "uFilterPicker/utilities.hpp"
#include "uDataPacketServiceAPI/v1/packet.pb.h"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"
#include "uDataPacketServiceAPI/v1/data_type.pb.h"
#include "uFilterPickerMessageStoreAPI/v1/pick.pb.h"
#include "uFilterPickerMessageStoreAPI/v1/phase_hint.pb.h"
#include "uFilterPickerMessageStoreAPI/v1/stream_identifier.pb.h"

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
std::chrono::microseconds UFilterPicker::Utilities::getNow()
{
    auto now 
       = std::chrono::duration_cast<std::chrono::microseconds>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}

template<>
std::chrono::nanoseconds UFilterPicker::Utilities::getNow()
{
    auto now  
       = std::chrono::duration_cast<std::chrono::nanoseconds>
         ((std::chrono::high_resolution_clock::now()).time_since_epoch());
    return now;
}

template<>
std::chrono::microseconds 
UFilterPicker::Utilities::getStartTime(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(
             packet.start_time());
    return std::chrono::microseconds {startTime};
}

template<>
std::chrono::nanoseconds 
UFilterPicker::Utilities::getStartTime<std::chrono::nanoseconds>(
    const UDataPacketServiceAPI::V1::Packet &packet)
{
    const auto startTime
        = google::protobuf::util::TimeUtil::TimestampToNanoseconds(
             packet.start_time());
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
    const std::chrono::microseconds endTime
        = getStartTime<std::chrono::microseconds> (packet)
        + std::chrono::duration_cast<std::chrono::microseconds>
            (std::chrono::nanoseconds {iEndTimeNanoSeconds});
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

int UFilterPicker::Utilities::getGapSizeInSamples(
    const std::chrono::microseconds &packetStartTimeIn,
    const double packetSamplingRateHz,
    const std::chrono::microseconds &latestSampleTimeIn )
{
    if (packetSamplingRateHz <= 0)
    {
        throw std::invalid_argument("Sampling rate not positive");
    }
    // Try to mitigate big numbers problems that come with epochal times
    auto stripOffset = std::min(std::chrono::duration_cast<std::chrono::seconds> (packetStartTimeIn),
                                std::chrono::duration_cast<std::chrono::seconds> (latestSampleTimeIn));
    const auto packetStartTime = packetStartTimeIn
                               - std::chrono::microseconds {stripOffset};
    const auto latestSampleTime = latestSampleTimeIn
                                - std::chrono::microseconds {stripOffset};
    const auto samplingPeriodMuS
        = static_cast<int64_t> (std::round(1000000./packetSamplingRateHz));
    const auto halfSamplingPeriodMuS
        = static_cast<int64_t> (std::round(500000./packetSamplingRateHz));
    int sign{1};
    // Most often this is the case within half a sample of expected start time
    const auto expectedStartTimeMuS
       = latestSampleTime.count() + samplingPeriodMuS;
    if (std::abs(expectedStartTimeMuS - packetStartTime.count()) 
        < halfSamplingPeriodMuS)
    {
        return 0;
    } 
    // Okay now have to think a bit
    auto t0 = latestSampleTime.count();
    auto t1 = packetStartTime.count();
    if (packetStartTime > latestSampleTime)
    {
        sign =+1;
    }
    else if (packetStartTime < latestSampleTime)
    {
        t0 = packetStartTime.count();
        t1 = latestSampleTime.count();
        sign =-1;
    }
    else 
    {
        return 0;
    }
    const int64_t deltaMuS = t1 - t0; 
#ifndef NDEBUG
    assert(deltaMuS >= 0);
#endif
    // Gap is the number of samples skipped 
    auto gapSamples
        = static_cast<int>
          (std::round(static_cast<double> (deltaMuS)
                     /static_cast<double> (samplingPeriodMuS))) - 1;
    gapSamples = std::max(0, gapSamples);
    // Goal is to get close and deal with numerical precision as it comes
    std::array<std::pair<int64_t, int>, 3> t1Estimates
    {
        std::pair {std::abs(t1 - (t0 + (gapSamples - 1)*samplingPeriodMuS)),
                   gapSamples - 1},
        std::pair {std::abs(t1 - (t0 + (gapSamples + 0)*samplingPeriodMuS)),
                   gapSamples + 0},
        std::pair {std::abs(t1 - (t0 + (gapSamples + 1)*samplingPeriodMuS)),
                   gapSamples + 1},
    };
    std::sort(t1Estimates.begin(), t1Estimates.end(), 
              [](const auto &lhs, const auto &rhs)
              {
                 return lhs.first < rhs.first;
              });
    if (t1Estimates.at(0).first <= halfSamplingPeriodMuS)
    {
        return sign*std::max(0, t1Estimates.at(0).second - 1);
    }
    // Do it the hard way
    std::cerr << " Hard way " << std::endl;
    auto t1Estimate = t0 + (gapSamples + 1)*samplingPeriodMuS;
    bool success{false};
    for (int k = 0; k < std::numeric_limits<int>::max(); ++k)
    {
        t1Estimate = t0 + k*samplingPeriodMuS;
        //std::cout << t1 << " " << t1Estimate << std::endl;
        if (std::abs(t1 - t1Estimate) <= halfSamplingPeriodMuS)
        {
            gapSamples = k;
            success = true;
            break;
        }
    }
    if (!success){throw std::runtime_error("Crude gap estimation failed");}
    return sign*gapSamples; 
}

UFilterPickerMessageStoreAPI::V1::Pick 
UFilterPicker::Utilities::toPPick(
    const std::chrono::microseconds &pickTime,
    const UFilterPickerMessageStoreAPI::V1::StreamIdentifier &identifierIn,
    const UFilterPickerMessageStoreAPI::V1::Algorithm &algorithm)
{
    UFilterPickerMessageStoreAPI::V1::Pick result;
    *result.mutable_stream_identifier() = identifierIn;
    *result.mutable_time()
        = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
             pickTime.count());
    result.set_phase_hint(
          UFilterPickerMessageStoreAPI::V1::PhaseHint::PHASE_HINT_P);
    *result.mutable_algorithm() = algorithm;
    return result; 
}

UFilterPickerMessageStoreAPI::V1::StreamIdentifier 
UFilterPicker::Utilities::convertIdentifier(
    const UDataPacketServiceAPI::V1::StreamIdentifier &identifierIn)
{
    /// Identifier
    UFilterPickerMessageStoreAPI::V1::StreamIdentifier identifier;
    auto network = identifierIn.network();
    if (network.empty()){throw std::invalid_argument("No network");}
    std::transform(network.begin(), network.end(), network.begin(), ::toupper);
    auto station = identifierIn.station();
    if (station.empty()){throw std::invalid_argument("No station");}
    std::transform(station.begin(), station.end(), station.begin(), ::toupper);
    auto channel = identifierIn.channel();
    if (channel.empty()){throw std::invalid_argument("No channel");}
    std::transform(channel.begin(), channel.end(), channel.begin(), ::toupper);
    
    identifier.set_network(std::move(network));
    identifier.set_station(std::move(station));
    identifier.set_channel(std::move(channel));
    if (identifierIn.has_location_code())
    {   
        auto locationCode = identifierIn.location_code();
        std::transform(locationCode.begin(), locationCode.end(),
                       locationCode.begin(), ::toupper);
        identifier.set_location_code(std::move(locationCode));
    }   
    else
    {
        identifier.set_location_code("--");
    }
    return identifier;
}

std::pair<std::vector<double>, std::chrono::microseconds> 
UFilterPicker::Utilities::leftTrim(
    const std::chrono::microseconds &desiredStartTime,
    const std::vector<double> &inputData,
    const std::chrono::microseconds &startTime,
    const double packetSamplingRateHz)
{
    // This is satisfied by default
    if (desiredStartTime <= startTime)
    {
        return std::pair {inputData, startTime};
    }
    // No way to fix this
    if (inputData.empty())
    {
        return std::pair {inputData, startTime};
    }
    // Figure out the offset
    const auto samplingPeriodMuS
        = static_cast<int64_t> (std::round(1000000./packetSamplingRateHz));
    const auto halfSamplingPeriodMuS
        = static_cast<int64_t> (std::round(500000./packetSamplingRateHz));
    auto nSamples = static_cast<int> (inputData.size());
    auto endTime
        = startTime
        + std::chrono::microseconds {(nSamples - 1)*samplingPeriodMuS};
    // Edge case at end (I want the last sample)
    if (std::abs(endTime.count() - desiredStartTime.count())
        < halfSamplingPeriodMuS)
    {
        const std::vector<double> outputData{inputData.back()};
        return std::pair {outputData, desiredStartTime};
    }
    // My desired start time exceeds the end time - I get nothing
    if (desiredStartTime > endTime)
    {
        const std::vector<double> outputData;
        return std::pair {outputData, desiredStartTime};
    }
    // Okay, this is normal - I'm some offset into the signal
    auto deltaMuS = desiredStartTime.count() - startTime.count(); 
#ifndef NDEBUG
    assert(deltaMuS >= 0);
#endif
    auto offsetSamples = static_cast<int>
          (std::round(static_cast<double> (deltaMuS)
                     /static_cast<double> (samplingPeriodMuS)));
    // Only first sample
    if (offsetSamples == 0 || deltaMuS < samplingPeriodMuS)
    {
        const std::vector<double> outputData{inputData.back()};
        return std::pair {outputData, desiredStartTime};
    }   
    // Again, we have a packet that is out of range
    if (offsetSamples >= nSamples)
    {
        const std::vector<double> outputData;
        return std::pair {outputData, desiredStartTime};
    }
    // Goal is to find the next sample that is greater than or equal to the
    // desired time
    constexpr int64_t t0{0};// - startTime.count()};
    const auto t1 = deltaMuS; //desiredStartTime.count();// - startTime.count();
    const std::array<std::pair<int64_t, int>, 4> t1Estimates
    {
        // Offset is inclusive - so output samples is + 1
        std::pair {t0 + (offsetSamples + 1 - 2)*samplingPeriodMuS,
                   offsetSamples + 1 - 2 + 1},
        std::pair {t0 + (offsetSamples + 1 - 1)*samplingPeriodMuS,
                   offsetSamples + 1 - 1 + 1},
        std::pair {t0 + (offsetSamples + 1 + 0)*samplingPeriodMuS,
                   offsetSamples + 1 + 0 + 1},
        std::pair {t0 + (offsetSamples + 1 + 1)*samplingPeriodMuS,
                   offsetSamples + 1 + 1 + 1}, 
    };
/*
    std::sort(t1Estimates.begin(), t1Estimates.end(), 
              [](const auto &lhs, const auto &rhs)
              {
                 return lhs.first < rhs.first;
              });  
*/
    for (const auto &estimate : t1Estimates)
    {
        if (estimate.first >= t1)
        {
            auto nSamplesOut = std::min(nSamples, estimate.second);
            std::vector<double> outputData;
            outputData.reserve(nSamplesOut);
            auto offsetRealized = nSamplesOut - 1;
            for (int i = offsetRealized; i < static_cast<int> (nSamples); ++i)
            {
                outputData.push_back(inputData[i]);
            }
            auto outputTime = startTime
                            + std::chrono::microseconds
                              {
                                  (nSamplesOut - 1)*samplingPeriodMuS
                              }; 
            return {outputData, outputTime};  
        }
    }
    throw std::runtime_error("Algorithmic failure");

}

