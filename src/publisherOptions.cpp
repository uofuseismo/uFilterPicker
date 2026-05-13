#include <memory>
#include <utility>
#include <stdexcept>
#include "uFilterPicker/publisherOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"

using namespace UFilterPicker;

class PublisherOptions::PublisherOptionsImpl
{
public:
    GRPCClientOptions mGRPCOptions;
    int mMaximumQueueSize{2048};
    bool mHasGRPCOptions{false};
};

/// Constructor
PublisherOptions::PublisherOptions() :
    pImpl(std::make_unique<PublisherOptionsImpl> ())
{
}

/// Copy constructor
PublisherOptions::PublisherOptions(const PublisherOptions &options)
{
    *this = options;
}

/// Move constructor
PublisherOptions::PublisherOptions(PublisherOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
PublisherOptions& PublisherOptions::operator=(const PublisherOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<PublisherOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
PublisherOptions& 
PublisherOptions::operator=(PublisherOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
PublisherOptions::~PublisherOptions() = default;

/// gRPC client options
void PublisherOptions::setGRPCOptions(const GRPCClientOptions &options)
{
    pImpl->mGRPCOptions = options;
    pImpl->mHasGRPCOptions = true;
}

GRPCClientOptions PublisherOptions::getGRPCOptions() const
{
    if (!hasGRPCOptions())
    {   
        throw std::runtime_error("gRPC client options not set");
    }   
    return pImpl->mGRPCOptions;
}

bool PublisherOptions::hasGRPCOptions() const noexcept
{
    return pImpl->mHasGRPCOptions;
}

/// Output queue size
void PublisherOptions::setMaximumQueueSize(const int queueSize)
{
    if (queueSize < 1)
    {
        throw std::invalid_argument("Queue size must be positive");
    }
    pImpl->mMaximumQueueSize = queueSize;
}

int PublisherOptions::getMaximumQueueSize() const noexcept
{
    return pImpl->mMaximumQueueSize;
}


