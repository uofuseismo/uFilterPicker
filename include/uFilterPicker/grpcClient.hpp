#ifndef UFILTER_PICKER_GRPC_CLIENT_HPP
#define UFILTER_PICKER_GRPC_CLIENT_HPP
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <spdlog/spdlog.h>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
}

namespace UFilterPicker
{
 class GRPCClientOptions;
}

namespace UFilterPicker
{
/// @class GRPCClient
/// @brief Defines the gRPC data packet client.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class GRPCClient
{
public:
    /// @brief Constructor
    GRPCClient(const GRPCClientOptions &options,
               const std::function<void (UDataPacketServiceAPI::V1::Packet &&paket)> &callback,
               std::shared_ptr<spdlog::logger> logger);

    /// @brief Destructor
    ~GRPCClient();
 
    GRPCClient(const GRPCClient &) = delete;
    GRPCClient(GRPCClient &&) noexcept = delete;
    GRPCClient& operator=(const GRPCClient &) = delete;
    GRPCClient& operator=(GRPCClient &&) noexcept = delete;
private:
    class GRPCClientImpl;
    std::unique_ptr<GRPCClientImpl> pImpl;
};
}
#endif
