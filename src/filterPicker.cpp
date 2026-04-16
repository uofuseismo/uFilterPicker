//#include <signal.h>
#include <cstdlib>
#include <memory>
#include <cstddef>
#include <csignal>
#include <iostream>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <utility>
#include <limits>
#include <functional>
#include <stdexcept>
#include <exception>
#include <sstream>
#include <string>
#include <chrono>
#include <atomic>
#include <future>
#include <thread>
#include <mutex>
#include <filesystem>
#include <map>
#ifndef NDEBUG
#include <cassert>
#endif
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>
#include <oneapi/tbb/concurrent_queue.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/metrics/provider.h>
//#include <absl/log/initialize.h>
/*
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
*/
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> 
#include <uDataPacketServiceAPI/v1/packet.pb.h>
//#include <uDataPacketImport/grpc/client.hpp>
//#include <uDataPacketImport/grpc/clientOptions.hpp>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
//#include <readerwriterqueue.h>
#include "uFilterPicker/utilities.hpp"
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/subscriber.hpp"
#include "uFilterPicker/subscriberOptions.hpp"
/*
#include "uFilterPicker/pipeline.hpp"
#include "uFilterPicker/characteristicFunction.hpp"
#include "uFilterPicker/envelope.hpp"
#include "uFilterPicker/narrowBandFilter.hpp"
*/
#include "uFilterPicker/thresholdTrigger.hpp"

#define APPLICATION_NAME "uFilterPickerDetector"

import Logger;
import Metrics;
//import Utilities;
import FilterPickerOptions;

namespace
{

volatile std::sig_atomic_t mSignalStatus;
std::atomic_bool mInterrupted{false};

}

namespace
{

struct DetectorPicker
{
    std::unique_ptr<UFilterPicker::Detector> detector{nullptr};
    std::unique_ptr<UFilterPicker::ThresholdTrigger> trigger{nullptr};
    double samplingRate{100};
};

class Detector
{
public:
    Detector(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
             std::shared_ptr<spdlog::logger> logger) :
        mIdentifier(identifier), 
        mIdentifierString(UFilterPicker::Utilities::toString(identifier)),
        mLogger(std::move(logger))
    {
        mDetector = UFilterPicker::Detector::create100HzBroadband();
        mTrigger = UFilterPicker::ThresholdTrigger::create100HzBroadband();
        mFilterGroupDelay = mDetector->getGroupDelay();
        mInitialized = mDetector->isInitialized();
        SPDLOG_LOGGER_INFO(mLogger, "Made detector for {}", mIdentifierString);
    }
    [[nodiscard]] std::string getIdentifierReference() const noexcept
    {
        return mIdentifierString;
    }
    [[nodiscard]] std::optional<UDataPacketServiceAPI::V1::Packet>
        apply(const UDataPacketServiceAPI::V1::Packet &packet)
    {
        return this->operator()(packet);
    }
    [[nodiscard]] std::optional<UDataPacketServiceAPI::V1::Packet>
        operator()(const UDataPacketServiceAPI::V1::Packet &packet)
    {
        // Apply to the packet
        if (UFilterPicker::Utilities::toString(packet) != mIdentifierString)
        {
            SPDLOG_LOGGER_ERROR(mLogger, 
                               "Inconsistent identifier - {} does not match {}",
                               UFilterPicker::Utilities::toString(packet),
                               mIdentifierString);
            return std::nullopt;
        }
        auto samplingRate = packet.sampling_rate();
        try
        {
            if (!UFilterPicker::Utilities::consistentSamplingRate(
                mNominalSamplingRate, samplingRate))
            {
                throw std::invalid_argument("Inconsistent sampling rates for "
                                          + mIdentifierString);
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Could not check sampling rate because {}",
                                   std::string {e.what()});
            throw std::runtime_error("Sample rate check failed");
        }
        auto nSamples = packet.number_of_samples();
        if (nSamples < 1)
        {
            throw std::invalid_argument("No data for " + mIdentifierString);
        }
        auto packetStartTime = UFilterPicker::Utilities::getStartTime<std::chrono::microseconds> (packet);
        auto packetEndTime = UFilterPicker::Utilities::getEndTime<std::chrono::microseconds> (packet);
        auto data = UFilterPicker::Utilities::toDoubleVector(packet);
        if (mFirstPacket)
        {
            auto filteredData = mDetector->apply(data);
            mFirstSampleTime = packetStartTime; 
            mLastSampleTime = packetEndTime;
            mFirstPacket = false;
        }
        else
        {
            if (packetStartTime < mLastSampleTime)
            {
                SPDLOG_LOGGER_WARN(mLogger, 
                   "Out of order packet detected for {} < {} {} {}; skipping",
                    packetStartTime.count()*1.e-6,
                    mLastSampleTime.count()*1.e-6,
                    UFilterPicker::Utilities::toString(packet),
                    mIdentifierString);
                return std::nullopt;
            }
            if (packetStartTime - mLastSampleTime > mGapTolerance)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Gap detected for {}",
                                   mIdentifierString);
                mDetector->resetInitialConditions(); 
                mTrigger->resetInitialConditions();
                mFirstSampleTime = packetStartTime;
            }
        }
        // TODO need richer definition of detection packet
        auto characteristicFunction = mDetector->apply(data);
        //auto triggerSignal = mTrigger->apply(characteristicFunction);
        mLastSampleTime = packetEndTime;
        UDataPacketServiceAPI::V1::Packet result;
        *result.mutable_stream_identifier() = mIdentifier;
        auto shiftedPacketStartTime = packetStartTime - mFilterGroupDelay;
        auto shiftedStartTimeProtobuf 
            = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
                 shiftedPacketStartTime.count());
        *result.mutable_start_time() = shiftedStartTimeProtobuf;
        result.set_sampling_rate(samplingRate);
        result.set_data_type(UDataPacketServiceAPI::V1::DataType::DATA_TYPE_DOUBLE);
        //*result.mutable_data() = characteristicFunction;
        auto endTime
            = UFilterPicker::Utilities::getEndTime<std::chrono::microseconds>
              (result);
        if (endTime - mFirstSampleTime > mBurnInTime &&
            endTime - mFirstSampleTime > mFilterGroupDelay)
        {
            auto picks = mTrigger->apply(characteristicFunction,
                                         shiftedPacketStartTime,
                                         samplingRate);
            return std::optional<UDataPacketServiceAPI::V1::Packet> (result);
        }
        return std::nullopt;
    }

    Detector(const Detector &) = delete;
    Detector(Detector &&) noexcept = delete;
    Detector& operator=(const Detector &) = delete;
    Detector& operator=(Detector &&) noexcept = delete;

    UDataPacketServiceAPI::V1::StreamIdentifier mIdentifier;
    std::string mIdentifierString;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    double mNominalSamplingRate{100}; 
    std::unique_ptr<UFilterPicker::Detector> mDetector{nullptr};
    std::unique_ptr<UFilterPicker::ThresholdTrigger> mTrigger{nullptr};
    std::chrono::microseconds mLastSampleTime{-1};
    std::chrono::microseconds mFirstSampleTime{-1};
    std::chrono::microseconds mGapTolerance{30000}; // Start with 3 samples
    std::chrono::microseconds mFilterGroupDelay{std::chrono::seconds {2}};
    std::chrono::microseconds mBurnInTime{std::chrono::seconds {10}};
    bool mInitialized{false};
    bool mFirstPacket{true};
};

