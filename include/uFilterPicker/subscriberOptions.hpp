#ifndef UFILTER_PICKER_SUBSCRIBER_OPTIONS_HPP
#define UFILTER_PICKER_SUBSCRIBER_OPTIONS_HPP
#include <memory>
#include <vector>
#include <optional>
namespace UDataPacketServiceAPI::V1
{
 class StreamIdentifier;
}
namespace UFilterPicker
{
 class GRPCClientOptions;
}
namespace UFilterPicker
{
/// @class SubscriberOptions "subscriberOptions.hpp"
/// @brief Defines the options for the options for the GRPC packet 
///        subscriber client.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class SubscriberOptions
{
public:
    /// @brief Constructor.
    SubscriberOptions(); 
    /// @brief Copy constructor.
    SubscriberOptions(const SubscriberOptions &options);
    /// @brief Move constructor.
    SubscriberOptions(SubscriberOptions &&options) noexcept;

    /// @brief Sets the stream identifiers to for which we will receive data packets.
    void setStreamIdentifiers(const std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> &identifiers);
    /// @result True indicates the stream identifiers were set.
    [[nodiscard]] bool hasStreamIdentifiers() const noexcept;

    /// @brief Sets the client options.
    void setGRPCOptions(const GRPCClientOptions &options);
    /// @result The client options.
    [[nodiscard]] GRPCClientOptions getGRPCOptions() const;
    /// @result True indicates the client options were set.
    [[nodiscard]] bool hasGRPCOptions() const noexcept;

    /// @brief Sets an identifier for the subscription.
    void setIdentifier(const std::string &identifier);
    /// @brief Sets an identifier for the subscription service.
    [[nodiscard]] std::optional<std::string> getIdentifier() const noexcept;

    /// @brief Destructor.
    ~SubscriberOptions();
    /// @brief Copy assignment.
    SubscriberOptions& operator=(const SubscriberOptions &);
    /// @brief Move assignment.
    SubscriberOptions& operator=(SubscriberOptions &&) noexcept;
private:
    class SubscriberOptionsImpl;
    std::unique_ptr<SubscriberOptionsImpl> pImpl; 
};
}
#endif
