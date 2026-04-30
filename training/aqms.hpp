#ifndef AQMS_HPP
#define AQMS_HPP
#include <algorithm>
#include <memory>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <spdlog/spdlog.h>
#ifndef NDEBUG
#include <cassert>
#endif
#include <pqxx/pqxx>
#include <libmseed.h>
#include <GeographicLib/Geodesic.hpp>

namespace
{
[[nodiscard]]
std::string hexToASCII(const std::string &hex, //const auto &hex, //const std::string &hex,
                       const int outputSize =-1,
                       const bool doul = true)
{
    std::string ascii;
    size_t length = hex.size();
    if (outputSize < 0)
    {   
        ascii.resize(hex.length()/2);
    }   
    else
    {   
        ascii.resize(outputSize);
        length = 2*outputSize; 
    }   
    const auto hexData = reinterpret_cast<const char *> (hex.data());
    std::string part;
    part.resize(2); 
    size_t j{0};
    for (size_t i = 0; i < length; i += 2)
    {   
        part[0] = hexData[i + 0]; 
        part[1] = hexData[i + 1]; 
        //std::string part = hex.substr(i, 2); 
        // change it into base 16 and typecast as the character
        constexpr int base{16};
        if (doul)
        {
            ascii[j] = std::stoul(part, nullptr, base);
        }
        else
        {
            ascii[j] = std::stoi(part.c_str(), nullptr, base);
        }
        // add this char to final ASCII string
        j = j + 1;
    }   
    return ascii;
}

[[nodiscard]]
std::string etypeToEventType(const std::string &type)
{
    if (type == "eq")
    {
        return "earthquake";
    }
    else if (type == "qb")
    {
        return "quarryBlast";
    }
    else
    {
        throw std::runtime_error("Unhandled event type");
    }
}

[[nodiscard]]
int qualityToQuality(const double quality)
{
    constexpr double tolerance{1.e-8};
    if (std::abs(quality - 1) < tolerance)
    {   
        return 0;
    }   
    else if (std::abs(quality - 0.75) < tolerance)
    {   
        return 1;
    }   
    else if (std::abs(quality - 0.50) < tolerance)
    {   
        return 2;
    }   
    else if (std::abs(quality - 0.25) < tolerance)
    {   
        return 3;
    }   
    return 4;
}

}
 
