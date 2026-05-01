#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <utility>
#include <exception>
#include <cmath>
#include <limits>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <memory>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include "aqms.hpp"
#include "featuresDatabase.hpp"
#include "uFilterPicker/narrowBandFilter.hpp"
#include "uFilterPicker/envelope.hpp"
#include "uFilterPicker/characteristicFunction.hpp"
#include "uFilterPicker/pipeline.hpp"
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/thresholdTrigger.hpp"

#define APPLICATION_NAME "uFilterPickerFeatures"

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
    UFilterPicker::EnvelopeOptions envOptions{envelopeLength, 8.0};
    auto envelope
        = std::make_unique<UFilterPicker::Envelope> (envOptions);
    auto characteristicFunction
        = std::make_unique<UFilterPicker::CharacteristicFunction> (characteristicFunctionLength);
    auto pipeline
        = std::make_unique<UFilterPicker::Pipeline> (std::move(narrowBandFilter),
                                                     std::move(envelope),
                                                     std::move(characteristicFunction)); 
    return pipeline;
} 

std::unique_ptr<UFilterPicker::Detector>
    createDetector(const int butterworthOrder,
                   const std::vector< std::pair<double, double>> &passbands,
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
    auto detector = std::make_unique<UFilterPicker::Detector> (std::move(pipelines));
    return detector;
}

std::unique_ptr<UFilterPicker::Detector>
    createBroadbandDetector(const int order = 5,
                            const double samplingRate = 100)
{
    auto detector = ::createDetector(order,
                                     std::vector<std::pair<double, double>> 
                                     {
                                        { 3 - 1,  8 + 3}, 
                                        { 8 - 3, 13 + 3}, 
                                        {13 - 3, 18 + 3}, 
                                        {18 - 3, 23 + 3}, 
                                        {23 - 3, 28 + 3}, 
                                        {28 - 3, 33 + 3}
                                     },
                                     400,
                                     400,
                                     samplingRate);
    //auto detector = UFilterPicker::Detector::create100HzBroadband();
    return detector;
}

struct Stream
{
    std::string network;
    std::string station;
    std::string channel;
    std::string locationCode;
};

struct DatabaseOptions
{
    std::string user;
    std::string password;
    std::string host;
    std::string name;
    uint16_t port{5432};
    std::chrono::seconds timeOut{5};
};

struct ProgramOptions
{
    std::string applicationName{APPLICATION_NAME};
    std::string authority{"uu"};
    std::filesystem::path featuresFile{"features.sqlite3"};
    DatabaseOptions databaseOptions;
    std::vector<Stream> streamList{ Stream{"UU", "DCU", "EHZ", "01"} };
//,
//                                    Stream{"UU", "TCRU", "HHZ", "01"} };
    std::chrono::microseconds pickWindow{std::chrono::milliseconds {250}};
    int butterworthOrder{5};
    int burnInFactor{2};
    int verbosity{3};
    bool dumpDebugFiles{false};
};

struct ProgramOptions parseIniFile(const std::filesystem::path &iniFile);
std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[]);

static std::pair<int, int> getMinMax(
    const std::vector<UFilterPicker::Training::Packet> &packets)
{
   
    int iMin = std::numeric_limits<int>::max();
    int iMax = std::numeric_limits<int>::lowest();
    for (const auto &packet : packets)
    {
        for (int i = 0; i < static_cast<int> (packet.data.size()); ++i)
        {
            iMin = std::min(iMin, packet.data[i]); 
            iMax = std::max(iMax, packet.data[i]);
        }
    }
    return std::pair{iMin, iMax};
}

double rescale(const int y, 
               const std::pair<int, int> &yMinMax, //const int yMin, const int yMax,
               const std::pair<double, double> &newRange) //double a, const double b)
{
    auto yMin = yMinMax.first;
    auto yMax = yMinMax.second;
    auto a = newRange.first;
    auto b = newRange.second;
    double denom = yMax - yMin;
    if (denom == 0){denom = 1;}
    return a + ((y - yMin)*(b - a))/denom;
}

