#include <cstdint>
#include <cstddef>
#include <string>
//#include <optional>
//#include <vector>
#include <chrono>
//#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/catch_test_macros.hpp>
#include "uFilterPicker/pickerOptions.hpp"
//#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

TEST_CASE("UFilterPicker", "[PickerOptions]")
{
    SECTION("Defaults")
    {  
        constexpr int gapSize{5};
        constexpr std::chrono::microseconds maxLatency{std::chrono::minutes{15}};
        constexpr std::chrono::microseconds maxFutureTime{0};
        constexpr int burnInFactor{3};
        const UFilterPicker::PickerOptions options;
        REQUIRE(options.getGapTolerance() == gapSize);
        REQUIRE(options.getMaximumLatency() == maxLatency);
        REQUIRE(options.getMaximumFutureTime() == maxFutureTime); 
        REQUIRE(options.getBurnInFactor() == burnInFactor);
    }

    SECTION("Options")
    {
        constexpr int gapSize{61};
        constexpr std::chrono::microseconds maxLatency{std::chrono::minutes{3}};
        constexpr std::chrono::microseconds maxFutureTime{12};
        constexpr int burnInFactor{4};

        UFilterPicker::PickerOptions options;
        options.setGapTolerance(gapSize);
        options.setMaximumLatency(maxLatency);
        options.setMaximumFutureTime(maxFutureTime);
        options.setBurnInFactor(burnInFactor);

        REQUIRE(options.getGapTolerance() == gapSize);
        REQUIRE(options.getMaximumLatency() == maxLatency);
        REQUIRE(options.getMaximumFutureTime() == maxFutureTime); 
        REQUIRE(options.getBurnInFactor() == burnInFactor);

        const UFilterPicker::PickerOptions copy{options};
        REQUIRE(copy.getGapTolerance() == gapSize);
        REQUIRE(copy.getMaximumLatency() == maxLatency);
        REQUIRE(copy.getMaximumFutureTime() == maxFutureTime); 
        REQUIRE(copy.getBurnInFactor() == burnInFactor);
    }
}