namespace UFilterPicker::Training
{

struct WaveRoot
{
    std::chrono::microseconds onTime;
    std::chrono::microseconds offTime;
    std::string fileRoot;
};

struct Packet
{
    std::string network;
    std::string station;
    std::string channel;
    std::string locationCode;
    std::vector<int> data;
    std::chrono::microseconds startTime{0};
    double samplingRate{100};
};

struct ChannelData
{
    std::string network;
    std::string station;
    std::string channel;
    std::string locationCode;
    std::chrono::seconds onTime;
    double latitude;
    double longitude;
    double elevation;
    double samplingRate;
    double azimuth;
    double dip;
    double gain;
};

struct Magnitude
{
    std::string type;
    double value;
};

struct Pick
{
    ChannelData channelData;
    std::string phase;
    std::chrono::microseconds time;
    double distance;
    double azimuth;
    int quality;
};

struct Origin
{
    std::chrono::microseconds time;
    double latitude;
    double longitude;
    double depth; 
    std::vector<Pick> picks;
};

struct Event
{
    Origin origin;
    Magnitude magnitude;
    int64_t identifier; 
    std::string eventType;
};


[[nodiscard]] 
std::vector<Packet> unpack(std::string &data,
                           const size_t nBytes,
                           const int8_t verbose,
                           const bool purgeTrailingZeros,
                           std::shared_ptr<spdlog::logger> logger)
{
    std::vector<Packet> dataPackets;
    auto bufferSize = static_cast<uint64_t> (nBytes);
    if (data.size() != nBytes)
    {
        throw std::runtime_error("Inconsistent sizes");
    }
    uint64_t offset{0};
    std::vector<double> fullTrace;
    while (bufferSize - offset > MINRECLEN)
    {
        constexpr uint32_t flags{MSF_UNPACKDATA};
        Packet dataPacket;
        MS3Record *miniSEEDRecord{nullptr};
        auto returnCode = msr3_parse(data.c_str() + offset,
                                     static_cast<uint64_t> (bufferSize) - offset,
                                     &miniSEEDRecord, flags,
                                     verbose);
        if (returnCode == MS_NOERROR && miniSEEDRecord)
        {
            // SNCL
            std::array<char, 64> networkWork;
            std::array<char, 64> stationWork;
            std::array<char, 64> channelWork;
            std::array<char, 64> locationWork;
            std::fill(networkWork.begin(),  networkWork.end(), '\0');
            std::fill(stationWork.begin(),  stationWork.end(), '\0');
            std::fill(channelWork.begin(),  channelWork.end(), '\0');
            std::fill(locationWork.begin(), locationWork.end(), '\0');
            returnCode = ms_sid2nslc_n(miniSEEDRecord->sid,
                                       networkWork.data(), networkWork.size(),
                                       stationWork.data(), stationWork.size(),
                                       locationWork.data(), locationWork.size(),
                                       channelWork.data(), channelWork.size());
            std::string network{networkWork.data()};
            std::string station{stationWork.data()};
            std::string channel{channelWork.data()};
            std::string location{locationWork.data()};
            if (locationWork[0] == '\0'){location = "--";}
            if (std::string {"  "} == location.substr(0, 2)){location = "--";}
            if (locationWork[0] == '\0'){location = "--";}
            if (std::string {"  "} == location.substr(0, 2)){location = "--";}
            if (returnCode == MS_NOERROR)
            {
                dataPacket.network = network;
                dataPacket.station = station;
                dataPacket.channel = channel;
                dataPacket.locationCode = location;
            }
            else
            {
                msr3_free(&miniSEEDRecord);
                throw std::runtime_error("Failed to unpack SNCL");
            }
            // Sampling rate
            dataPacket.samplingRate = miniSEEDRecord->samprate;
            // Start time (convert from nanoseconds to microseconds)
            std::chrono::microseconds startTime
            {
                static_cast<int64_t> 
                    (std::round(miniSEEDRecord->starttime*1.e-3))
            };
            dataPacket.startTime = startTime;
            // Data
            auto nSamples = static_cast<int> (miniSEEDRecord->numsamples);
            if (nSamples > 0)
            {
                if (miniSEEDRecord->sampletype == 'i')
                {
                    const auto data
                        = reinterpret_cast<const int *>
                          (miniSEEDRecord->datasamples);
                    dataPacket.data.resize(nSamples);
                    std::copy(data, data + nSamples, dataPacket.data.begin());
                }
                else if (miniSEEDRecord->sampletype == 'f')
                {
                    const auto data
                        = reinterpret_cast<const float *>
                          (miniSEEDRecord->datasamples);
                    dataPacket.data.resize(nSamples);
                    std::copy(data, data + nSamples, dataPacket.data.begin());
                    if (logger)
                    {
                        SPDLOG_LOGGER_WARN(logger, "Casting float to int");
                    }
                }
                else if (miniSEEDRecord->sampletype == 'd')
                {
                    const auto data
                        = reinterpret_cast<const double *>
                          (miniSEEDRecord->datasamples);
                    dataPacket.data.resize(nSamples);
                    std::copy(data, data + nSamples, dataPacket.data.begin());
                    if (logger)
                    {
                        SPDLOG_LOGGER_WARN(logger, "Casting double to int");
                    }
                }
                else
                {
                    throw std::runtime_error("Unhandled data format");
                }
            }
            dataPackets.push_back(std::move(dataPacket));
            offset = offset + miniSEEDRecord->reclen;
            msr3_free(&miniSEEDRecord);
        }
    }
    // Ensure sorted
    std::sort(dataPackets.begin(), dataPackets.end(),
              [](const auto &lhs, const auto &rhs)
              {
                  return lhs.startTime < rhs.startTime;
              });
    // Purge trailing zeros
    if (purgeTrailingZeros)
    {
        auto nPackets = static_cast<int> (dataPackets.size());
        for (int i = nPackets - 1; i >= 0; --i)
        {
            bool allZero{true};
            auto nSamples = dataPackets.at(i).data.size();  
            for (int j = nSamples - 1; j >= 0; --j)
            {
                if (dataPackets[i].data[j] != 0)
                {
                    dataPackets[i].data.resize(j + 1);
                    allZero = false;
                }
            }
            if (!allZero){break;}
         }
    }
    return dataPackets;
}

class AQMS
{
public:
    explicit AQMS(const std::string &connectionString,
                  std::shared_ptr<spdlog::logger> logger = nullptr) :
        mLogger(logger)
    {
        mConnection
           = std::make_unique<pqxx::connection>
             (connectionString);
        if (!isConnected())
        {
            throw std::runtime_error("Failed to connect to AQMS database");
        }
        if (mLogger)
        {
            SPDLOG_LOGGER_INFO(mLogger, "Connected to database.");
        }
        getFileRoots();
    }
    /// Convenience function to make a connection string
    static std::string makeConnectionString(const std::string &user,
                                            const std::string &password,
                                            const std::string &host,
                                            const std::string &databaseName,
                                            const uint16_t port = 5432,
                                            const std::chrono::seconds timeOut = std::chrono::seconds {5},
                                            const std::string &applicationName = "uFilterPickerTraining")
    {
         auto connectionString = "user=" + user
                               + " password=" + password
                               + " host=" + host 
                               + " dbname=" + databaseName
                               + " port=" + std::to_string(port);
         if (timeOut.count() > 0)
         {   
              connectionString = connectionString
                               + " connect_timeout="
                               + std::to_string(timeOut.count());
         }
         if (!applicationName.empty())
         {
             connectionString = connectionString
                              + " application_name=" + applicationName;
         }
         return connectionString;
    }
    /// @result The picks for the stream.
    [[nodiscard]] std::vector<Event> 
        getPPicksForStream(const std::string &network,
                           const std::string &station,
                           const std::string &channel,
                           const std::string &locationCode)
    {
        const auto &geodesic = GeographicLib::Geodesic::WGS84();
        std::vector<Event> events;
        /// Throws
        auto channelData = getChannelDataActiveStream(network,
                                                      station,
                                                      channel,
                                                      locationCode);
        auto iOnTime
            = std::round(static_cast<double> (channelData.onTime.count()));
        constexpr pqxx::zview query
{
R"""(
SELECT event.evid, event.etype,
   TrueTime.getEpoch(origin.datetime, 'UNIX'), origin.lat, origin.lon, origin.depth,
   netmag.magnitude, netmag.magtype,
   arrival.iphase, TrueTime.getEpoch(arrival.datetime, 'UNIX'), arrival.quality
FROM event
  INNER JOIN origin ON event.prefor = origin.orid
    INNER JOIN netmag ON event.prefmag = netmag.magid 
       INNER JOIN assocaro ON assocaro.orid = origin.orid
          INNER JOIN arrival ON assocaro.arid = arrival.arid
WHERE (event.etype = 'eq' OR event.etype = 'qb') AND netmag.magnitude > -9.99 AND origin.gtype = 'l'
  AND origin.rflag = 'F' AND TrueTime.getEpoch(origin.datetime, 'UNIX') > $1
  AND arrival.net = $2 AND arrival.sta = $3 AND arrival.seedchan = $4 AND arrival.location = $5
  AND arrival.iphase = 'P' AND arrival.subsource = 'Jiggle' AND arrival.rflag = 'H' 
  ORDER BY origin.datetime DESC LIMIT 100
)"""};
    if (mLogger)
    {
        SPDLOG_LOGGER_INFO(mLogger,
                           "Beginning P pick query for {}.{}.{}.{} with time > {}",
                           network, station, channel, locationCode, iOnTime);
    }
    pqxx::params queryParameters{iOnTime, network, station, channel, locationCode};
    pqxx::work transaction(*mConnection);
    pqxx::result queryResult = transaction.exec(query, queryParameters);
    for (const auto &row : queryResult)
    {
        auto eventIdentifier = row[0].as<int64_t> ();
        auto eventType = ::etypeToEventType(row[1].as<std::string> ());
        auto originTime
            = std::chrono::microseconds
              {static_cast<int64_t> (std::round(row[2].as<double> ()*1.e6))};
        auto eventLatitude  = row[3].as<double> ();
        auto eventLongitude = row[4].as<double> ();
        auto eventDepth = row[5].as<double> ();
        auto magnitudeValue = row[6].as<double> ();
        auto magnitudeType = row[7].as<std::string> ();       
        auto phase = row[8].as<std::string> ();
        auto pickTime
            = std::chrono::microseconds
              {static_cast<int64_t> (std::round(row[9].as<double> ()*1.e6))};
        auto quality = ::qualityToQuality(row[10].as<double> ());

        double distanceInMeters{0};
        double azimuth{0};
        double backAzimuth{0}; // Starts as forward azimuth [-180, 180]
        geodesic.Inverse(eventLatitude, eventLongitude, 
                         channelData.latitude, channelData.longitude,
                         distanceInMeters, azimuth, backAzimuth);
        // Convert to back azimuth while putting in range [0, 360)
        backAzimuth = backAzimuth + 180;

        Magnitude magnitude{magnitudeType, magnitudeValue};
        Pick pick{channelData, phase, pickTime,
                  distanceInMeters, backAzimuth, quality};
        Origin origin{originTime,
                      eventLatitude, eventLongitude, eventDepth,
                      std::vector<Pick> {pick}};
        Event event{std::move(origin), std::move(magnitude), eventIdentifier, eventType};

        events.push_back(std::move(event));
    }
    transaction.commit();
    if (mLogger)
    {
        SPDLOG_LOGGER_INFO(mLogger,
                           "Finished with picks from {} events for {}.{}.{}.{}",
                           events.size(),
                           network, station, channel, locationCode);
    } 
    std::sort(events.begin(), events.end(),
              [](const auto &lhs, const auto &rhs)
              {
                 return lhs.origin.time < rhs.origin.time;
              });
    return events;
    }
    /// @result The ondate for the stream.
    [[nodiscard]] 
    ChannelData getChannelDataActiveStream(const std::string &network,
                                           const std::string &station,
                                           const std::string &channel,
                                           const std::string &locationCode)
    {
        ChannelData channelData;
#ifndef NDEBUG
        assert(isConnected());
#endif
        constexpr pqxx::zview query{
R"""(
SELECT EXTRACT(epoch FROM channel_data.ondate), lat, lon, elev, samprate, azimuth, dip, gain 
  FROM channel_data INNER JOIN simple_response
    ON channel_data.net = simple_response.net AND
       channel_data.sta = simple_response.sta AND
       channel_data.seedchan = simple_response.seedchan
       AND channel_data.location = simple_response.location
  WHERE channel_data.net = $1 AND 
        channel_data.sta = $2 AND 
        channel_data.seedchan = $3 AND
        channel_data.location = $4 AND
        channel_data.offdate > NOW() AND
        simple_response.offdate > NOW()
)"""
        };  
        bool gotOne{false};
        pqxx::params queryParameters{network, station, channel, locationCode};
        pqxx::work transaction(*mConnection);
        pqxx::result queryResult = transaction.exec(query, queryParameters);
        if (!queryResult.empty())
        {
            gotOne = true;
            const auto row = queryResult[0];
            auto iStartTimeS
                 = static_cast<int64_t> (std::round(row[0].as<double> ()));
            channelData.network = network;
            channelData.station = station;
            channelData.channel = channel;
            channelData.locationCode = locationCode;
            channelData.onTime = std::chrono::seconds {iStartTimeS};
            channelData.latitude = row[1].as<double> ();
            channelData.longitude = row[2].as<double> ();
            channelData.elevation = row[3].as<double> ();
            channelData.samplingRate = row[4].as<double> ();
            channelData.azimuth = row[5].as<double> ();
            channelData.dip = row[6].as<double> ();
            channelData.gain = row[7].as<double> ();
        }
        transaction.commit(); 
        if (!gotOne)
        {
            throw std::runtime_error(network + "."
                                   + station + "."
                                   + channel + "."
                                   + locationCode + " is not active");
        }
        return channelData;
    }

