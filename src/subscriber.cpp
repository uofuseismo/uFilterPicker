#include <atomic>
#include <future>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <condition_variable>
#include <memory>
#include <exception>
#include <stdexcept>
#include <functional>
#include <utility>
#include <map>
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
#include <oneapi/tbb/concurrent_map.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/subscription_request.pb.h>
#include <uDataPacketServiceAPI/v1/broadcast.grpc.pb.h>
#include "uFilterPicker/subscriber.hpp"
#include "uFilterPicker/subscriberOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"
#include "uFilterPicker/utilities.hpp"

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
        grpc::string_ref, // serviceURL, 
        grpc::string_ref, // methodName,
        const grpc::AuthContext &,//channelAuthContext,
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
                  spdlog::logger *logger)
{
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
            SPDLOG_LOGGER_INFO(logger,
                               "Creating secure channel with API key to {}",
                               address);
            auto callCredentials = grpc::MetadataCredentialsFromPlugin(
                std::unique_ptr<grpc::MetadataCredentialsPlugin> (
                    new ::CustomAuthenticator(apiKey)));
            grpc::SslCredentialsOptions sslOptions;
            sslOptions.pem_root_certs = *serverCertificate;
            auto channelCredentials
                = grpc::CompositeChannelCredentials(
                      grpc::SslCredentials(sslOptions),
                      callCredentials);
            return grpc::CreateChannel(address, channelCredentials);
        }
        SPDLOG_LOGGER_INFO(logger,
                           "Creating secure channel without API key to {}",
                           address);
        grpc::SslCredentialsOptions sslOptions;
        sslOptions.pem_root_certs = *serverCertificate;
        return grpc::CreateChannel(address,
                                   grpc::SslCredentials(sslOptions));
     }
     SPDLOG_LOGGER_INFO(logger,
                        "Creating non-secure channel to {}",
                         address);
     return grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
}

class AsyncSubscriber :
    public grpc::ClientReadReactor<UDataPacketServiceAPI::V1::Packet>
{
public:
    AsyncSubscriber
    (
        UDataPacketServiceAPI::V1::Broadcast::Stub *stub,
        const UDataPacketServiceAPI::V1::SubscriptionRequest &request,
        std::function<void (UDataPacketServiceAPI::V1::Packet &&)> &addPacketCallback,
        std::shared_ptr<spdlog::logger> logger,
        std::atomic<bool> *keepRunning
    ) :
        mRequest(request),
        mAddPacketCallback(addPacketCallback),
        mLogger(std::move(logger)),
        mKeepRunning(keepRunning)
    {
        mClientContext.set_wait_for_ready(false); // Fail immediately if server isn't there
        stub->async()->Subscribe(&mClientContext, &mRequest, this);
        StartRead(&mPacket);
        StartCall();
    }

    void OnReadDone(bool ok) override
    {
        if (ok)
        {
            mHadSuccessfulRead = true;
            try
            {
                auto copy = mPacket;
                mAddPacketCallback(std::move(copy));
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_ERROR(mLogger,
                                    "Failed to add packet to callback because {}",
                                    std::string {e.what()});
            }
            if (!mKeepRunning->load())
            {
                if (!mTryCancel.load())
                {
                    SPDLOG_LOGGER_DEBUG(mLogger, "Cancelling");
                    mTryCancel.store(true);
                    mClientContext.TryCancel();
                }
            }
            else
            {
                StartRead(&mPacket);
            }
        }
        else
        {
            if (!mKeepRunning->load())
            {
                if (!mTryCancel.load())
                {
                    SPDLOG_LOGGER_DEBUG(mLogger, "Cancelling");
                    mTryCancel.store(true);
                    mClientContext.TryCancel();
                }
            }
        }
    }

    void OnDone(const grpc::Status &status) override
    {
        const std::unique_lock<std::mutex> lock(mMutex);
        mStatus = status;
        mDone = true;
        mConditionVariable.notify_one();
    }

    [[nodiscard]] std::pair<grpc::Status, bool> await()
    {
        while (!mDone)
        {
            if (!mKeepRunning->load())
            {
                if (!mTryCancel.load())
                {
                    SPDLOG_LOGGER_DEBUG(mLogger, "Cancelling");
                    mTryCancel.store(true);
                    mClientContext.TryCancel();
                }
            }
            constexpr std::chrono::milliseconds timeOut{250};
            std::unique_lock<std::mutex> lock(mMutex);
            //mConditionVariable.wait(lock, [this] {return mDone;});
            mConditionVariable.wait_for(lock, timeOut,
                                        [this]
                                        {
                                            return mDone;
                                        }); 
        }
        return std::pair{std::move(mStatus), mHadSuccessfulRead};
    }

    void kill()
    {
        mClientContext.TryCancel();
    }

#ifndef NDEBUG
    ~AsyncSubscriber()
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "In destructor");
    }
