#ifndef UFILTER_PICKER_TRAINING_FEATURES_DATABASE_HPP
#define UFILTER_PICKER_TRAINING_FEATURES_DATABASE_HPP
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>
#include <spdlog/logger.h>
namespace UFilterPicker::Training
{

struct EventRow
{
    std::string identifier;
    std::string eventType;
    double latitude;
    double longitude;
    double depth;     // KM
    std::chrono::microseconds eventTime;
    std::string preferredMagnitudeType;
    double preferredMagnitude;
};

struct Stream
{
    std::string network;
    std::string channel;
    std::string station;
    std::string locationCode;
    double latitude;
    double longitude;
    double elevation;
};

struct Row
{
    Stream stream;
/*
    std::string network;
    std::string channel;
    std::string station;
    std::string locationCode;
*/
    std::string eventIdentifier;  // Key for event row
    double distance;              // Source receiver distance
    double backAzimuth;           // Receiver-to-source azimuth
    int analystQuality;           // 0 best - 4 worst
    std::chrono::microseconds truePickTime;
    std::chrono::microseconds estimatePickTime;
    double cfValueAtPick;
    double nominalSamplingRate;
    //std::vector<double> cfValues; 
    bool reviewed;
};
 
/// @class FeaturesDatabase
/// @brief A simple sqlite3 database to house features.
/// @copyright Ben Baker (University of Utah) distributed under the MIT license. 
class FeaturesDatabase
{
public:
    enum class Mode
    {
        Create,     /*!< Opens the database as read-write.  Moreover, 
                         if the database exists then it will be deleted
                         then recreated. */
        ReadWrite,  /*!< Opens the database in read-write mode. */
        ReadOnly    /*!< Opens the database in read-only mode. */
    };
public:
    /// @brief Opens (and if necessary) creates the database.
    FeaturesDatabase(std::shared_ptr<spdlog::logger> logger,
                     const std::filesystem::path &fileName,
                     Mode mode);

    void write(const EventRow &row);
    void write(const Row &row);

    ~FeaturesDatabase();
    FeaturesDatabase() = delete;
    FeaturesDatabase(const FeaturesDatabase &) = delete;
    FeaturesDatabase& operator=(const FeaturesDatabase &) = delete;
private:
    class FeaturesDatabaseImpl;
    std::unique_ptr<FeaturesDatabaseImpl> pImpl;
};
}
#endif
