#include <iostream>
#include <cstdint>
#include <algorithm>
#include <utility>
#include <limits>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
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
#include <uDataPacketServiceAPI/v1/packet.pb.h>
//#include <uDataPacketImport/grpc/client.hpp>
//#include <uDataPacketImport/grpc/clientOptions.hpp>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/data_type.pb.h>
//#include <readerwriterqueue.h>
#include "uFilterPicker/utilities.hpp"
#include "uFilterPicker/detector.hpp"
/*
#include "uFilterPicker/pipeline.hpp"
#include "uFilterPicker/characteristicFunction.hpp"
#include "uFilterPicker/envelope.hpp"
#include "uFilterPicker/narrowBandFilter.hpp"
*/
#include "uFilterPicker/thresholdTrigger.hpp"

#define APPLICATION_NAME "uFilterPickerDetector"

import Logger;
//import Utilities;
import FilterPickerOptions;

namespace
{

struct PacketImport
{
    std::string host;
    uint16_t port;
    std::string clientCertificate;
    std::string clientToken;
};

struct ProgramOptions
{
    PacketImport importOptions;
    std::string applicationName{APPLICATION_NAME}; 
    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers;
    int verbosity{3};
};

[[nodiscard]] std::string toString(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
    auto name = identifier.network() 
              + "." + identifier.station()
              + "." + identifier.channel()
              + "." + identifier.location_code();
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    return name;
}

[[nodiscard]] std::string toString(const UDataPacketServiceAPI::V1::Packet &packet)
{
    return ::toString(packet.stream_identifier());
}

[[nodiscard]] std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[]);
[[nodiscard]] ::ProgramOptions parseIniFile(const std::filesystem::path &iniFile);

}