#endif

    AsyncSubscriber() = delete;
    AsyncSubscriber(const AsyncSubscriber &) = delete;
    AsyncSubscriber(AsyncSubscriber &&) noexcept = delete;
    AsyncSubscriber& operator=(const AsyncSubscriber &) = delete;
    AsyncSubscriber& operator=(AsyncSubscriber &&) noexcept = delete;
private:
    grpc::ClientContext mClientContext;
    UDataPacketServiceAPI::V1::SubscriptionRequest mRequest;
    std::function
    <
        void (UDataPacketServiceAPI::V1::Packet &&packet)
    > mAddPacketCallback;
    std::shared_ptr<spdlog::logger> mLogger;
    std::mutex mMutex;
    std::condition_variable mConditionVariable;
    UDataPacketServiceAPI::V1::Packet mPacket;
    grpc::Status mStatus{grpc::Status::OK};
    bool mDone{false};
    std::atomic<bool> *mKeepRunning{nullptr};
    std::atomic<bool> mTryCancel{false};
    bool mHadSuccessfulRead{false};
};

}

class Subscriber::SubscriberImpl
{
public:
    SubscriberImpl
    (
        const SubscriberOptions &options,
        const std::function<void (UDataPacketServiceAPI::V1::Packet &&)> &callback,
        std::shared_ptr<spdlog::logger> logger
    ) :
        mOptions(options),
        mAddPacketCallback(callback),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasGRPCOptions())
        {
            throw std::runtime_error("GRPC client options not set");
        }
        if (!mOptions.hasStreamIdentifiers())
        {
            throw std::invalid_argument(
                "No streams selected to which to subscribe");
        }
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            auto classId
                = std::to_string (reinterpret_cast<std::uintptr_t> (this));
            mLogger = spdlog::stdout_color_mt("subscriber-console-"
                                            + classId);
            // NOLINTEND(misc-include-cleaner)
        }
        mInitialized = true;
    }

    ~SubscriberImpl()
    {
        stop();
    }

    std::future<void> start()
    {
        mShutdownRequested = false;
        mKeepRunning.store(true);
        return std::async(&SubscriberImpl::acquirePackets, this);
    }

    void stop()                    
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "Stopping acquisition");
        mShutdownRequested = true;
        mShutdownCondition.notify_all();
        mKeepRunning.store(false);
    }           

    void acquirePackets()
    {
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif
        auto reconnectSchedule = mOptions.getGRPCOptions().getReconnectSchedule();
        auto nReconnect = static_cast<int> (reconnectSchedule.size());
        for (int kReconnect =-1; kReconnect < nReconnect; ++kReconnect)
        {
            if (!mKeepRunning.load()){break;}
            if (kReconnect >= 0)
            {
                SPDLOG_LOGGER_INFO(mLogger,
                                   "Will attempt to reconnect in {} s",
                                   reconnectSchedule.at(kReconnect).count());
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
            // Create request
            UDataPacketServiceAPI::V1::SubscriptionRequest request;
            auto subscriberIdentifier = mOptions.getIdentifier();
            if (subscriberIdentifier)
            {
                request.set_identifier(*subscriberIdentifier);
            }   
            for (const auto &selection : mOptions.getStreamIdentifiers())
            {
                *request.add_selections() = selection;
            }
            // Create channel
            auto channel
                = ::createChannel(mOptions.getGRPCOptions(), mLogger.get());
            auto stub = UDataPacketServiceAPI::V1::Broadcast::NewStub(channel);
            // Fire off the subscriber
            AsyncSubscriber subscriber{stub.get(),
                                       request,
                                       mOverrideAddPacketCallback,
                                       mLogger,
                                       &mKeepRunning};
            auto [status, hadSuccessfulRead] = subscriber.await();
            if (hadSuccessfulRead){kReconnect =-1;}
            if (status.ok())
            {   
                if (!mKeepRunning.load())
                {
                    SPDLOG_LOGGER_INFO(mLogger,
                                       "Subscriber RPC successfully finished");
                    break;
                }
                else
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                        "Subscriber RPC successfully finished but I should keep reading");
                }
            }
            else
            {
                int errorCode(status.error_code());
                std::string errorMessage(status.error_message());
                if (errorCode == grpc::StatusCode::UNAVAILABLE)
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Server unavailable (message: {})",
                                       errorMessage);
                }
                else if (errorCode == grpc::StatusCode::CANCELLED)
                {
                    if (!mKeepRunning.load()){break;}
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Server-side cancel (message: {})",
                                       errorMessage);
                }
                else
                {
                    SPDLOG_LOGGER_ERROR(mLogger,
                             "Subscribe RPC failed with error code {} (what: {})",
                             errorCode,  errorMessage);
                    break;
                }
            }
        } // Loop on retries
        if (mKeepRunning.load())
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Subscriber thread quitting!");
            throw std::runtime_error("Premature end of subscriber thread");
        }
        SPDLOG_LOGGER_INFO(mLogger, "Subscriber thread exiting");

    }
  
    void addPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    { 
        auto streamIdentifier = packet.stream_identifier();
        std::string name;
        try
        {
            name = Utilities::toString(streamIdentifier);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to get name because {}",
                                std::string {e.what()});
        }

        std::chrono::microseconds endTime{0};
        try
        {
            endTime
                = Utilities::getEndTime<std::chrono::microseconds> (packet);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to get packet end time for {} because {}",
                                name, std::string {e.what()});
        }

        try
        {
            mAddPacketCallback(std::move(packet));
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to add packet because {}",
                                std::string {e.what()});
            return;
        }

        auto idx = mLastPacketReceivedMap.find(name);
        if (idx != mLastPacketReceivedMap.end())
        {
            idx->second.second = std::max(endTime, idx->second.second);
        }
        else
        {
            auto streamTime = std::pair{std::move(streamIdentifier), endTime};
            auto added = mLastPacketReceivedMap.insert(
               std::pair {name, std::move(streamTime) }).second;
            if (!added)
            {
                SPDLOG_LOGGER_WARN(mLogger, "Failed to add {} to map", name);
            }
        }
    }
