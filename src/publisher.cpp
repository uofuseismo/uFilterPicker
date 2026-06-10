#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
//NOLINTNEXTLINE(misc-include-cleaner)
#include <spdlog/sinks/stdout_color_sinks.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/support/client_callback.h>
#include <grpcpp/support/config.h>
#include <grpcpp/support/status.h>
#include <grpcpp/support/string_ref.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/security/auth_context.h>
#include <tbb/concurrent_queue.h>
#include <uFilterPickerPickBrokerAPI/v1/publish_service.grpc.pb.h>
#include <uFilterPickerPickBrokerAPI/v1/pick.pb.h>
#include <uFilterPickerPickBrokerAPI/v1/publish_response.pb.h>
#include "uFilterPicker/publisher.hpp"
#include "uFilterPicker/publisherOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"
#include "uFilterPicker/metrics.hpp"

using namespace UFilterPicker;

namespace
{

class CustomAuthenticator : public grpc::MetadataCredentialsPlugin
{    
public:    
    CustomAuthenticator(const grpc::string &token) :
        mToken(token)
    {   
    }   
    grpc::Status GetMetadata(
        grpc::string_ref , //serviceURL, 
        grpc::string_ref , //methodName,
        const grpc::AuthContext &, //channelAuthContext,
        std::multimap<grpc::string, grpc::string> *metadata) override
    {   
        metadata->insert(std::make_pair("x-custom-auth-token", mToken));
        return grpc::Status::OK;
    }   
//private:
    grpc::string mToken;
};

std::shared_ptr<grpc::Channel>
    createChannel(const UFilterPicker::GRPCClientOptions &options,
                  spdlog::logger *logger,
                  const bool isReconnect)
{
#ifndef NDEBUG
    assert(logger);
#endif
    auto address = UFilterPicker::makeAddress(options);
    auto serverCertificate = options.getServerCertificate();
    if (serverCertificate)
    {
#ifndef NDEBUG
        assert(!serverCertificate->empty());
#endif
        if (options.getAccessToken())
        {
            auto apiKey = *options.getAccessToken();
#ifndef NDEBUG
            assert(!apiKey.empty());
#endif
            if (!isReconnect)
            {
                SPDLOG_LOGGER_INFO(logger,
                                   "Creating secure channel with API key to {}",
                                   address);
            }
            else
            {
                SPDLOG_LOGGER_INFO(logger,
                                 "Recreating secure channel with API key to {}",
                                 address);
            }
            auto callCredentials = grpc::MetadataCredentialsFromPlugin(
                std::unique_ptr<grpc::MetadataCredentialsPlugin> (
                    new CustomAuthenticator(apiKey)));
            grpc::SslCredentialsOptions sslOptions;
            sslOptions.pem_root_certs = *serverCertificate;
            auto channelCredentials
                = grpc::CompositeChannelCredentials(
                      grpc::SslCredentials(sslOptions),
                      callCredentials);
            return grpc::CreateChannel(address, channelCredentials);
        }
        if (!isReconnect)
        {
            SPDLOG_LOGGER_INFO(logger,
                               "Creating secure channel without API key to {}",
                               address);
        }
        else
        {
            SPDLOG_LOGGER_INFO(
                logger,
                "Recreating secure channel without API key to {}",
                address);
        }
        grpc::SslCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = *serverCertificate;
        return grpc::CreateChannel(address,
                                   grpc::SslCredentials(sslOptions));
     }
     if (!isReconnect)
     {
         SPDLOG_LOGGER_INFO(logger,
                            "Creating non-secure channel to {}",
                             address);
     }
     else
     {
         SPDLOG_LOGGER_INFO(logger,
                            "Recreating non-secure channel to {}",
                             address);
     }
     return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

std::pair<grpc::Status, bool> 
    publishSynchronously(
        const UFilterPicker::GRPCClientOptions &grpcOptions,
        const std::chrono::milliseconds &timeOut,
        const bool isReconnect,
        //NOLINTBEGIN(misc-include-cleaner)
        tbb::concurrent_bounded_queue
        <
            UFilterPickerPickBrokerAPI::V1::Pick
        > *exportQueue,
        //NOLINTEND(misc-include-cleaner)
        std::atomic<bool> *keepRunning,
        std::shared_ptr<spdlog::logger> &logger)
{
#ifndef NDEBUG
    assert(keepRunning);
    assert(logger != nullptr);
    assert(exportQueue != nullptr);
#endif
    auto channel = ::createChannel(grpcOptions, logger.get(), isReconnect);
    auto stub = UFilterPickerPickBrokerAPI::V1::PublishService::NewStub(channel);
    grpc::ClientContext context;
    context.set_wait_for_ready(false);
    UFilterPicker::Metrics::MetricsSingleton &mMetrics
    {   
        UFilterPicker::Metrics::MetricsSingleton::getInstance()
    };
    bool hadSuccessfulWrite{false};
    UFilterPickerPickBrokerAPI::V1::PublishResponse publishResponse;
    std::unique_ptr
    <
        grpc::ClientWriter<UFilterPickerPickBrokerAPI::V1::Pick>
    > writer(stub->Publish(&context, &publishResponse));
#ifndef NDEBUG
    assert(writer != nullptr);
#endif
    constexpr int checkStateEvery{15};
    int checkStateCounter{0};
    while (keepRunning->load())
    {
        UFilterPickerPickBrokerAPI::V1::Pick pick;
        if (exportQueue->try_pop(pick))
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "Sending pick");
            if (!writer->Write(pick))
            {
                SPDLOG_LOGGER_WARN(logger, "Broken stream");
                break;
            }
            checkStateCounter = 0;
            hadSuccessfulWrite = true;
            mMetrics.incrementPicksSentCounter();
        }
        else
        {
            ++checkStateCounter;
            if (checkStateCounter >= checkStateEvery)
            {
                auto state = channel->GetState(false);
                if (state != grpc_connectivity_state::GRPC_CHANNEL_READY)
                {
                    if (state == grpc_connectivity_state::GRPC_CHANNEL_SHUTDOWN)
                    {
                        SPDLOG_LOGGER_WARN(logger, "Channel was shutdown");
                    }
                    else if (state ==
                             grpc_connectivity_state::GRPC_CHANNEL_IDLE)
                    {
                        SPDLOG_LOGGER_WARN(logger, "Channel is idle");
                    }
                    else
                    {
                        SPDLOG_LOGGER_WARN(logger, "Channel is bad state");
                    }
                    break;
                }
                checkStateCounter = 0;
            }
            std::this_thread::sleep_for(timeOut);
        }
    }
    // Termination condition met - send a write done message
    if (!keepRunning->load())
    {
        SPDLOG_LOGGER_DEBUG(logger, "Sending WritesDone message");
        writer->WritesDone();
    }
    auto status = writer->Finish();
    return std::pair {status, hadSuccessfulWrite};
}

}

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
        mMaximumQueueSize
            = static_cast<int> (mOptions.getMaximumQueueSize());
        mExportQueue.set_capacity(mMaximumQueueSize);
    }

    ~PublisherImpl()
    {
        stop();
    }

    [[nodiscard]] std::future<void> start()
    {
        //stop();
        //mShutdownRequested = true;
        mKeepRunning.store(true);
        auto result = std::async(&PublisherImpl::publish, this);
        return result;
    }

    void enqueue(UFilterPickerPickBrokerAPI::V1::Pick &&pick)
    {
        int nPicksPurged{0};
        while (static_cast<int> (mExportQueue.size()) >=
               std::max(0, mMaximumQueueSize - 1))
        {
            UFilterPickerPickBrokerAPI::V1::Pick workspace;
            if (mExportQueue.empty()){break;}
            if (!mExportQueue.try_pop(workspace))
            {
                SPDLOG_LOGGER_WARN(mLogger, "Failed to pop pick");
                break;
            } 
            nPicksPurged = nPicksPurged + 1; 
        }
        if (!mExportQueue.try_push(std::move(pick)))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to enqueue next pick");
        }
        if (nPicksPurged > 0)
        {
            SPDLOG_LOGGER_WARN(mLogger, "Purged {} picks from export queue",
                               nPicksPurged);
        }
    }

    void publish()
    {
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif
        SPDLOG_LOGGER_INFO(mLogger, "Starting publisher");
        constexpr std::chrono::milliseconds timeOut{10};
        const auto grpcOptions = mOptions.getGRPCOptions();
        auto reconnectSchedule = grpcOptions.getReconnectSchedule();
        for (int kReconnect =-1;
             kReconnect < static_cast<int> (reconnectSchedule.size());
             ++kReconnect)
        {
            bool isReconnect{false};
            if (!mKeepRunning.load()){break;}
            if (kReconnect >= 0)
            {
                SPDLOG_LOGGER_INFO(mLogger,
                    "Will reconnect in {} milliseconds",
                    std::to_string(reconnectSchedule.at(kReconnect).count()));
                isReconnect = true;
                std::unique_lock<std::mutex> lock(mShutdownMutex);
                mShutdownCondition.wait_for(lock,
                                            reconnectSchedule.at(kReconnect),
                                            [this]
                                            {
                                                return mShutdownRequested;
                                            });
                lock.unlock();
                if (!mKeepRunning.load()){break;}
            }
            auto [status, hadSuccessfulWrite]
                = ::publishSynchronously(grpcOptions,
                                         timeOut,
                                         isReconnect,
                                         &mExportQueue,
                                         &mKeepRunning,
                                         mLogger);
            if (hadSuccessfulWrite){kReconnect =-1;}
            // Handle the return codes
            if (status.ok())
            {
                if (!mKeepRunning.load())
                {
                    SPDLOG_LOGGER_INFO(mLogger, "RPC successfully finished");
                    break;
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "RPC successfully finished but I should keep writing");
                }
            }
            else
            {
                const int errorCode(status.error_code());
                std::string errorMessage(status.error_message());
                if (errorCode == grpc::StatusCode::UNAVAILABLE)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Server unavailable (message: {})",
                                       errorMessage);
                }
                else if (errorCode == grpc::StatusCode::CANCELLED)
                {
                    if (mKeepRunning.load())
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                                           "Server-side cancel (message: {})",
                                           errorMessage);
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    SPDLOG_LOGGER_ERROR(mLogger,
                             "Publish RPC failed with error code {} (what: {})",
                             errorCode,  errorMessage);
                    break;
                }
            }
        } // Loop on reconnects
        if (mKeepRunning.load())
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Publisher thread quitting!");
            throw std::runtime_error("Premature end of publisher thread");
        }
        SPDLOG_LOGGER_INFO(mLogger, "Publisher thread exiting");
    }

    void stop()
    {
        mKeepRunning.store(false);
        mShutdownRequested = true; 
        mShutdownCondition.notify_all();
    }

    PublisherOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    tbb::concurrent_bounded_queue<UFilterPickerPickBrokerAPI::V1::Pick> mExportQueue;
    std::condition_variable mShutdownCondition;
    std::mutex mShutdownMutex;
    std::atomic<bool> mKeepRunning{true};
    int mMaximumQueueSize{0};
    bool mShutdownRequested{false};
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
    pImpl->enqueue(std::move(pick));
}

void Publisher::enqueue(const UFilterPickerPickBrokerAPI::V1::Pick &pick)
{
    UFilterPickerPickBrokerAPI::V1::Pick pickCopy{pick};
    enqueue(std::move(pickCopy));
}

/// Destructor
Publisher::~Publisher() = default;

/// Start
std::future<void> Publisher::start()
{
    return pImpl->start();
}

/// Stop
void Publisher::stop()
{
    pImpl->stop();
}
