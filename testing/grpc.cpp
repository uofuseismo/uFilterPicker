#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
#include <vector>
#include <chrono>
//#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/catch_test_macros.hpp>
#include "uFilterPicker/grpcClientOptions.hpp"

TEST_CASE("UFilterPicker", "[grpcClientOptions]")
{
    SECTION("Defaults")
    {   
        const UFilterPicker::GRPCClientOptions options;
        REQUIRE(options.getHost() == "localhost");
        REQUIRE(options.getPort() == 50000);
        REQUIRE(options.getAccessToken() == std::nullopt);
        REQUIRE(options.getServerCertificate() == std::nullopt);
        REQUIRE(options.getClientCertificate() == std::nullopt);
        REQUIRE(options.getClientKey() == std::nullopt);
        const std::vector<std::chrono::milliseconds> schedule
        {
            std::chrono::seconds {0},
            std::chrono::seconds {5},
            std::chrono::seconds {15},
            std::chrono::seconds {30}
        };
        auto retrySchedule = options.getRetrySchedule();
        REQUIRE(schedule.size() == retrySchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == retrySchedule.at(i));
        }
    }   

    SECTION("Options")
    {   
        const std::string host{"some.host.org"};
        const std::string token{"super-secret-token"};
        const std::string serverCertificate{"some-wonky-hash"};
        const std::string clientCertificate{"some-other-hash"};
        const std::string clientKey{"some-private-hash"};
        const uint16_t port{12345};
        const std::vector<std::chrono::milliseconds> schedule
        {
            std::chrono::seconds {2},
            std::chrono::seconds {3},
            std::chrono::seconds {4}
        };

        UFilterPicker::GRPCClientOptions options;

        options.setHost(host);
        options.setPort(port);
        options.setServerCertificate(serverCertificate);
        options.setAccessToken(token);
        options.setClientCertificate(clientCertificate);
        options.setClientKey(clientKey);
        REQUIRE_NOTHROW(options.setRetrySchedule(schedule));

        REQUIRE(options.getHost() == host);
        REQUIRE(options.getPort() == port);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*options.getServerCertificate() == serverCertificate);
        REQUIRE(*options.getAccessToken() == token);
        REQUIRE(*options.getClientCertificate() == clientCertificate);
        REQUIRE(*options.getClientKey() == clientKey);
        //NOLINTEND(bugprone-unchecked-optional-access)
        auto retrySchedule = options.getRetrySchedule();
        REQUIRE(schedule.size() == retrySchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == retrySchedule.at(i));
        }
    }   
}

