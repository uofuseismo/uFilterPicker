#include <memory>
#include <stdexcept>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <uFilterPickerPickBrokerAPI/v1/pick.pb.h>
#include "uFilterPicker/publisher.hpp"
#include "uFilterPicker/publisherOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"

using namespace UFilterPicker;

class Publisher::PublisherImpl
{
public:
    PublisherImpl(const PublisherOptions &options,
                  std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasGRPCOptions())
        {
            throw std::invalid_argument("gRPC client options not set");
        }
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            const auto classId
                = std::to_string(reinterpret_cast<std::uintptr_t>(this));
            mLogger = spdlog::stdout_color_mt("SubscribeServiceConsole-"
                                            + classId);
            // NOLINTEND(misc-include-cleaner)
        }
    }
    PublisherOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
};

/// Constructor
Publisher::Publisher(const PublisherOptions &options,
                     std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<PublisherImpl> (options, std::move(logger)))
{
}

/// Enqueue it
void Publisher::enqueue(UFilterPickerPickBrokerAPI::V1::Pick &&pick)
{
    // Some quick checks before sending it
    if (!pick.has_time())
    {
        throw std::invalid_argument("Pick time not set"); 
    }
    if (!pick.has_stream_identifier())
    {
        throw std::invalid_argument("Stream identifier not set");
    }
    if (!pick.has_algorithm())
    {
        throw std::invalid_argument("Algorithm not set");
    } 
}

void Publisher::enqueue(const UFilterPickerPickBrokerAPI::V1::Pick &pick)
{
    UFilterPickerPickBrokerAPI::V1::Pick pickCopy{pick};
    enqueue(std::move(pickCopy));
}

/// Destructor
Publisher::~Publisher() = default;
