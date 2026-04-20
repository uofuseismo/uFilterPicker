#include <bit>
#include <type_traits>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string>
#include <chrono>
#ifndef NDEBUG
#include <cassert>
#endif
#include <google/protobuf/util/time_util.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uFilterPickerProxyAPI/v1/pick.pb.h>
#include <uFilterPickerProxyAPI/v1/phase_hint.pb.h>
#include <uFilterPickerProxyAPI/v1/stream_identifier.pb.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
//#include <catch2/catch_approx.hpp>
//#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "uFilterPicker/utilities.hpp"

namespace
{

template<typename T>
std::string pack(const T *data, const int nSamples, const bool swapBytes)
{
    constexpr auto dataTypeSize = sizeof(T);
    std::string result;
    if (nSamples < 1){return result;}
    result.resize(dataTypeSize*nSamples);
    // Pack it up
    union CharacterValueUnion
    {   
        char cArray[dataTypeSize]; // Unpack uses unsigned char so this pushes it
        T value;
    };  
    CharacterValueUnion cvUnion;
    if (!swapBytes)
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                      result.data() + dataTypeSize*i);
        }
    }   
    else
    {   
        for (int i = 0; i < nSamples; ++i)
        {
            cvUnion.value = data[i];
            std::reverse_copy(cvUnion.cArray, cvUnion.cArray + dataTypeSize,
                              result.data() + dataTypeSize*i);
        }
    }   
    return result;
}

template<typename T>
std::string pack(const std::vector<T> &data)
{
    const bool swapBytes
    {
        std::endian::native == std::endian::little ? false : true
    };
    return ::pack(data.data(), static_cast<int> (data.size()), swapBytes);
}

template<typename T>
UDataPacketServiceAPI::V1::DataType toDataType()
{
    if (std::is_same<T, int>::value || std::is_same<T, int32_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_32;
    }   
    else if (std::is_same<T, int64_t>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_INTEGER_64;
    }   
    else if (std::is_same<T, float>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_FLOAT;
    }   
    else if (std::is_same<T, double>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_DOUBLE;
    }   
    else if (std::is_same<T, char>::value)
    {   
        return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_TEXT;
    }   
    return UDataPacketServiceAPI::V1::DataType::DATA_TYPE_UNKNOWN;
}

}

TEST_CASE("UFilterPicker::Utilities", "[toString]")
{
    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    // NOLINTBEGIN
    REQUIRE(UFilterPicker::Utilities::toString(identifier) == "UU.HVU.HHZ.01");
    // NOLINTEND

    SECTION("From Packet")
    {
        UDataPacketServiceAPI::V1::Packet packet;
        *packet.mutable_stream_identifier() = identifier;
        // NOLINTBEGIN
        REQUIRE(UFilterPicker::Utilities::toString(packet) == "UU.HVU.HHZ.01");
        // NOLINTEND
    }
}

TEST_CASE("UFilterPicker::Utilities", "[convertIdentifier]")
{
    const std::string networkIn{"uu"}; const std::string network{"UU"};
    const std::string stationIn{"hvu"}; const std::string station{"HVU"};
    const std::string channelIn{"hhz"}; const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    UDataPacketServiceAPI::V1::StreamIdentifier identifierIn;
    identifierIn.set_network(networkIn);
    identifierIn.set_station(stationIn);
    identifierIn.set_channel(channelIn);

    SECTION("With location code")
    {
        identifierIn.set_location_code(locationCode);

        auto identifier
            = UFilterPicker::Utilities::convertIdentifier(identifierIn); 
        REQUIRE(identifier.network() == network);
        REQUIRE(identifier.station() == station);
        REQUIRE(identifier.channel() == channel);
        REQUIRE(identifier.location_code() == locationCode);
    }

    SECTION("No location code")
    {
        auto identifier 
            = UFilterPicker::Utilities::convertIdentifier(identifierIn);
        REQUIRE(identifier.network() == network);
        REQUIRE(identifier.station() == station);
        REQUIRE(identifier.channel() == channel);
        REQUIRE(identifier.location_code() == "--");
    }
}

TEST_CASE("UFilterPicker::Utilities", "[toPPick]")
{
    constexpr std::chrono::microseconds pickTime{10};
    const std::string algorithm{"uFilterPicker12"};
    
    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    UFilterPickerProxyAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    auto pPick = UFilterPicker::Utilities::toPPick(pickTime, identifier, algorithm);
    const auto pickTimeBack
        = google::protobuf::util::TimeUtil::TimestampToMicroseconds(pPick.time());
    REQUIRE(pickTimeBack == pickTime.count());
    REQUIRE(pPick.phase_hint() == UFilterPickerProxyAPI::V1::PhaseHint::PHASE_HINT_P);
    REQUIRE(pPick.stream_identifier().network() == network);
    REQUIRE(pPick.stream_identifier().station() == station);
    REQUIRE(pPick.stream_identifier().channel() == channel);
    REQUIRE(pPick.stream_identifier().location_code() == locationCode);
    REQUIRE(pPick.algorithm() == algorithm);
 
}

