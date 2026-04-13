#ifndef UFILTER_PICKER_SUBSCRIBER_HPP
#define UFILTER_PICKER_SUBSCRIBER_HPP
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
 class SubscriberOptions;
}

namespace UFilterPicker
{
/// @class Subscriber
/// @brief Defines the gRPC data packet client.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Subscriber
{
public:
    /// @brief Constructor
    Subscriber(const SubscriberOptions &options,
               const std::function<void (UDataPacketServiceAPI::V1::Packet &&paket)> &callback,
               std::shared_ptr<spdlog::logger> logger);

    /// @brief Destructor
    ~Subscriber();
 
    Subscriber(const Subscriber &) = delete;
    Subscriber(Subscriber &&) noexcept = delete;
    Subscriber& operator=(const Subscriber &) = delete;
    Subscriber& operator=(Subscriber &&) noexcept = delete;
private:
    class SubscriberImpl;
    std::unique_ptr<SubscriberImpl> pImpl;
};
}
#endif
