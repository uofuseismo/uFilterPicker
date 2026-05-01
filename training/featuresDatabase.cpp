#include <iostream>
#include <string>
#include <stdexcept>
#include <filesystem>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <sqlite3.h>
#include "uFilterPicker/version.hpp"
#include "featuresDatabase.hpp"

using namespace UFilterPicker::Training;

namespace
{

[[nodiscard]] std::string toName(const std::string &network,
                                 const std::string &station,
                                 const std::string &channel,
                                 const std::string &locationCode)
{
    auto result = network + "." + station + "." + channel;
    if (!locationCode.empty())
    {
        result = result + "." + locationCode;
    }
    else
    {
        result = result + ".--";
    }
    return result;
}

void bindText(const std::string &text,
              const int index,
              const std::string &column,
              const std::string &table,
              sqlite3_stmt *statement)
{
#ifndef NDEBUG
    assert(index > 0);
#endif
    auto returnCode
        = sqlite3_bind_text(
             statement, index,
             text.c_str(), static_cast<int> (text.size()), nullptr);
    if (returnCode != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        throw std::runtime_error("Failed to bind "
                               + text + " to "
                               + column + " " + table);
    }
}

void bindInt(const int value,
             const int index,
             const std::string &column,
             const std::string &table,
             sqlite3_stmt *statement) 
{           
#ifndef NDEBUG
    assert(index > 0); 
#endif
    auto returnCode
        = sqlite3_bind_int(
             statement, index,
             value);
    if (returnCode != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        throw std::runtime_error("Failed to bind "
                               + std::to_string(value) + " to "
                               + column + " " + table);
    }
}

void bindInt64(const int64_t value,
               const int index,
               const std::string &column,
               const std::string &table,
               sqlite3_stmt *statement)
{
#ifndef NDEBUG
    assert(index > 0); 
#endif
    auto returnCode
        = sqlite3_bind_int64(
             statement, index,
             value);
    if (returnCode != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        throw std::runtime_error("Failed to bind "
                               + std::to_string(value) + " to "
                               + column + " " + table);
    }   
}


void bindDouble(const double value,
                const int index,
                const std::string &column,
                const std::string &table,
                sqlite3_stmt *statement)
{
#ifndef NDEBUG
    assert(index > 0);
#endif
    auto returnCode
        = sqlite3_bind_double(
             statement, index,
             value);
    if (returnCode != SQLITE_OK)
    {
        sqlite3_finalize(statement);
        throw std::runtime_error("Failed to bind "
                               + std::to_string(value) + " to "
                               + column + " " + table);
    }
}

}

