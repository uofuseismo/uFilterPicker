#ifndef UFILTER_PICKER_METRICS_HPP
#define UFILTER_PICKER_METRICS_HPP
#include <string>
#include <map>
#include <mutex>
namespace UDataPacketServiceAPI::V1
{
 class Packet;
 class StreamIdentifier;
}
namespace UFilterPicker::Metrics
{

/// @class MetricsSingleton metrics.hpp
/// @brief Metrics singleton to be used in instrumented library
///        classes.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class MetricsSingleton
{
public:
    /// @result An instance of the singleton.  This is thread-safe.
    [[maybe_unused]] static MetricsSingleton &getInstance();
    

    /// @brief Increments the number of times a detector is reset.
    void incrementDetectorResetsCounter(const std::string &key);
    /// @brief Gets the current resets corresponding to each key.
    std::map<std::string, int64_t> getDetectorResetsCounters() const noexcept;

    /// @brief Incmrements the number of times a pick is made for the corresponding key.
    void incrementPicksCounter(const std::string &key, int nPicks = 1);
    /// @reuslt The number of picks for each key.
    std::map<std::string, int64_t> getPicksCounters() const noexcept;

    /// @brief Resets the counters and clears maps.  This is useful for unit tests.
    void resetCounters();
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    std::map<std::string, int64_t> mResetsCounterMap;
    std::map<std::string, int64_t> mPicksCounterMap;
    mutable std::mutex mMutex;
};

/// @brief Call this once at the start of your main routine to instantiate
///        the singleton.
void initializeSingleton();
/// @brief Converts a stream identifier to a key name.
[[nodiscard]]
std::string toKeyName(
     const UDataPacketServiceAPI::V1::StreamIdentifier &identifier);
/// @brief Extracts the stream identifier from the packet and
///        returns the corresponding key name.
[[nodiscard]]
std::string toKeyName(
     const UDataPacketServiceAPI::V1::Packet &packet);

}
#endif
