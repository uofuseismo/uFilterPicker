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

struct Row
{
    std::string network;
    std::string channel;
    std::string station;
    std::string locationCode;
    std::string eventIdentifier;
    double distance;              // Source receiver distance
    double backAzimuth;           // receiver-to-source azimuth
    std::vector<double> cfValues;
    std::chrono::microseconds estimatePickTime;
    std::chrono::microseconds truePickTime;
    double cfValueAtPick;
    double nominalSamplingRate;
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
