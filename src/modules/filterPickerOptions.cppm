module;
#include <cctype>
#include <cstdint>
#include <algorithm>
#include <limits>
#include <utility>
#include <optional>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <set>
#include <spdlog/spdlog.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include "uFilterPicker/subscriberOptions.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"
#include "uFilterPicker/utilities.hpp"

#define APPLICATION_NAME "uFilterPickerDetector"

export module FilterPickerOptions;
import OTelOptions;

namespace
{

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

[[nodiscard]] UFilterPicker::GRPCClientOptions
    getGRPCClientOptions(
    const boost::property_tree::ptree &propertyTree,
    const std::string &section)
{
    UFilterPicker::GRPCClientOptions options;

    auto host
        = propertyTree.get<std::string> (section + ".host",
                                         options.getHost());
    if (host.empty())
    {   
        throw std::runtime_error(section + ".host is empty");
    }   
    options.setHost(host);

    uint16_t port{50000};
    options.setPort(port);

    port = propertyTree.get<uint16_t> (section + ".port", options.getPort());
    options.setPort(port); 

    auto serverCertificate
        = propertyTree.get<std::string> (section + ".serverCertificate", "");
    if (!serverCertificate.empty())
    {   
        if (!std::filesystem::exists(serverCertificate))
        {
            throw std::invalid_argument("gRPC server certificate file "
                                      + serverCertificate
                                      + " does not exist");
        }   
        options.setServerCertificate(loadStringFromFile(serverCertificate));
    }   

    auto accessToken
        = propertyTree.get_optional<std::string> (section + ".accessToken");
    if (accessToken)
    {   
        if (options.getServerCertificate() == std::nullopt)
        {   
            throw std::invalid_argument(
                "Must set server certificate to use access token");
        }
        options.setAccessToken(*accessToken);
    }

    auto clientKey
        = propertyTree.get<std::string> (section + ".clientKey", "");
    auto clientCertificate
        = propertyTree.get<std::string> (section + ".clientCertificate", "");
    if (!clientKey.empty() && !clientCertificate.empty())
    {
        if (!std::filesystem::exists(clientKey))
        {
            throw std::invalid_argument("gRPC client key file "
                                      + clientKey
                                      + " does not exist");
        }
        if (!std::filesystem::exists(clientCertificate))
        {
            throw std::invalid_argument("gRPC client certificate file "
                                      + clientCertificate
                                      + " does not exist");
        }
        options.setClientKey(loadStringFromFile(clientKey));
        options.setClientCertificate(loadStringFromFile(clientCertificate));
    }
    return options;
}

}

namespace UFilterPicker::Options
{

export
struct ProgramOptions
{
    //UFilterPicker::GRPCClientOptions grpcClientOptions;
    UFilterPicker::SubscriberOptions packetSubscriberOptions;
    //NOLINTBEGIN(misc-include-cleaner)
    UFilterPicker::OTelOptions::HTTPMetrics otelHTTPMetricsOptions;
    UFilterPicker::OTelOptions::HTTPLog otelHTTPLogOptions;
    UFilterPicker::OTelOptions::GRPCMetrics otelGRPCMetricsOptions;
    UFilterPicker::OTelOptions::GRPCLog otelGRPCLogOptions;
    //NOLINTEND(misc-include-cleaner)
    std::string applicationName{APPLICATION_NAME}; 
    //std::vector<UDataPacketServiceAPI::V1::StreamIdentifier> streamIdentifiers;
    int verbosity{3};
    bool exportLogsWithHTTP{true};
    bool exportLogs{false};
};

export
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
    //NOLINTBEGIN(misc-include-cleaner)
    auto parsedMap
        = boost::program_options::parse_command_line(argc, argv, desc);
    //NOLINTEND(misc-include-cleaner)
    boost::program_options::store(parsedMap, vm);
    boost::program_options::notify(vm);
    if (vm.count("help"))
    {   
        std::cout << desc << "\n";
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

export
ProgramOptions parseIniFile(const std::filesystem::path &iniFile)
{
    ProgramOptions options;
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

    // GRPC client options
    auto grpcClientOptions
        = ::getGRPCClientOptions(propertyTree, "GRPCClient");
    options.packetSubscriberOptions.setGRPCOptions(grpcClientOptions);
    options.packetSubscriberOptions.setIdentifier(options.applicationName);

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
           // Need to preprocess selector so there's no double spaces
           for (int k = 1; k < static_cast<int> (streamName->size()); )
           {
               if (streamName->at(k - 1) == streamName->at(k) &&
                   streamName->at(k) == ' ')
               {
                   streamName->erase(k, 1);
               }
               else
               {
                   ++k;
               }
            }   

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
            //NOLINTBEGIN(misc-include-cleaner)
            auto identifierString = UFilterPicker::Utilities::toString(identifier);
            //NOLINTEND(misc-include-cleaner)
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
    options.packetSubscriberOptions.setStreamIdentifiers(streams);

    return options;
}

}
