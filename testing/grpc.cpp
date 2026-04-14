#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>
#include <vector>
#include <chrono>
//#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/catch_test_macros.hpp>
#include "uFilterPicker/grpcClientOptions.hpp"
#include "uFilterPicker/subscriberOptions.hpp"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

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
        auto reconnectSchedule = options.getReconnectSchedule();
        REQUIRE(schedule.size() == reconnectSchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == reconnectSchedule.at(i));
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
        REQUIRE_NOTHROW(options.setReconnectSchedule(schedule));

        REQUIRE(options.getHost() == host);
        REQUIRE(options.getPort() == port);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*options.getServerCertificate() == serverCertificate);
        REQUIRE(*options.getAccessToken() == token);
        REQUIRE(*options.getClientCertificate() == clientCertificate);
        REQUIRE(*options.getClientKey() == clientKey);
        //NOLINTEND(bugprone-unchecked-optional-access)
        auto reconnectSchedule = options.getReconnectSchedule();
        REQUIRE(schedule.size() == reconnectSchedule.size());
        for (size_t i = 0; i < schedule.size(); ++i)
        {
            REQUIRE(schedule.at(i) == reconnectSchedule.at(i));
        }
    }   
}

TEST_CASE("UFilterPicker", "[SubscriberOptions]")
{
    const std::string identifier{"grpc-client-12"};
    const std::string host{"some.host.org"};
    const uint16_t port{12345};

    UDataPacketServiceAPI::V1::StreamIdentifier id1;
    id1.set_network("UU");
    id1.set_station("CTU");
    id1.set_channel("HHZ");
    id1.set_location_code("01");

    UDataPacketServiceAPI::V1::StreamIdentifier id2;
    id2.set_network("UU");
    id2.set_station("ELU");
    id2.set_channel("EHZ");
    id2.set_location_code("01");

    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers
    {
        id1, id2
    };

    UFilterPicker::GRPCClientOptions grpcOptions;
    grpcOptions.setHost(host);
    grpcOptions.setPort(port);

    UFilterPicker::SubscriberOptions options;
    //NOLINTBEGIN(bugprone-unchecked-optional-access)
    REQUIRE(*options.getIdentifier() == "uFilterPicker");
    //NOLINTEND(bugprone-unchecked-optional-access)

    REQUIRE_NOTHROW(options.setGRPCOptions(grpcOptions));
    REQUIRE_NOTHROW(options.setStreamIdentifiers(streamIdentifiers));
    options.setIdentifier(identifier);

    REQUIRE(options.getGRPCOptions().getHost() == host);
    REQUIRE(options.getGRPCOptions().getPort() == port);
    //NOLINTBEGIN(bugprone-unchecked-optional-access)
    REQUIRE(*options.getIdentifier() == identifier);
    //NOLINTEND(bugprone-unchecked-optional-access)
    REQUIRE(options.getStreamIdentifiers().size() == streamIdentifiers.size());
    for (const auto &id : options.getStreamIdentifiers())
    {
        bool matched{false};
        for (const auto &jd : streamIdentifiers)
        {
            if (id.network() == jd.network() &&
                id.station() == jd.station() &&
                id.channel() == jd.channel() &&
                id.location_code() == jd.location_code())
            {
                matched = true;
            }
       }
       REQUIRE(matched);
    }

    streamIdentifiers.push_back(id1);
    REQUIRE_THROWS(options.setStreamIdentifiers(streamIdentifiers));
}