    [[nodiscard]] std::pair<std::string, int>
        getFileNameAndSize(const int64_t eventIdentifier,
                           const std::string &network,
                           const std::string &station,
                           const std::string &channel,
                           const std::string &locationCode,
                           const std::string &fileRoot)
    {
#ifndef NDEBUG
        assert(isConnected());
#endif
        std::string fileName;
        int nBytes{0};
        constexpr pqxx::zview query {
R"""(
SELECT filename.dfile, filename.nbytes FROM AssocWaE 
   INNER JOIN waveform ON waveform.wfid = AssocWaE.wfid
      INNER JOIN filename ON filename.fileid = waveform.fileid 
WHERE AssocWaE.evid = $1 AND
      waveform.net = $2 AND waveform.sta = $3 AND
      waveform.seedchan = $4 AND waveform.location = $5 LIMIT 1
)"""
        };
        bool gotOne{false};
        pqxx::params queryParameters{eventIdentifier, 
                                     network,
                                     station,
                                     channel,
                                     locationCode};
        pqxx::work transaction(*mConnection);
        pqxx::result queryResult = transaction.exec(query, queryParameters);
        if (!queryResult.empty())
        {
            gotOne = true; 
            const auto row = queryResult[0]; 
            fileName = row[0].as<std::string> ();
            nBytes = row[1].as<int> ();
        }
        transaction.commit();
        if (!gotOne && mLogger)
        {
            SPDLOG_LOGGER_WARN(mLogger, 
                               "Failed to create filename for {}.{}.{}.{} (event {}) because its filename could not be found.",
                               network, station, channel, locationCode,
                               eventIdentifier);
            return std::pair {fileName, nBytes};
        }
        auto work = std::filesystem::path {fileRoot} /
                    std::filesystem::path {std::to_string(eventIdentifier)} /
                    std::filesystem::path {fileName};
        fileName = work;
        return std::pair {fileName, nBytes};
    }