namespace
{

/*
std::unique_ptr<UFilterPicker::Pipeline>
    createPipeline(const int butterworthOrder,
                   const std::pair<double, double> &passband,
                   const int envelopeLength,
                   const int characteristicFunctionLength,
                   const double samplingRate)
{
    auto narrowBandFilter
        = std::make_unique<UFilterPicker::NarrowBandFilter>
          (butterworthOrder, passband, samplingRate);
    UFilterPicker::EnvelopeOptions envelopeOptions{envelopeLength, 8.0};
    auto envelope
        = std::make_unique<UFilterPicker::Envelope> (envelopeOptions); //Length);
    auto characteristicFunction
        = std::make_unique<UFilterPicker::CharacteristicFunction>
          (characteristicFunctionLength);
    auto pipeline
        = std::make_unique<UFilterPicker::Pipeline> (std::move(narrowBandFilter),
                                                     std::move(envelope),
                                                     std::move(characteristicFunction)); 
    return pipeline;
} 

std::unique_ptr<UFilterPicker::Detector>
    createDetector(const int butterworthOrder,
                   const std::vector< std::pair<double, double> > &passbands,
                   const int envelopeLength,
                   const int characteristicFunctionLength,
                   const double samplingRate)
{
    std::vector<std::unique_ptr<UFilterPicker::Pipeline>> pipelines;
    for (const auto &passband : passbands)
    {   
        auto pipeline = ::createPipeline(butterworthOrder,
                                         passband,
                                         envelopeLength,
                                         characteristicFunctionLength,
                                         samplingRate);
        pipelines.push_back(std::move(pipeline));
    }   
    auto detector
        = std::make_unique<UFilterPicker::Detector> (std::move(pipelines));
    return detector;
}

std::unique_ptr<UFilterPicker::Detector>
    createBroadbandDetector100Hz()
{
    constexpr int butterworthOrder{5};
    constexpr int envelopeLength{400};
    constexpr int characteristicFunctionLength{400};
    constexpr double samplingRate{100}; 
    auto detector = ::createDetector(butterworthOrder, //5,
                                     std::vector<std::pair<double, double>> 
                                     {
                                        { 3 - 1,  8 + 3},
                                        { 8 - 3, 13 + 3},
                                        {13 - 3, 18 + 3},
                                        {18 - 3, 23 + 3},
                                        {23 - 3, 28 + 3},
                                        {28 - 3, 33 + 3}
                                     },
                                     envelopeLength, //400,
                                     characteristicFunctionLength, //400,
                                     samplingRate);
     return detector;
}

std::unique_ptr<UFilterPicker::ThresholdTrigger>
    createBroadbandThresholdTrigger100Hz(
        const std::pair<double, double> &onAndOffThreshold = std::pair<double, double> {6, 5})
{
    return std::make_unique<UFilterPicker::ThresholdTrigger>
           (onAndOffThreshold);
}

[[nodiscard]]
    bool consistentSamplingRate(const double nominalSamplingRate,
                                const double packetSamplingRate)
{
    if (std::abs(nominalSamplingRate - 100.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.01/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 200.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.005/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 250.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.004/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 500.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.002/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 1000.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.0001/2)
        {
            return false;
        }
        return true;
    }
    else if (std::abs(nominalSamplingRate - 40.0) < 1.e-5)
    {
        if (std::abs(nominalSamplingRate - packetSamplingRate) > 0.025/2)
        {
            return false;
        }
        return true;
    }
    else
    {
        spdlog::warn("Unhandled nominal sampling rate "
                   + std::to_string(nominalSamplingRate));
    }
    return std::abs(nominalSamplingRate - packetSamplingRate) < 1.e-4;
}
*/

class Detector
{
public:
    Detector(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
             std::shared_ptr<spdlog::logger> logger) :
        mIdentifier(identifier), 
        mIdentifierString(::toString(identifier)),
        mLogger(logger)
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
        if (::toString(packet) != mIdentifierString)
        {
            SPDLOG_LOGGER_ERROR(mLogger, 
                               "Inconsistent identifier - {} does not match {}",
                               ::toString(packet), mIdentifierString);
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
                    ::toString(packet),
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
    //NetworkDetector(const UFilterPicker::ProgramOptions &options)
NetworkDetector()
    {
/*
        UDataPacketImport::GRPC::ClientOptions clientOptions;
        clientOptions.setAddress(options.importOptions.host + ":"
                               + std::to_string(options.importOptions.port));
        if (!options.importOptions.clientCertificate.empty())
        {
            clientOptions.setCertificate(
                options.importOptions.clientCertificate);
            if (!options.importOptions.clientToken.empty())
            {
                clientOptions.setToken(options.importOptions.clientToken);
            }
        }
std::cout << options.streamIdentifiers.size() << std::endl;
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
/*
        constexpr std::chrono::milliseconds timeOut{10};
int np{0};
        while (mKeepRunning)
        {
            UDataPacketServiceAPI::V1::Packet newPacket;
            if (mImportQueue.try_dequeue(newPacket))
            {
np++;
if (np > 1000){
 spdlog::warn("Terminating");
 mKeepRunning = false;
}
                try
                {
                    auto streamIdentifier
                        = newPacket.getStreamIdentifier();
                    auto streamIdentifierString = streamIdentifier.toString();
                    if (!mStreamsToProcess.contains(streamIdentifier))
                    {
                         spdlog::warn("Received unwanted stream "
                                    + streamIdentifierString);
                         continue;
                    }
                    
                    if (!mDetectors.contains(streamIdentifierString))
                    {
                        auto detector
                            = std::make_unique<::Detector> (streamIdentifier);
                        mDetectors.insert( std::pair{streamIdentifierString,
                                                     std::move(detector) } );
                    } 
                    auto idx = mDetectors.find(streamIdentifierString);
#ifndef NDEBUG
                    assert(idx != mDetectors.end());
#else
                    if (idx == mDetectors.end())
                    {
                        throw std::runtime_error("Algorithmic error");
                    }
#endif
                    auto characteristicFunctionPacket
                        = idx->second->apply(newPacket); 
                    if (characteristicFunctionPacket)
                    {
                    } 
                 }
                 catch (const std::exception &e)
                 {
                    spdlog::error("Failed to process packet " + std::string {e.what()});
                 }
            }
            else
            {
                std::this_thread::sleep_for(timeOut);
            }
        }
*/
    }
    void getPacket(UDataPacketServiceAPI::V1::Packet &&packet)
    {
        //spdlog::debug("got packet");
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
    void start()
    {
        stop();

        mKeepRunning = true;
//        mImportFuture = mImportClient->start();
        mDataProcessingFuture = std::async(&NetworkDetector::filterPackets, this);
    }
    void stop()
    {
        mKeepRunning = false;
        //mImportClient->stop();
    }
//private:
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::function<void(UDataPacketServiceAPI::V1::Packet &&)>
        mImportCallback
    {   
        std::bind(&::NetworkDetector::getPacket, this,
                  std::placeholders::_1)
    };
//    std::unique_ptr<UDataPacketImport::GRPC::Client> mImportClient{nullptr};
    std::map
    <
        std::string,
        std::unique_ptr<::Detector>
    > mDetectors;
    oneapi::tbb::concurrent_bounded_queue<UDataPacketServiceAPI::V1::Packet> mImportQueue;
    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> mStreamsToProcess;
    std::future<void> mImportFuture;
    std::future<void> mDataProcessingFuture;
    std::atomic<bool> mKeepRunning{true};
    size_t mMaximumImportQueueSize{512};
};

}

int main(int argc, char *argv[])
{
    // Get the ini file from the command line
    std::string iniFile;
    try
    {
        auto [iniFileName, isHelp] = UFilterPicker::Options::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        if (iniFileName.empty())
        {
            throw std::runtime_error("No initialization file specified");
        }
        iniFile = iniFileName;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Read the program properties
    UFilterPicker::Options::ProgramOptions programOptions;
    try
    {
        programOptions = UFilterPicker::Options::parseIniFile(iniFile);
    }
    catch (const std::exception &e) 
    {   
        spdlog::error(e.what());
        return EXIT_FAILURE;
    }

/*
    std::unique_ptr<::NetworkDetector> networkDetector;
    try
    {
//        networkDetector = std::make_unique<::NetworkDetector> (programOptions);
    }
    catch (const std::exception &e)
    {
        spdlog::error(e.what());
        return EXIT_FAILURE;
    }

//  networkDetector->start();
    std::this_thread::sleep_for(std::chrono::seconds {120});
//    networkDetector->stop();
*/
    return EXIT_SUCCESS;
}

///--------------------------------------------------------------------------///
///                            Utility Functions                             ///
///--------------------------------------------------------------------------///
namespace
{

/*
/// Read the program options from the command line
std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[])
{
    std::string iniFile;
    boost::program_options::options_description desc(R"""(
The uFilterPickerDetector applies the Lomax Filter Picker to waveforms 
collected at UUSS.  The result is a detection signal.

    uFilterPickerDetector --ini=detector.ini

Allowed options)""");
    desc.add_options()
        ("help", "Produces this help message")
        ("ini",  boost::program_options::value<std::string> (),
                 "The initialization file for this executable");
    boost::program_options::variables_map vm;
    boost::program_options::store(
        boost::program_options::parse_command_line(argc, argv, desc), vm);
    boost::program_options::notify(vm);
    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return {iniFile, true};
    }
    if (vm.count("ini"))
    {
        iniFile = vm["ini"].as<std::string>();
        if (!std::filesystem::exists(iniFile))
        {
            throw std::runtime_error("Initialization file: " + iniFile
                                   + " does not exist");
        }
    }
    return {iniFile, false};
}

[[nodiscard]] std::string
loadStringFromFile(const std::filesystem::path &path)
{
    std::string result;
    if (!std::filesystem::exists(path)){return result;}
    std::ifstream file(path);
    if (!file.is_open())
    {   
        throw std::runtime_error("Failed to open " + path.string());
    }   
    std::stringstream sstr;
    sstr << file.rdbuf();
    file.close(); 
    result = sstr.str();
    return result;
}
*/

/*
::ProgramOptions parseIniFile(const std::filesystem::path &iniFile)
{
    ::ProgramOptions options;
    if (!std::filesystem::exists(iniFile)){return options;}
    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    // Application name
    options.applicationName
        = propertyTree.get<std::string> ("General.applicationName",
                                         options.applicationName);
    if (options.applicationName.empty())
    {   
        options.applicationName = APPLICATION_NAME;
    }   
    options.verbosity
        = propertyTree.get<int> ("General.verbosity", options.verbosity);

    
    std::string section{"GRPCClient"};
    ::PacketImport importOptions;
    importOptions.host
        = propertyTree.get<std::string> (section + ".host");
    importOptions.port
        = propertyTree.get<uint16_t> (section + ".port");
    auto certificate
        = propertyTree.get_optional<std::string>
          (section + ".clientCertificate");
    if (certificate)
    {
        std::filesystem::path certificatePath{*certificate};
        if (std::filesystem::exists(certificatePath))
        {   
            importOptions.clientCertificate
                = ::loadStringFromFile(certificatePath);
        }
    }
    auto token
       = propertyTree.get_optional<std::string>
         (section + ".clientToken"); 
    if (token)
    {
        if (importOptions.clientCertificate.empty())
        {
            throw std::invalid_argument(
                "clientCertificate required to use token in section "
              + section);
        }
        if (token->empty())
        {
            throw std::invalid_argument("Token is empty");
        }
        importOptions.clientToken = *token;
    }
    options.importOptions = importOptions;

    // Get the streams to process
    std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streams;
    std::set<std::string> addedStreams;
    for (int i = 1; i <= std::numeric_limits<int16_t>::max(); ++i)
    {
        const std::string key{"Streams.stream_" + std::to_string(i)};
        auto streamName
           = propertyTree.get_optional<std::string> (key);
        if (streamName)
        {
            std::string network;
            std::string station;
            std::string channel;
            std::string locationCode{"--"};
            std::vector<std::string> splitStreamName;
            boost::split(splitStreamName, *streamName,
                         boost::is_any_of("._ \n\t"));
            if (splitStreamName.size() == 3 || splitStreamName.size() == 4)
            {
                network = boost::trim_copy(splitStreamName.at(0));
                station = boost::trim_copy(splitStreamName.at(1));
                channel = boost::trim_copy(splitStreamName.at(2));
                if (splitStreamName.size() == 4)
                {
                    locationCode = boost::trim_copy(splitStreamName.at(3));
                }
            }
            else
            {
                throw std::invalid_argument(
                    "Invalid stream - expecting NETWORK.STATION.CHANNEL.LOCATION; e.g., UU.CWU.HHZ.01 or PB.B205.EHZ.-- but got "
                  + *streamName);
            }
            UDataPacketServiceAPI::V1::StreamIdentifier
                identifier;
            std::transform(network.begin(), network.end(), network.begin(), ::toupper);
            std::transform(station.begin(), station.end(), station.begin(), ::toupper);
            std::transform(channel.begin(), channel.end(), channel.begin(), ::toupper);
            std::transform(locationCode.begin(), locationCode.end(), locationCode.begin(), ::toupper);
            identifier.set_network(network);
            identifier.set_station(station);
            identifier.set_channel(channel);
            identifier.set_location_code(locationCode);
            auto identifierString = UFilterPicker::Utilities::toString(identifier);
            if (addedStreams.contains(identifierString))
            {
                spdlog::warn(identifierString + " already exists; skipping");
            }
            else
            {
                spdlog::debug("Will subscribe to " + identifierString);
                streams.push_back(std::move(identifier));
                addedStreams.insert(identifierString);
            }
        }
        else
        {
            break;
        }
    }
    if (streams.empty())
    {
        throw std::invalid_argument("No streams specified");
    }
    options.streamIdentifiers = std::move(streams);

    return options;
}
*/
}