class NetworkDetector
{
public:
    NetworkDetector(const UFilterPicker::Options::ProgramOptions &options,
                    std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (mLogger == nullptr)
        {
            //NOLINTNEXTLINE(misc-include-cleaner)
            mLogger = spdlog::stdout_color_st("console");
        }

        // Create a subscriber
        mPacketSubscriber
            = std::make_unique<UFilterPicker::Subscriber> 
              (options.packetSubscriberOptions, mImportCallback, mLogger);

        // Create some detectors
        for (const auto &streamIdentifier :
             mOptions.packetSubscriberOptions.getStreamIdentifiers())
        {
            auto streamName = UFilterPicker::Utilities::toString(streamIdentifier);
            if (streamIdentifier.channel() != "HHZ")
            {
                throw std::invalid_argument("Only HHZ detectors implemented");
            }
            // TODO
            auto detector = UFilterPicker::Detector::create100HzBroadband(); 
            auto thresholdPicker = UFilterPicker::ThresholdTrigger::create100HzBroadband();
            auto detectorPicker = std::make_unique<::DetectorPicker> (std::move(detector), std::move(thresholdPicker));
            auto added
                = mDetectorPickers.insert(
                      std::pair {streamName,
                                 std::move(detectorPicker)} ).second;
            if (!added)
            {
                throw std::runtime_error("Failed to add detector "
                                       + streamName);
            }
        }

        // Setup metrics
        if (mOptions.exportMetrics)
        {

        }
/*
        mStreamsToProcess = options.streamIdentifiers;
        clientOptions.setStreamSelections(options.streamIdentifiers);
        mImportClient
            = std::make_unique<UDataPacketImport::GRPC::Client>
              (mImportCallback, clientOptions); 
*/
    }

    void publishCharacteristicFunctions()
    {
    }

