#include <atomic>
#include <cctype>
#include <string>
#include <cstdint>
#include <utility>
#include <mutex>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include "uFilterPicker/metrics.hpp"

using namespace UFilterPicker::Metrics;

[[nodiscard]]
std::string UFilterPicker::Metrics::toKeyName(
     const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
     const auto &network = identifier.network();
     if (network.empty()){throw std::runtime_error("Network is empty");}
     const auto &station = identifier.station();
     if (station.empty()){throw std::runtime_error("Station is empty");}
     const auto &channel = identifier.channel();
     if (channel.empty()){throw std::runtime_error("Channel is empty");}
     const auto &locationCode = identifier.location_code();

     auto result = network + "_"
                 + station + "_"
                 + channel;
     if (!locationCode.empty()){result = result + "_" + locationCode;}
     std::transform(result.begin(), result.end(), result.begin(), ::tolower);
     return result;
}


[[nodiscard]]
std::string 
UFilterPicker::Metrics::toKeyName(const UDataPacketServiceAPI::V1::Packet &packet)
{
     return toKeyName(packet.stream_identifier());
}

///--------------------------------------------------------------------------///

MetricsSingleton &MetricsSingleton::getInstance()
{   
    std::mutex mutex;
    const std::scoped_lock lock{mutex};
    static MetricsSingleton instance;
    return instance;
}   

void MetricsSingleton::incrementDetectorResetsCounter(const std::string &key)
{
    const std::lock_guard<std::mutex> lock(mMutex);
    auto idx = mResetsCounterMap.find(key);
    if (idx == mResetsCounterMap.end())
    {
        mResetsCounterMap.insert( std::pair {key, 1} );
    }
    else
    {
        idx->second = idx->second + 1;
    }
}

std::map<std::string, int64_t> 
MetricsSingleton::getDetectorResetsCounters() const noexcept
{
    const std::lock_guard<std::mutex> lock(mMutex);
    return mResetsCounterMap;
}

void MetricsSingleton::incrementPicksCounter(
    const std::string &key, const int nPicks)
{
    if (nPicks < 0)
    {
        throw std::invalid_argument("Must increment picks by positive amount");
    }
    if (nPicks == 0){return;}
    const std::lock_guard<std::mutex> lock(mMutex);
    auto idx = mPicksCounterMap.find(key);
    if (idx == mPicksCounterMap.end())
    {
        mPicksCounterMap.insert( std::pair {key, nPicks} );
    }
    else
    {
        idx->second = idx->second + nPicks;
    }
}

std::map<std::string, int64_t> 
MetricsSingleton::getPicksCounters() const noexcept
{
    const std::lock_guard<std::mutex> lock(mMutex);
    return mPicksCounterMap;
}

void UFilterPicker::Metrics::initializeSingleton()
{
    MetricsSingleton::getInstance();
}

void MetricsSingleton::incrementPicksSentCounter()
{
    mPicksSentCounter.fetch_add(1, std::memory_order_relaxed);
}

int64_t MetricsSingleton::getPicksSentCount() const noexcept
{
    return mPicksSentCounter.load(std::memory_order_relaxed);
}

int64_t MetricsSingleton::sumDetectorResets() const noexcept
{
    int64_t result{0};
    {
    const std::lock_guard<std::mutex> lock(mMutex);
    for (const auto &item : mResetsCounterMap)
    {
        result = result + item.second;
    }
    }
    return result;
}

int64_t MetricsSingleton::sumPicks() const noexcept
{
    int64_t result{0};
    {
    const std::lock_guard<std::mutex> lock(mMutex);
    for (const auto &item : mPicksCounterMap)
    {
        result = result + item.second;
    }
    }
    return result;
}
