#include <bit>
#include <type_traits>
#include <limits>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#ifndef NDEBUG
#include <cassert>
#endif
#include <google/protobuf/util/time_util.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
//#include <catch2/catch_approx.hpp>
//#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

import Utilities;

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