    void filterPackets()
    {
        constexpr std::chrono::milliseconds timeOut{10};
int np{0};
        while (mKeepRunning)
        {
            UDataPacketServiceAPI::V1::Packet packet;
            if (mImportQueue.try_pop(packet))
            {
np++;
if (np > 1000){
 spdlog::warn("Terminating");
 std::raise(SIGINT);
}
                try
                {
                    auto streamName = UFilterPicker::Utilities::toString(packet);
                    auto idx = mDetectorPickers.find(streamName);
                    if (idx == mDetectorPickers.end())
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                                      "Received packet from unwanted stream {}",
                                      streamName);
                        continue;
                    }
                    if (!packet.has_sampling_rate())
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                            "Stream {} does not have sampling rate - skipping",
                            streamName);
                        continue;
                    }
                    auto samplingRate = packet.sampling_rate();
                    auto packetStartTime
                        = UFilterPicker::Utilities::getStartTime
                          <std::chrono::microseconds> (packet);
                    auto timeSeries
                        = UFilterPicker::Utilities::toDoubleVector(packet);
                    if (timeSeries.empty())
                    {
                        SPDLOG_LOGGER_WARN(mLogger,
                                           "Empty packet detected for {}",
                                           streamName);
                        continue;
                    }
                    auto characteristicFunction
                        = idx->second->detector->apply(timeSeries);
                    auto onOffSignal = idx->second->trigger->apply(
                            characteristicFunction,
                            packetStartTime,
                            samplingRate);
/*
                    auto characteristicFunctionPacket
                        = idx->second->apply(newPacket); 
                    if (characteristicFunctionPacket)
                    {
                    } 
*/
                 }
                 catch (const std::exception &e)
                 {
                    SPDLOG_LOGGER_ERROR(mLogger,
                                        "Failed to process packet because {} ",
                                        std::string {e.what()});
                 }
            }
            else
            {
                std::this_thread::sleep_for(timeOut);
            }
        }
    }
 
    /// Callback for packet subscriber
    void getPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        if (mImportQueue.size() >= mMaximumImportQueueSize)
        {
            SPDLOG_LOGGER_WARN(mLogger, "Queue full - popping packets");
            while (mImportQueue.size() >= mMaximumImportQueueSize)
            {
                UDataPacketServiceAPI::V1::Packet work;
                if (!mImportQueue.try_pop(work))
                {
                    SPDLOG_LOGGER_WARN(mLogger, "Failed to pop front of queue");
                    break;
                }
            }
        }
        if (!mImportQueue.try_push(std::move(packet)))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to enqueue packet");
        }
    }

    /// Starts the application
    void start()
    {
#ifndef NDEBUG
        assert(mPacketSubscriber != nullptr);
#endif
        mKeepRunning.store(true);
        mPacketSubscriptionFuture = mPacketSubscriber->start();
        mDataProcessingFuture = std::async(&NetworkDetector::filterPackets, this);
        handleMainThread();
    }

    /// Stops the application
    void stop()
    {
        mKeepRunning.store(false);
        if (mPacketSubscriber)
        {
            mPacketSubscriber->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds {15});
        if (mDataProcessingFuture.valid())
        {
            mDataProcessingFuture.get();
        }
        if (mPacketSubscriptionFuture.valid())
        {
            mPacketSubscriptionFuture.get();
        }
    }

    /// Keeps the main thread occupied
    void handleMainThread()
    {
        SPDLOG_LOGGER_DEBUG(mLogger, "Main thread entering waiting loop");
        stdCatchSignals();
        while (!mStopRequested)
        {
            if (mInterrupted)
            {
                 SPDLOG_LOGGER_INFO(mLogger,
                                   "SIGINT/SIGTERM signal received!");
                mStopRequested = true;
                break;
            }
            constexpr std::chrono::milliseconds waitForFuture {5};
            if (!checkFuturesOkay(waitForFuture))
                {
                SPDLOG_LOGGER_CRITICAL(mLogger,
                   "Futures exception caught; terminating app");
                mStopRequested = true;
                break;
            }
            printSummary();
            std::unique_lock<std::mutex> lock(mStopMutex);
            constexpr std::chrono::milliseconds pause{100};
            mStopCondition.wait_for(lock, pause,
                                    [this]
                                    {
                                        return mStopRequested;
                                    });
        }
        if (mStopRequested)
        {
            SPDLOG_LOGGER_DEBUG(mLogger,
                                "Stop request received.  Terminating...");
            stop();
            std::this_thread::sleep_for(std::chrono::milliseconds {15});
        }
    }

    /// @brief Prints an update
    void printSummary()
    {

    }

    /// @brief Checks the futures
    /// @result True indicates the all the processes are running a-okay.
    [[nodiscard]]
    bool checkFuturesOkay(const std::chrono::milliseconds &timeOut)
    {
        bool isOkay{true};
        try
        {
            auto status = mPacketSubscriptionFuture.wait_for(timeOut);
            if (status == std::future_status::ready)
            {
                mPacketSubscriptionFuture.get();
            }
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Fatal error in acquisition: {}",
                                   std::string {e.what()});
            isOkay = false;
        }

        try
        {   
            auto status = mDataProcessingFuture.wait_for(timeOut);
            if (status == std::future_status::ready)
            {
                mDataProcessingFuture.get();
            }
        }
        catch (const std::exception &e) 
        {
            SPDLOG_LOGGER_CRITICAL(mLogger,
                                   "Fatal error in processing: {}",
                                   std::string {e.what()});
            isOkay = false;
        }

        return isOkay;
    }

    void stdCatchSignals()
    {
        std::signal(SIGINT,  NetworkDetector::stdSignalHandler);
        std::signal(SIGTERM, NetworkDetector::stdSignalHandler);
    }

    static void stdSignalHandler(const int signal)
    {
        mSignalStatus = signal;
        mInterrupted = true;
    }