    void getFileRoots()
    {
        waveRoots.clear();
        constexpr pqxx::zview query {
R"""(
SELECT datetime_on, datetime_off, fileroot FROM waveroots WHERE status = 'A' ORDER BY datetime_on ASC
)"""
        };
        pqxx::work transaction(*mConnection);
        pqxx::result queryResult = transaction.exec(query);
        for (const auto &row : queryResult)
        {
            auto onTime  = std::chrono::seconds {row[0].as<int64_t> ()};
            auto offTime = std::chrono::seconds {row[1].as<int64_t> ()};
            auto fileRoot = row[2].as<std::string> ();
            WaveRoot waveRoot{onTime, offTime, fileRoot};
            waveRoots.push_back(std::move(waveRoot));
        }
    }

    [[nodiscard]] std::string getFileRoot(const std::chrono::microseconds &originTime)
    {
        for (const auto &waveRoot : waveRoots)
        {
            if (originTime >= waveRoot.onTime && originTime < waveRoot.offTime)
            {
                return waveRoot.fileRoot;
            }
        }
        throw std::runtime_error("Couldn't find file root");
    }

    [[nodiscard]] std::vector<Packet>
        getWaveform(const int64_t eventIdentifier,
                    const std::chrono::microseconds &originTime,
                    const std::string &network,
                    const std::string &station,
                    const std::string &channel,
                    const std::string &locationCode)
    {
        std::vector<Packet> packets;
        auto fileRoot = getFileRoot(originTime);
        auto [fileName, nBytes]
              = getFileNameAndSize(eventIdentifier,
                                   network, station, channel, locationCode,
                                   fileRoot);
        constexpr pqxx::zview query {
R"""(
SELECT encode(wave.get_waveform_blob($1, 0, $2, 0, 4070908800), 'hex')
)"""
        };
        bool gotOne{false};
        pqxx::params queryParameters{fileName, nBytes};
        pqxx::work transaction(*mConnection);
        pqxx::result queryResult = transaction.exec(query, queryParameters);
        if (!queryResult.empty())
        {   
            gotOne = true; 
            const auto row = queryResult[0];
            auto binaryData = row[0].as<std::string> ();
            if (binaryData.empty())
            {
                throw std::runtime_error("No data found for "
                                        + network + "." + station
                                        + channel + "." + locationCode
                                        + " (event "
                                        + std::to_string(eventIdentifier)
                                        + ")");
            }            
            constexpr bool doul{true};
            constexpr bool purgeTrailingZeros{true};
            try
            {
                auto miniSEED = ::hexToASCII(binaryData, nBytes, doul);
                constexpr int8_t verbose{0};
                packets = unpack(miniSEED, nBytes, verbose,
                                 purgeTrailingZeros, mLogger);
            }
            catch (const std::exception &e)
            {
                if (mLogger)
                {
                    SPDLOG_LOGGER_ERROR(mLogger,
                                        "hexToASCII failed because of {}",
                                        std::string {e.what()}); 
                    gotOne = false;
                }
            }
        } 
        transaction.commit();
        if (!gotOne)
        {
            throw std::runtime_error("Failed to get waveform");
        }
        return packets;
    }

    [[nodiscard]] bool isConnected() const noexcept
    {
        if (mConnection)
        {
            return mConnection->is_open();
        }
        return false;
    }

    AQMS() = delete;
    /// Destructor
    ~AQMS()
    {
        if (mConnection){mConnection->close();}
        mConnection = nullptr;
    }
private:
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::unique_ptr<pqxx::connection> mConnection{nullptr};
    std::vector<WaveRoot> waveRoots;
};

}
#endif
