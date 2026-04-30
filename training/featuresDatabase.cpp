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
#include "featuresDatabase.hpp"

using namespace UFilterPicker::Training;

namespace
{

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
SELECT COUNT(*) FROM event WHERE identifier = ?;
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
        ::bindText(eventIdentifier, 1, "identifier", "event", statement);
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

    void insertEvent( )
    {

    }

    void create()
    {
        const std::string eventSchema{
R"""(
CREATE TABLE event(identifier TEXT PRIMARY KEY,
                   time DATETIME, 
                   latitude DOUBLE,
                   longitude DOUBLE,
                   depth DOUBLE,
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
            throw std::runtime_error("Failed to create phasehint table because "
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