//private:
    UFilterPicker::Options::ProgramOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::unique_ptr<UFilterPicker::Subscriber> mPacketSubscriber{nullptr};
    std::function<void(UDataPacketServiceAPI::V1::Packet &&)>
        mImportCallback
    {   
        std::bind(&::NetworkDetector::getPacket, this,
                  std::placeholders::_1)
    };
    std::map
    <
        std::string,
        std::unique_ptr<::DetectorPicker>
    > mDetectorPickers;
    oneapi::tbb::concurrent_bounded_queue<UDataPacketServiceAPI::V1::Packet> mImportQueue;
    std::future<void> mPacketSubscriptionFuture;
    std::future<void> mDataProcessingFuture;
    std::atomic<bool> mKeepRunning{true};
    mutable std::mutex mStopMutex;
    std::condition_variable mStopCondition;
    bool mStopRequested{false};
    size_t mMaximumImportQueueSize{512};
};

}

///---------------------------------------------------------------------------///

int main(int argc, char *argv[])
{
    // Initialize the metrics singleton regardless of if we export
    //NOLINTNEXTLINE(misc-include-cleaner)
    UFilterPicker::Metrics::initializeSingleton();

    // Get the ini file from the command line
    std::string iniFile;
    try
    {
        //NOLINTBEGIN(misc-include-cleaner)
        auto [iniFileName, isHelp] 
            = UFilterPicker::Options::parseCommandLineOptions(argc, argv);
        //NOLINTEND(misc-include-cleaner)
        if (isHelp){return EXIT_SUCCESS;}
        if (iniFileName.empty())
        {
            throw std::runtime_error("No initialization file specified");
        }
        iniFile = iniFileName;
    }
    catch (const std::exception &e)
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read command line arguments because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Read the program properties
    UFilterPicker::Options::ProgramOptions programOptions;
    try
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        programOptions = UFilterPicker::Options::parseIniFile(iniFile);
    }
    catch (const std::exception &e) 
    {   
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read program options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    std::shared_ptr<spdlog::logger> logger{nullptr};
    try
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        logger = UFilterPicker::Logger::initialize(programOptions);
    }
    catch (const std::exception &e)
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        auto consoleLogger = spdlog::stdout_color_st("console");
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to initialize logger because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    try 
    {
        //NOLINTNEXTLINE(misc-include-cleaner)
        UFilterPicker::Metrics::initialize(programOptions);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to initialize metrics because {}",
                               std::string {e.what()});
        //NOLINTNEXTLINE(misc-include-cleaner)
        UFilterPicker::Logger::cleanup();
        return EXIT_FAILURE;
    }

    std::unique_ptr<::NetworkDetector> networkDetector;
    try
    {
        SPDLOG_LOGGER_INFO(logger, "Initializing network detector");
        networkDetector
            = std::make_unique<::NetworkDetector> (programOptions, logger);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to create network detector because {}",
                               std::string {e.what()});
        //NOLINTBEGIN(misc-include-cleaner)
        UFilterPicker::Metrics::cleanup();
        UFilterPicker::Logger::cleanup();
        //NOLINTEND(misc-include-cleaner)
        return EXIT_FAILURE;
    }

    try
    {
        SPDLOG_LOGGER_INFO(logger, "Starting detector");
        networkDetector->start(); 
        //NOLINTBEGIN(misc-include-cleaner)
        UFilterPicker::Metrics::cleanup();
        UFilterPicker::Logger::cleanup();
        //NOLINTEND(misc-include-cleaner)
    }
    catch (const std::exception &e) 
    {   
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to create network detector because {}",
                               std::string {e.what()});
        //NOLINTBEGIN(misc-include-cleaner)
        UFilterPicker::Metrics::cleanup();
        UFilterPicker::Logger::cleanup();
        //NOLINTEND(misc-include-cleaner)
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