class FeaturesDatabase::FeaturesDatabaseImpl
{
public:
    FeaturesDatabaseImpl(std::shared_ptr<spdlog::logger> logger,
                         const std::filesystem::path &fileName,
                         const FeaturesDatabase::Mode mode) :
        mLogger(std::move(logger)),
        mMode(mode)
    {
        if (mLogger == nullptr)
        {
            //NOLINTBEGIN(misc-include-cleaner)
            auto classId
                = std::to_string (reinterpret_cast<std::uintptr_t> (this));
            mLogger = spdlog::stdout_color_mt("sqlite-db-" + classId);
            //NOLINTEND(misc-include-cleaner)
        }
#ifndef NDEBUG
        assert(mLogger != nullptr);
#endif
        if (mode == FeaturesDatabase::Mode::ReadOnly)
        {
            openReadOnly(fileName);
        }
        else
        {
            bool createDatabase{false};
            if (mode == FeaturesDatabase::Mode::Create)
            {
                createDatabase = true;
                if (std::filesystem::exists(fileName))
                {
                    std::filesystem::remove(fileName);
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Removing existing database {}",
                                       std::string{fileName});
                }
                const auto directory = fileName.parent_path();
                if (!directory.empty())
                {
                    if (!std::filesystem::exists(directory))
                    {
                        if (!std::filesystem::create_directories(directory))
                        {
                            throw std::runtime_error(
                                "Failed to create directory "
                               + std::string {directory});
                        }
                    }
                }
            }
            else // We're in read-write mode
            {
                if (!std::filesystem::exists(fileName))
                {
                    createDatabase = true;
                    const auto directory = fileName.parent_path();
                    if (!directory.empty())
                    {
                        if (!std::filesystem::exists(directory))
                        {
                            if (!std::filesystem::create_directories(directory))
                            {
                                throw std::runtime_error(
                                    "Failed to create directory for missing db "
                                   + std::string {directory});
                            }
                        }
                    }
                    SPDLOG_LOGGER_INFO(mLogger,
                                       "Will create missing database {}",
                                       std::string{fileName});
                }
            }
            openReadWrite(fileName, createDatabase);
        }
    }

    void openReadWrite(const std::filesystem::path &fileName,
                       const bool createDatabase)
    {
        int flags = SQLITE_OPEN_READWRITE;
        if (createDatabase)
        {
            SPDLOG_LOGGER_INFO(mLogger, "Will create database {}",
                               std::string{fileName});
            mTablesInitialized = false;
            flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        }
        else
        {
            SPDLOG_LOGGER_INFO(mLogger,
                               "Will open database {} as read-write",
                               std::string{fileName});
            mTablesInitialized = true;
        }
        const char *vfs{nullptr};
        auto returnCode = sqlite3_open_v2(fileName.c_str(),
                                          &mDatabaseHandle,
                                          flags,
                                          vfs);
        if (returnCode != SQLITE_OK)
        {
            auto errorMessage = std::string{sqlite3_errstr(returnCode)};
            if (createDatabase)
            {
                throw std::runtime_error(
                   "Failed to open database in create mode because "
                  + errorMessage);
            }
            throw std::runtime_error("Failed to open database because "
                                   + errorMessage);
        }
        mIsOpen = true;
        mMode = FeaturesDatabase::Mode::ReadWrite;
        if (createDatabase)
        {
            create();
        }
    }

    void close()
    {
        if (isOpen())
        {
            SPDLOG_LOGGER_INFO(mLogger, "Closing database");
            sqlite3_close(mDatabaseHandle);
            mMode = FeaturesDatabase::Mode::ReadOnly;
            mIsOpen = false;
            mDatabaseHandle = nullptr;
        }
    }

    [[nodiscard]] bool isOpen() const noexcept
    {   
        return mIsOpen;
    }

    void openReadOnly(const std::filesystem::path &fileName)
    {
        close();
        if (!std::filesystem::exists(fileName))
        {
            throw std::invalid_argument("Cannot open "
                              + std::string {fileName}
                              + " in read-only mode because it does not exist");
        }
        const char *vfs{nullptr};
        const int flags{SQLITE_OPEN_READWRITE};
        auto returnCode = sqlite3_open_v2(fileName.c_str(),
                                          &mDatabaseHandle,
                                          flags,
                                          vfs);
        if (returnCode != SQLITE_OK)
        {
            auto errorMessage = std::string{sqlite3_errstr(returnCode)};
            throw std::runtime_error(
                "Failed to open read-only database because "
              + errorMessage);
        }
        mIsOpen = true;
        mTablesInitialized = true;
        mMode = FeaturesDatabase::Mode::ReadOnly;
    }

    [[nodiscard]] bool eventExists(const std::string &eventIdentifier) const
    {
        bool exists{false};
        if (!isOpen()){throw std::runtime_error("Database not open");}
        const std::string query{
R"""(
SELECT COUNT(*) FROM events WHERE identifier = ?;
)"""
        };
        sqlite3_stmt *statement{nullptr};
        auto returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                             query.c_str(),
                                             -1, 
                                             &statement, 
                                             nullptr);
        if (returnCode != SQLITE_OK)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to prepare event count statement");
        }
        ::bindText(eventIdentifier, 1, "identifier", "events", statement);
        returnCode = sqlite3_step(statement);
        if (returnCode == SQLITE_ROW)
        {
            auto iExists = sqlite3_column_int(statement, 0);
            if (iExists >= 1){exists = 1;}
            sqlite3_finalize(statement);
        }
        else
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Query did not return row");
        } 
        return exists;
    }

    [[nodiscard]] bool streamExists(const std::string &network,
                                    const std::string &station,
                                    const std::string &channel,
                                    const std::string &locationCodeIn) const
    {
        bool exists{false};
        if (!isOpen()){throw std::runtime_error("Database not open");}
        const std::string query{
R"""(
SELECT COUNT(*) FROM streams WHERE network = ? AND station = ? AND channel = ? AND location_code = ?;
)"""    
        };
        sqlite3_stmt *statement{nullptr};
        auto returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                             query.c_str(),
                                             -1,
                                             &statement,
                                             nullptr);
        if (returnCode != SQLITE_OK)
        {   
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to prepare streams count statement");
        }
        auto locationCode = locationCodeIn;
        if (locationCode.empty()){locationCode = "--";}
        ::bindText(network,      1, "network",       "streams", statement);
        ::bindText(station,      2, "station",       "streams", statement);
        ::bindText(channel,      3, "channel",       "streams", statement);
        ::bindText(locationCode, 4, "location_code", "streams", statement);

        returnCode = sqlite3_step(statement);
        if (returnCode == SQLITE_ROW)
        {   
            auto iExists = sqlite3_column_int(statement, 0);
            if (iExists >= 1){exists = 1;}
            sqlite3_finalize(statement);
        }
        else
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Query did not return row");
        }
        return exists;
    }

    [[nodiscard]] int getStreamIdentifier(const Stream &stream)
    {
        int streamIdentifier{-1};
        if (!streamExists(stream.network,
                          stream.station,
                          stream.channel,
                          stream.locationCode))
        {
            const std::string insertSQL{
R"""(
INSERT INTO streams(network, station, channel, location_code, latitude, longitude, elevation)
            VALUES(?, ?, ?, ?, ?, ?, ?) RETURNING identifier;
)"""};

            sqlite3_stmt *statement{nullptr};
            auto returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                                 insertSQL.c_str(),
                                                 -1,
                                                 &statement,
                                                 nullptr);
            if (returnCode != SQLITE_OK)
            {
                sqlite3_finalize(statement);
                throw std::runtime_error("Failed to prepare streams count statement");
            }
            auto locationCode = stream.locationCode;
            if (locationCode.empty()){locationCode = "--";}
            ::bindText(stream.network,      1, "network",       "streams", statement);
            ::bindText(stream.station,      2, "station",       "streams", statement);
            ::bindText(stream.channel,      3, "channel",       "streams", statement);
            ::bindText(stream.locationCode, 4, "location_code", "streams", statement);
            ::bindDouble(stream.latitude,   5, "latitude",      "streams", statement);
            ::bindDouble(stream.longitude,  6, "longitude",     "streams", statement);
            ::bindDouble(stream.elevation,  7, "elevation",     "streams", statement); 
            // Send it 
            returnCode = sqlite3_step(statement);
            if (returnCode == SQLITE_ROW)
            {
                streamIdentifier = sqlite3_column_int(statement, 0); 
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "Got stream identifier {} from db",
                                    std::to_string(streamIdentifier));
            }
            if (sqlite3_step(statement) != SQLITE_DONE)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                                "There exists more rows but terminating early");
            }
            sqlite3_finalize(statement);
            return streamIdentifier;
        }
        // Okay let's get that identifier                                 
        const std::string query{
R"""(
SELECT identifier FROM streams WHERE network = ? AND station = ? AND channel = ? AND location_code = ?;
)"""
        };
        sqlite3_stmt *statement{nullptr};
        auto returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                             query.c_str(),
                                             -1,
                                             &statement,
                                             nullptr);
        if (returnCode != SQLITE_OK)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to prepare streams count statement");
        }
        auto locationCode = stream.locationCode;
        if (locationCode.empty()){locationCode = "--";}
        ::bindText(stream.network,      1, "network",
                   "streams", statement);
        ::bindText(stream.station,      2, "station",
                   "streams", statement);
        ::bindText(stream.channel,      3, "channel",
                   "streams", statement);
        ::bindText(stream.locationCode, 4, "location_code",
                   "streams", statement);
        auto step = sqlite3_step(statement);
        if (step == SQLITE_ROW)
        {
            streamIdentifier = sqlite3_column_int(statement, 0);
            if (sqlite3_step(statement) != SQLITE_DONE)
            {   
                SPDLOG_LOGGER_WARN(mLogger,
                                "There exists more rows but terminating early");
            }
        }
        else
        {
            SPDLOG_LOGGER_ERROR(mLogger,
                                "Failed to get get stream identifier - rc {}",
                                step);
            streamIdentifier =-1;
        }
        sqlite3_finalize(statement);
        return streamIdentifier;
    }

    void addEvent(const EventRow &row)
    {
        if (eventExists(row.identifier))
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "{} already exists", row.identifier);
            return;
        }
        // Okay insert event
        const std::string insertSQL{
R"""(
INSERT INTO events(identifier, time, latitude, longitude, depth_km, magnitude_type, magnitude, event_type)
            VALUES(?, ?, ?, ?, ?, ?, ?, ?);
)"""};
        sqlite3_stmt *statement{nullptr};
        auto returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                             insertSQL.c_str(),
                                             -1,
                                             &statement,
                                             nullptr);
        if (returnCode != SQLITE_OK)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error(
                  "Failed to prepare insert event statement.  Failed with "
                + std::to_string (returnCode));
        }
        ::bindText(row.identifier,                1,
                   "identifier", "events", statement);
        ::bindDouble(row.eventTime.count()*1.e-6, 2,
                     "time",     "events", statement);
        ::bindDouble(row.latitude,                3,
                     "latitude", "events", statement);
        ::bindDouble(row.longitude,               4,
                     "longitude", "events", statement);
        ::bindDouble(row.depth,                   5,
                     "depth_km",     "events", statement);
        ::bindText(row.preferredMagnitudeType,    6,
                    "magnitude_type", "events", statement);
        ::bindDouble(row.preferredMagnitude,      7,
                     "magnitude", "events", statement);
        ::bindText(row.eventType,                 8,
                   "event_type",  "events", statement);
        returnCode = sqlite3_step(statement);
        if (returnCode != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error(
               "Failed to insert " + row.identifier + " into event table");
        }
        sqlite3_finalize(statement);
    }

    void addRow(const Row &row)
    {
        if (!eventExists(row.eventIdentifier))
        {
            SPDLOG_LOGGER_ERROR(mLogger, "{} does not exist", row.eventIdentifier);
            return;
        }
        auto streamIdentifier = getStreamIdentifier(row.stream);
SPDLOG_LOGGER_INFO(mLogger, "{}", streamIdentifier);
streamIdentifier = getStreamIdentifier(row.stream);
SPDLOG_LOGGER_INFO(mLogger, "{}", streamIdentifier);

/*
        const std::string insertSQL{
R"""(
INSERT INTO features(stream, true_pick_time, estimate_pick_time)  VALUES(?, ?, ?);
)"""
        };
        sqlite3_stmt *insertVersionStatement{nullptr};
        returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                        insertSQL.c_str(),
                                        -1,
                                        &insertVersionStatement,
                                        nullptr);
        ::bindRow(streamIdentifier, 1, "stream", "features", statement);
*/
    }


    void create()
    {
        const std::string eventSchema{
R"""(
CREATE TABLE events(identifier TEXT PRIMARY KEY,
                    time DOUBLE,
                    latitude DOUBLE,
                    longitude DOUBLE,
                    depth_km DOUBLE,
                    magnitude_type STRING,
                    magnitude REAL,
                    event_type TEXT CHECK(event_type IN ('earthquake', 'quarryBlast'))
                   );
)""" 
        };
        char *errorMessage{nullptr};
        auto returnCode = sqlite3_exec(mDatabaseHandle,
                                       eventSchema.c_str(),
                                       nullptr,
                                       nullptr, 
                                       &errorMessage);
        if (returnCode != SQLITE_OK)
        {
            auto message = std::string{errorMessage}; 
            sqlite3_free(errorMessage); 
            throw std::runtime_error("Failed to create event table because "
                                   + message);
        }

        const std::string versionSchema{
R"""(
CREATE TABLE version(number TEXT, tag TEXT);
)"""
        };
        returnCode = sqlite3_exec(mDatabaseHandle,
                                  versionSchema.c_str(),
                                  nullptr,
                                  nullptr, 
                                  &errorMessage);
        if (returnCode != SQLITE_OK)
        {   
            auto message = std::string{errorMessage}; 
            sqlite3_free(errorMessage); 
            throw std::runtime_error("Failed to create version table because "
                                   + message);
        }
        const std::string insertVersion{
R"""(
INSERT INTO version(number, tag) VALUES(?, ?);
)"""
        };
        sqlite3_stmt *insertVersionStatement{nullptr};
        returnCode = sqlite3_prepare_v2(mDatabaseHandle,
                                        insertVersion.c_str(),
                                        -1,
                                        &insertVersionStatement,
                                        nullptr);
        if (returnCode != SQLITE_OK)
        {    
            sqlite3_finalize(insertVersionStatement);
            throw std::runtime_error("Failed to prepare insert version statement");
        }
        ::bindText(UFilterPicker::Version::getVersion(), 1, "number",
                   "version", insertVersionStatement);
        ::bindText(UFilterPicker::Version::getTag(),     2, "tag",
                   "version", insertVersionStatement);
        returnCode = sqlite3_step(insertVersionStatement);
        if (returnCode != SQLITE_DONE)
        {
            sqlite3_finalize(insertVersionStatement);
            throw std::runtime_error(
               "Failed to insert number/tag into version table");
        }
        sqlite3_finalize(insertVersionStatement); 

        // Stream table
        const std::string streamSchema{
R"""(
CREATE TABLE streams(identifier INTEGER PRIMARY KEY AUTOINCREMENT,
                     network TEXT NOT NULL,
                     station TEXT NOT NULL,
                     channel TEXT NOT NULL,
                     location_code TEXT DEFAULT('--'),
                     latitude DOUBLE CHECK(latitude >= -90 AND latitude <= 90),
                     longitude DOUBLE,
                     elevation DOUBLE
                   );
)"""
        };
        returnCode = sqlite3_exec(mDatabaseHandle,
                                  streamSchema.c_str(),
                                  nullptr,
                                  nullptr, 
                                  &errorMessage);
        if (returnCode != SQLITE_OK)
        {
            auto message = std::string{errorMessage}; 
            sqlite3_free(errorMessage);
            throw std::runtime_error("Failed to create stream table because "
                                   + message);
        }

        // Features table
        const std::string featuresSchema{
R"""(
CREATE TABLE features(stream INT,
                      true_pick_time BIGINT,
                      estimate_pick_time BIGINT,
                      FOREIGN KEY(stream) REFERENCES stream(identifier)
                     );
)"""
        };
        returnCode = sqlite3_exec(mDatabaseHandle,
                                  featuresSchema.c_str(),
                                  nullptr,
                                  nullptr,
                                  &errorMessage);
        if (returnCode != SQLITE_OK)
        {
            auto message = std::string{errorMessage};
            sqlite3_free(errorMessage);
            throw std::runtime_error("Failed to create features table because "
                                   + message);
        }

        mTablesInitialized = true;
    }

    ~FeaturesDatabaseImpl()
    {
        close();
    }