TEMPLATE_TEST_CASE("UFilterPicker::Utilities", "[unpackData]",
                   int, float, double, int64_t)
{
    const std::vector<TestType> data{-1, -0, 1, 2, 3, 4, 5, 6, 7, 8};
    const std::chrono::seconds startTimeS{1774627730};
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};

    const double samplingRate{100};
    auto nSamples = static_cast<int> (data.size());

    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    UDataPacketServiceAPI::V1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    *packet.mutable_start_time() = startTime;
    packet.set_sampling_rate(samplingRate);
    packet.set_number_of_samples(nSamples);
    packet.set_data_type(::toDataType<TestType>());
    packet.set_data(::pack(data));
    
    // NOLINTBEGIN
    auto dVector = UFilterPicker::Utilities::toDoubleVector(packet);
    // NOLINTEND
    REQUIRE(dVector.size() == data.size());
    for (int i = 0; i < static_cast<int> (data.size()); ++i)
    {
        auto di = static_cast<double> (data[i]);
        constexpr double tolerance{std::numeric_limits<double>::epsilon()*100};
        REQUIRE_THAT(dVector[i], Catch::Matchers::WithinRel(di, tolerance));
    }
}

TEST_CASE("UFilterPicker::Utilities", "[startEndTime]")
{
    using namespace UFilterPicker::Utilities;

    const std::string network{"UU"};
    const std::string station{"HVU"};
    const std::string channel{"HHZ"};
    const std::string locationCode{"01"};
    
    UDataPacketServiceAPI::V1::StreamIdentifier identifier;
    identifier.set_network(network);
    identifier.set_station(station);
    identifier.set_channel(channel);
    identifier.set_location_code(locationCode);

    const double samplingRate{100};
    const std::vector<int> data{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const int nSamples{static_cast<int> (data.size())};
    constexpr std::chrono::seconds startTimeS{1774627730};
    const std::chrono::microseconds startTimeMuS{startTimeS};
    const std::chrono::nanoseconds startTimeNanoS{startTimeS}; 
    constexpr std::chrono::microseconds durationMuS{90000};
    constexpr std::chrono::nanoseconds durationNanoS{90000000};
    const std::chrono::microseconds endTimeMuSRef
        = startTimeMuS + durationMuS;
    const std::chrono::nanoseconds endTimeNanoSRef
        = startTimeNanoS + durationNanoS;
    const auto startTime
        = google::protobuf::util::TimeUtil::SecondsToTimestamp(
            startTimeS.count());
    UDataPacketServiceAPI::V1::Packet packet;
    *packet.mutable_stream_identifier() = identifier;
    *packet.mutable_start_time() = startTime;
    packet.set_sampling_rate(samplingRate);
    packet.set_number_of_samples(nSamples);
    packet.set_data_type(::toDataType<int>());
    packet.set_data(::pack(data));
    
    REQUIRE(getStartTime<std::chrono::microseconds> (packet) == startTimeMuS);
    REQUIRE(getStartTime<std::chrono::nanoseconds> (packet) == startTimeNanoS);

    auto endTimeMuS = getEndTime<std::chrono::microseconds> (packet);
    REQUIRE(endTimeMuS == endTimeMuSRef);

    auto endTimeNanoS = getEndTime<std::chrono::nanoseconds> (packet);
    REQUIRE(endTimeNanoS == endTimeNanoSRef);
}   


TEST_CASE("UFilterPicker::Utilities", "[gap]")
{
    using namespace UFilterPicker::Utilities;
    SECTION("Exact - no gaps")
    {
        constexpr double samplingRate{100}; // Pretty standard
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37010000}; // 37.01s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 0 );
    }
    SECTION("Exact - no gaps - rounding check")
    {
        constexpr double samplingRate{100}; // Pretty standard
        constexpr std::chrono::microseconds t0{36990000}; // 36.99s
        constexpr std::chrono::microseconds t1{37000000}; // 37.00s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 0 );
    }   
    SECTION("Slight undershot - no gaps")
    {   
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37009000}; // 37.009s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 0 );
    }   
    SECTION("Slight overrshot - no gaps")
    {
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37011000}; // 37.011s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 0 );
    }
    SECTION("Exact gap")
    {
        // Real samples are:
        //      37     37.01 37.02 37.03 37.04 37.05 37.06 37.08 37.09 37.1 37.11
        // (included)  (                   9 skipped                      ) (next)
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37100000}; // 37.1s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 9 ); 
        REQUIRE( getGapSizeInSamples(t0, samplingRate, t1) ==-9 );
    } 
    SECTION("Exact gap - rounding check")
    {   
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{36900000}; // 36.9
        constexpr std::chrono::microseconds t1{37000000}; // 37.0s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 9 );  
        REQUIRE( getGapSizeInSamples(t0, samplingRate, t1) ==-9 );
    }   
    SECTION("Slight undershot - gap")
    {
        // Real samples are:
        //      37     37.01 37.02 37.03 37.04 37.05 37.06 37.08 37.09 37.1 37.099
        // (included)  (                   9 skipped                      ) (next)
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37099000}; // 37.099s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 9 );  
        REQUIRE( getGapSizeInSamples(t0, samplingRate, t1) ==-9 );
    }
    SECTION("Slight overshot - gap")
    {   
        // Real samples are:
        //      37     37.01 37.02 37.03 37.04 37.05 37.06 37.08 37.09 37.1 37.104
        // (included)  (                   9 skipped                      ) (next)
        constexpr double samplingRate{100};
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37104000}; // 37.104s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 9 );
        REQUIRE( getGapSizeInSamples(t0, samplingRate, t1) ==-9 );
    }
    SECTION("Slight overshot with exact perturbation - gap")
    {
        // Real samples are:
        //      37     37.01 37.02 37.03 37.04 37.05 37.06 37.08 37.09 37.1 37.105 (rounds to 37.11)
        // (included)  (                   9 skipped                      ) (next)
        constexpr double samplingRate{100}; 
        constexpr std::chrono::microseconds t0{37000000}; // 37s
        constexpr std::chrono::microseconds t1{37105000}; // 37.105s
        REQUIRE( getGapSizeInSamples(t1, samplingRate, t0) == 9 );  
        REQUIRE( getGapSizeInSamples(t0, samplingRate, t1) ==-9 );
    }   
}