int main(int argc, char *argv[])
{
    auto logger = spdlog::stdout_color_st("consoleSubToAll");

    // Get the ini file from the command line
    std::string iniFile;
    try
    {
        //NOLINTBEGIN(misc-include-cleaner)
        auto [iniFileName, isHelp]
            = parseCommandLineOptions(argc, argv);
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
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to read command line arguments because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }


    ::ProgramOptions programOptions;
    try
    {
        programOptions = parseIniFile(iniFile);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to parse ini file because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }   

    // Initialize the features file
    std::unique_ptr<UFilterPicker::Training::FeaturesDatabase>
        featuresDatabase{nullptr};
    try
    {
        featuresDatabase
            = std::make_unique<UFilterPicker::Training::FeaturesDatabase>
              (
                  logger,
                  programOptions.featuresFile,
                  UFilterPicker::Training::FeaturesDatabase::Mode::Create
              );
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to open features database because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Initialize the AQMS connection string
    auto connectionString
        = UFilterPicker::Training::AQMS::makeConnectionString(
              programOptions.databaseOptions.user,
              programOptions.databaseOptions.password,
              programOptions.databaseOptions.host,
              programOptions.databaseOptions.name,
              programOptions.databaseOptions.port,
              programOptions.databaseOptions.timeOut,
              programOptions.applicationName);
 
    std::unique_ptr<UFilterPicker::Training::AQMS> aqms{nullptr};
    try
    {
        aqms = std::make_unique<UFilterPicker::Training::AQMS> (connectionString, logger);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(logger,
                               "Failed to create AQMS database because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }
#ifndef NDEBUG
    assert(aqms);
#endif
    for (const auto &stream : programOptions.streamList)
    {
        std::vector<UFilterPicker::Training::Event> events;
        try
        {
            auto locationCode = stream.locationCode;
            if (stream.network == "PB"){locationCode = "  ";}
            if (stream.network == "NN"){locationCode = "  ";}
            events = aqms->getPPicksForStream(stream.network,
                                              stream.station,
                                              stream.channel,
                                              locationCode);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(logger, "Pick query failed with {}", e.what());
            continue;
        }
        if (events.empty())
        {
            SPDLOG_LOGGER_WARN(logger, "No picks found for {}.{}.{}.{}",
                               stream.network, stream.station,
                               stream.channel, stream.locationCode);
            continue;
        }
        auto detector = UFilterPicker::Detector::create100HzBroadband();
        const auto groupDelay = detector->getGroupDelay(); 
        const auto burnInTime = groupDelay*programOptions.burnInFactor;
        //    = createBroadbandDetector(programOptions.butterworthOrder,
        //         events.at(0).origin.picks.at(0).channelData.samplingRate);
        // Get the waveforms
        for (const auto &event : events)
        {
            // Add event to output database
            UFilterPicker::Training::EventRow eventRow;
            eventRow.identifier = programOptions.authority
                                + std::to_string(event.identifier);
            eventRow.eventType = event.eventType;
            eventRow.latitude = event.origin.latitude;
            eventRow.longitude = event.origin.longitude;
            eventRow.depth = event.origin.depth;
            eventRow.eventTime = event.origin.time;
            eventRow.preferredMagnitudeType = event.magnitude.type;
            eventRow.preferredMagnitude = event.magnitude.value;
            try
            {
                featuresDatabase->write(eventRow);
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_CRITICAL(logger,
                                       "Failed to write event because: {}",
                                       std::string {e.what()});
                return EXIT_FAILURE;
            }
            
            std::pair<std::chrono::microseconds, double> estimatePickTimeAndValue
            {
                std::chrono::microseconds {0},
                std::numeric_limits<double>::lowest()
            };
            std::vector<double> cfValues(detector->getNumberOfPipelines());
            std::fill(cfValues.begin(), cfValues.end(), -1);
            if (event.origin.picks.empty())
            {
                SPDLOG_LOGGER_WARN(logger,
                                   "Did not make pick for {}",
                                   event.identifier);
                continue;
            }
#ifndef NDEBUG
            assert(event.origin.picks.size() == 1); 
#endif
            std::chrono::microseconds traceStartTime;
            const auto channelData = event.origin.picks.at(0).channelData;
            const auto truePickTime = event.origin.picks.at(0).time;
            bool gapDetected{false};
            try
            {
//std::cout << event.origin.picks.at(0).distance*1.e-3 << " " << event.magnitude.value << std::endl;
                auto packets = aqms->getWaveform(event.identifier,
                                                 event.origin.time,
                                                 channelData.network,
                                                 channelData.station,
                                                 channelData.channel,
                                                 channelData.locationCode);
                auto [iMin, iMax] = ::getMinMax(packets);
                traceStartTime = packets.at(0).startTime;
                // Skip if pick is too early in trace
                if (truePickTime < traceStartTime + burnInTime)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                  "Pick precedes burn in time for {}; skipping",
                                  event.identifier);
                    continue;
                }
                auto t0ShiftedEnv = traceStartTime.count()
                                  + detector->getGroupDelay().count(); 


                std::string outFileName;
                std::ofstream ofile;
                if (programOptions.dumpDebugFiles)
                {
                    if (!std::filesystem::exists("testWaves"))
                    {
                        std::filesystem::create_directories("testWaves");
                    }
                    outFileName = "testWaves/" + std::to_string(event.identifier) + "-"
                                + channelData.network + "." + channelData.station + "."
                                + channelData.channel + "." + channelData.locationCode;
                    ofile.open(outFileName);
                }
                // Loop on packets
                std::vector<double> cfEnv2Full;
                std::vector<double> xFull;
                std::chrono::microseconds lastPacketEndTime{-1};
                for (const auto &packet : packets) //int i = 0; i < static_cast<int> (packets.size()); ++i)
                {
                    if (packet.data.empty())
                    {
                        SPDLOG_LOGGER_WARN(logger, "Empty packet");
                        continue;
                    }
                    auto gapDuration = packet.startTime - lastPacketEndTime;
                    if (lastPacketEndTime.count() >-1 &&
                        std::abs(gapDuration.count()) > std::chrono::microseconds {std::chrono::milliseconds {50}}.count())
                    {
                        SPDLOG_LOGGER_WARN(logger, "Gap detected");
                        lastPacketEndTime = std::chrono::microseconds {-1};
                        gapDetected = true;
                        break;
                    }
                    auto cfEnv2 = detector->apply(packet.data);
                    // If we're in the pick window then extract the max
                    const auto dtMuS 
                        = static_cast<int64_t>
                          (std::round(1000000/packet.samplingRate));
                    const auto pickWindow = programOptions.pickWindow;
                    for (int k = 0; k < static_cast<int> (cfEnv2.size()); ++k)
                    {
                        auto sampleTime = packet.startTime
                                        + std::chrono::microseconds {k*dtMuS};
                        lastPacketEndTime = std::max(sampleTime, lastPacketEndTime); 
                        auto shiftedEnvelopeTime  = sampleTime - groupDelay;
                        if (sampleTime - traceStartTime < burnInTime){cfEnv2[k] = 0;} 
                        if (shiftedEnvelopeTime >= truePickTime - pickWindow &&
                            shiftedEnvelopeTime <= truePickTime + pickWindow)
                        {
                            const auto value = cfEnv2[k];
                            if (value >= estimatePickTimeAndValue.second)
                            {
                                estimatePickTimeAndValue.first
                                    = shiftedEnvelopeTime;
                                estimatePickTimeAndValue.second = value; 
                                for (size_t ip = 0; ip < cfValues.size(); ++ip)
                                {
                                    const auto &cfEnv2ip = detector->getCharacteristicFunction(ip);
                                    cfValues[ip] = cfEnv2ip[k];
//std::cout << ip << " " << cfValues[ip] << " " << cfEnv2.at(k) << " " << cfEnv2ip.size() << " " << cfEnv2.size() << std::endl;
                                }
                            }
                        }
                    }
                    xFull.insert(xFull.begin(), packet.data.begin(), packet.data.end());
                    cfEnv2Full.insert(cfEnv2Full.end(), cfEnv2.begin(), cfEnv2.end());

                    if (programOptions.dumpDebugFiles)
                    {
                        auto reducedStartTime = (packet.startTime.count() - traceStartTime.count())*1.e-6;
                        //auto startTimeShifted = (packet.startTime.count() - t0Shifted)*1.e-6;
                        auto startTimeShiftedEnv = (packet.startTime.count() - t0ShiftedEnv)*1.e-6;
                        const auto dt = 1./packet.samplingRate;
                        for (int k = 0; k < static_cast<int> (cfEnv2.size()); ++k)
                        {
                            ofile << std::setprecision(12)
                                  << reducedStartTime + k*dt << " "
                                  << rescale(packet.data[k], std::pair {iMin, iMax}, std::pair {-1.0, 1.0}) << " " 
                                 //<< startTimeShifted + i*dt << " " << yFiltered.at(k) << " " 
                                 //<< startTimeShiftedEnv + i*dt << " " << yEnv.at(i) << " " 
                                 //<< startTimeShiftedEnv + i*dt + 0 << " " << cfEnv.at(i) << " "
                                 << startTimeShiftedEnv + k*dt + 0 << " "
                                 << cfEnv2.at(k)
                                 << std::endl;
                        }
                    }
                } // Loop on packets
                if (programOptions.dumpDebugFiles){ofile.close();}
            }
            catch (const std::exception &e)
            {
                SPDLOG_LOGGER_WARN(logger, "Failed to get data for {} because {}",
                                   event.identifier, std::string {e.what()}); 
            }
            if (estimatePickTimeAndValue.second >
                std::numeric_limits<double>::lowest() && !gapDetected)
            {
                UFilterPicker::Training::Row row;
                UFilterPicker::Training::Stream sRow;
                sRow.network = stream.network;
                sRow.station = stream.station;
                sRow.channel = stream.channel;
                sRow.locationCode = stream.locationCode;
                sRow.latitude = event.origin.picks[0].channelData.latitude;
                sRow.longitude = event.origin.picks[0].channelData.longitude;
                sRow.elevation = event.origin.picks[0].channelData.elevation;
                row.stream = sRow;
                //row.network = stream.network;
                //row.station = stream.station;
                //row.channel = stream.channel;
                //row.locationCode = stream.locationCode;
                row.eventIdentifier = eventRow.identifier;
                row.distance = event.origin.picks.at(0).distance*1.e3; // KM
                row.backAzimuth = event.origin.picks[0].backAzimuth;
                row.analystQuality = event.origin.picks[0].quality;
                row.truePickTime = truePickTime;
/*
    std::vector<double> cfValues;
*/
                row.estimatePickTime = estimatePickTimeAndValue.first; 
                row.cfValueAtPick = estimatePickTimeAndValue.second;
                row.nominalSamplingRate = event.origin.picks[0].channelData.samplingRate;

featuresDatabase->write(row);

                auto residual = (estimatePickTimeAndValue.first.count() - truePickTime.count())*1.e-6;
                std::cout << event.identifier << ","
                          << event.origin.latitude << ","
                          << event.origin.longitude << ","
                          << event.origin.depth << ","
                          << event.origin.picks.at(0).distance*1.e-3 << ","
                          << event.magnitude.value << ","
                          << event.magnitude.type << ","
                          << event.origin.picks.at(0).quality << "," 
                          << residual << ","
                          << truePickTime.count() << ","
                          << estimatePickTimeAndValue.first.count() << ","
                          << estimatePickTimeAndValue.second << ",";
                for (int ip = 0; ip < static_cast<int>(cfValues.size()) - 1; ++ip)
                {
                     std::cout << cfValues[ip] << ",";
                }
                std::cout << cfValues.back() << std::endl;
            }
break;
        } // Loop on picks
    }
    return EXIT_SUCCESS;
}

std::pair<std::string, bool> parseCommandLineOptions(int argc, char *argv[])
{
    std::string iniFile;
    boost::program_options::options_description desc(R"""(
This utility collects features and responses for developing detection 
models for the uFilterPicker.
    
    uFilterPickerFeatures --ini=features.ini
    
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

struct ProgramOptions parseIniFile(const std::filesystem::path &iniFile)
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

    // AQMS database
    DatabaseOptions aqmsOptions;
    aqmsOptions.user = propertyTree.get<std::string> ("AQMS.user");
    aqmsOptions.password = propertyTree.get<std::string> ("AQMS.password");
    aqmsOptions.host = propertyTree.get<std::string> ("AQMS.host", "localhost");
    aqmsOptions.name = propertyTree.get<std::string> ("AQMS.databaseName");
    aqmsOptions.port = propertyTree.get<uint16_t> ("AQMS.port", aqmsOptions.port);
    options.databaseOptions = aqmsOptions; 

    // Output
    auto featuresFile
        = propertyTree.get<std::string> ("FeaturesDatabase.featuresFile",
                                         "features.sqlite3");
    if (featuresFile.empty())
    {
         throw std::invalid_argument("Invalid output filename");
    }
    auto featuresFilePath = std::filesystem::path{featuresFile};
    if (!std::filesystem::exists(featuresFilePath))
    {
        auto parentPath = featuresFilePath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath))
        {
            if (!std::filesystem::create_directories(parentPath))
            {
                throw std::invalid_argument("Could not make parent path "
                                           + std::string {parentPath});
            }
        }
    }
    options.featuresFile = featuresFilePath; 
    
    options.burnInFactor
        = propertyTree.get<int> ("UFilterPicker.burnInFactor",
                                 options.burnInFactor);
    if (options.burnInFactor < 0)
    {
        throw std::invalid_argument("Burn in factor must be positive");
    }

    std::set<std::string> streamNames;
    std::vector<Stream> streams;
    for (uint16_t i = 1; i <= std::numeric_limits<uint16_t>::max(); ++i)
    {
        auto keyName = "UFilterPicker.stream_"
                     + std::to_string(i);
        auto sensor = propertyTree.get_optional<std::string> (keyName);
        if (sensor)
        {
            // Need to preprocess selector so there's no double spaces
            for (int k = 1; k < static_cast<int> (sensor->size()); )
            {
                if (sensor->at(k - 1) == sensor->at(k) &&
                    sensor->at(k) == ' ') 
                {
                    sensor->erase(k, 1); 
                }
                else
                {
                    ++k;
                }
            }   
            std::vector<std::string> splitSensor;
            boost::split(splitSensor, *sensor,
                         boost::is_any_of(" \t"));
            if (splitSensor.size() < 3)
            {
                throw std::invalid_argument("Sensor should have format NN SS CCC LL - got " + *sensor);
            }
            auto network = splitSensor.at(0);
            auto station = splitSensor.at(1);
            auto channel = splitSensor.at(2);
            std::string locationCode = "--";
            if (splitSensor.size() > 4)
            {
                locationCode = splitSensor.at(3);
            }
            auto streamName = network + "."
                            + station + "."
                            + channel + "."
                            + locationCode;
            if (streamNames.contains(streamName))
            {
                throw std::invalid_argument("Duplciate stream: " + streamName);
            }
            streamNames.insert(streamName);
            Stream stream{network, station, channel, locationCode};
            streams.push_back(stream);
        }
        else
        {
            break;
        }
    }
    if (streams.empty()){throw std::invalid_argument("No streams");}
    options.streamList = streams;

    return options;
}