private:
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    FeaturesDatabase::Mode mMode{FeaturesDatabase::Mode::ReadOnly};
    sqlite3 *mDatabaseHandle{nullptr};
    bool mTablesInitialized{false};
    bool mIsOpen{false}; 
};

/// Constructor
FeaturesDatabase::FeaturesDatabase(
    std::shared_ptr<spdlog::logger> logger,
    const std::filesystem::path &fileName,
    Mode mode) :
    pImpl(std::make_unique<FeaturesDatabaseImpl> (logger, fileName, mode))
{
}

/// Destructor
FeaturesDatabase::~FeaturesDatabase() = default;

/// Write the event row
void FeaturesDatabase::write(const EventRow &row)
{
    if (row.identifier.empty())
    {
        throw std::invalid_argument("Event identifier not set");
    }
    if (row.latitude <-90 || row.latitude > 90)
    {
        throw std::invalid_argument("Event latitude wrong");
    }
    if (row.preferredMagnitude >= 10)
    {
        throw std::invalid_argument("Event magnitude cannot exceed 10");
    }
    if (row.eventType.empty())
    {
        throw std::invalid_argument("Event type not defined");
    }
    if (row.preferredMagnitudeType.empty())
    {
        throw std::invalid_argument("Pref mag type not set");
    }
    pImpl->addEvent(row);
}

void FeaturesDatabase::write(const Row &row)
{
    if (row.eventIdentifier.empty())
    {
        throw std::invalid_argument("Event identifier not set");
    }
    if (row.stream.network.empty())
    {
        throw std::invalid_argument("Network not set");
    }
    if (row.stream.station.empty())
    {
        throw std::invalid_argument("Station not set");
    }   
    if (row.stream.channel.empty())
    {
        throw std::invalid_argument("Channel not set");
    }   
    pImpl->addRow(row);
}