//private:
    SubscriberOptions mOptions;
    std::function<void (UDataPacketServiceAPI::V1::Packet &&)> mAddPacketCallback;
    std::function<void(UDataPacketServiceAPI::V1::Packet &&)>
        mOverrideAddPacketCallback
    {
        std::bind(&SubscriberImpl::addPacket, this,
                  std::placeholders::_1)
    };
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    oneapi::tbb::concurrent_map
    <   
        std::string,
        std::pair
        <
           UDataPacketServiceAPI::V1::StreamIdentifier,
           std::chrono::microseconds
        >
    > mLastPacketReceivedMap;
    mutable std::mutex mShutdownMutex;
    std::condition_variable mShutdownCondition;
    std::atomic<bool> mKeepRunning{true};
    bool mShutdownRequested{false};
    bool mInitialized{false};
};

/// Constructor
Subscriber::Subscriber(const SubscriberOptions &options,
                       const std::function<void (UDataPacketServiceAPI::V1::Packet &&)> &callback,
                       std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<SubscriberImpl> (options, callback, std::move(logger)))
{
}

/// Initialized
bool Subscriber::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Start
std::future<void> Subscriber::start()
{
    if (!isInitialized())
    {
        throw std::runtime_error("Subscriber not initialized");
    }
    return pImpl->start();
}

/// Stop
void Subscriber::stop()
{
    pImpl->stop();
}

/// Destructor
Subscriber::~Subscriber() = default;
