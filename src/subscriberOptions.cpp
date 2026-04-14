#include <utility>
#include <optional>
#include <stdexcept>
#include <set>
#include <memory>
#include <vector>
#include <string>
#include "uFilterPicker/subscriberOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"
#include "uFilterPicker/utilities.hpp"
#include "uDataPacketServiceAPI/v1/stream_identifier.pb.h"

using namespace UFilterPicker;

class SubscriberOptions::SubscriberOptionsImpl
{
public:
    std::string mIdentifier{"uFilterPicker"};
    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> mStreamIdentifiers;
    GRPCClientOptions mGRPCOptions;
    bool mHasGRPCOptions{false};
};

/// Constructor
SubscriberOptions::SubscriberOptions() :
    pImpl(std::make_unique<SubscriberOptionsImpl> ())
{
}

/// Copy assignment
SubscriberOptions::SubscriberOptions(const SubscriberOptions &options)
{
    *this = options;
}

/// Move assignment
SubscriberOptions::SubscriberOptions(SubscriberOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
SubscriberOptions&
SubscriberOptions::operator=(const SubscriberOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<SubscriberOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
SubscriberOptions&
SubscriberOptions::operator=(SubscriberOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
SubscriberOptions::~SubscriberOptions() = default;

/// gRPC client options
void SubscriberOptions::setGRPCOptions(const GRPCClientOptions &options)
{
    pImpl->mGRPCOptions = options;
    pImpl->mHasGRPCOptions = true;
}

GRPCClientOptions SubscriberOptions::getGRPCOptions() const
{
    if (!hasGRPCOptions())
    {
        throw std::runtime_error("gRPC client options not set");
    }
    return pImpl->mGRPCOptions;
}

bool SubscriberOptions::hasGRPCOptions() const noexcept
{
    return pImpl->mHasGRPCOptions;
}

/// Stream identifiers
void SubscriberOptions::setStreamIdentifiers(
    const std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> &identifiers)
{
    if (identifiers.empty())
    {
        throw std::invalid_argument("No identiifers specified");
    }
    std::set<std::string> identifierNames;
    for (const auto &identifier : identifiers)
    {
        auto name = Utilities::toString(identifier);
        if (identifierNames.contains(name))
        {
            throw std::invalid_argument(name + " already specified");
        }
        identifierNames.insert(name);
    }
    pImpl->mStreamIdentifiers = identifiers;
}

std::vector<UDataPacketServiceAPI::V1::StreamIdentifier>
SubscriberOptions::getStreamIdentifiers() const
{
    if (!hasStreamIdentifiers())
    {
        throw std::invalid_argument("No stream identifiers set");
    }
    return pImpl->mStreamIdentifiers;
}

bool SubscriberOptions::hasStreamIdentifiers() const noexcept
{
    return !pImpl->mStreamIdentifiers.empty();
}

/// Subscription identifier
void SubscriberOptions::setIdentifier(const std::string &identifier)
{
    if (identifier.empty())
    {
        throw std::invalid_argument("Request identifier is empty");
    }
    pImpl->mIdentifier = identifier;
}

std::optional<std::string> SubscriberOptions::getIdentifier() const noexcept
{
    return !pImpl->mIdentifier.empty() ?
           std::make_optional<std::string> (pImpl->mIdentifier) : std::nullopt;
}
