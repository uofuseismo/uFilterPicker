#include <utility>
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
    GRPCClientOptions mClientOptions;
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