TEST_CASE("UFilterPicker::Utilities", "[leftTrim]")
{
    using namespace UFilterPicker::Utilities;
    SECTION("Too early")
    {
        std::vector<double> data(100); 
        std::iota(data.begin(), data.end(), 0);
        constexpr double samplingRate{100};
        const std::chrono::microseconds startTime{1776643200};
        const std::chrono::microseconds desiredStartTime
            = startTime - std::chrono::seconds {1}; 
        auto [trimmedData, newStartTime]
            = leftTrim(desiredStartTime, data, startTime, samplingRate);
        REQUIRE(newStartTime == startTime);
        REQUIRE(trimmedData.size() == data.size());
    }
    SECTION("Right on time - copy everything")
    {
        std::vector<double> data(100); 
        std::iota(data.begin(), data.end(), 0); 
        constexpr double samplingRate{100};
        const std::chrono::microseconds startTime{1776643200};
        const std::chrono::microseconds desiredStartTime{startTime};
        auto [trimmedData, newStartTime]
            = leftTrim(desiredStartTime, data, startTime, samplingRate);
        REQUIRE(newStartTime == startTime);
        REQUIRE(trimmedData.size() == data.size());
    }
    SECTION("Too late")
    {
        std::vector<double> data(100); 
        std::iota(data.begin(), data.end(), 0); 
        constexpr double samplingRate{100};
        const std::chrono::microseconds startTime{1776643200};
        const std::chrono::microseconds desiredStartTime
            = startTime + std::chrono::seconds{1};
        auto [trimmedData, newStartTime]
            = leftTrim(desiredStartTime, data, startTime, samplingRate);
        REQUIRE(newStartTime == desiredStartTime);
        REQUIRE(trimmedData.empty());
    }
    SECTION("Last sample")
    {
        std::vector<double> data(101); 
        std::iota(data.begin(), data.end(), 0);
        constexpr double samplingRate{100};
        const std::chrono::microseconds startTime{1776643200};
        const std::chrono::microseconds desiredStartTime
            = startTime + std::chrono::seconds{1};
        auto [trimmedData, newStartTime]
            = leftTrim(desiredStartTime, data, startTime, samplingRate);
        REQUIRE(newStartTime == desiredStartTime);
        REQUIRE(trimmedData.size() == 1);
    }
    SECTION("Half signal")
    {
        std::vector<double> data(101);
        std::iota(data.begin(), data.end(), 0);
        constexpr double samplingRate{100};
        const std::chrono::microseconds startTime{1776643200};
        const std::chrono::microseconds desiredStartTime
            = startTime + std::chrono::milliseconds{500}; // Half second
        auto [trimmedData, newStartTime]
            = leftTrim(desiredStartTime, data, startTime, samplingRate);
        REQUIRE(newStartTime == desiredStartTime);
        REQUIRE(trimmedData.size() == static_cast<size_t> (50 + 1));
        REQUIRE(std::abs(data.at(50) - trimmedData.front()) < 1.e-13);
        REQUIRE(std::abs(data.back() - trimmedData.back()) < 1.e-13);
    }
}
