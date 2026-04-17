#include <bit>
#include <memory>
#include <chrono>
#include <utility>
#include <string>
#include <vector>
#include <stdexcept>
#include <exception>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <spdlog/logger.h>
#include "uFilterPicker/picker.hpp"
#include "uFilterPicker/pickerOptions.hpp"
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/thresholdTrigger.hpp"
#include "uFilterPicker/utilities.hpp"
#include "uFilterPicker/metrics.hpp"

using namespace UFilterPicker;

class Picker::PickerImpl
{
public:
/*
    PickerImpl(const PickerOptions &options,
               const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
               std::unique_ptr<Detector> &&detector,
               std::unique_ptr<ThresholdTrigger> &&trigger,
               std::shared_ptr<spdlog::logger> logger,
               const double nominalSamplingRate) :
        PickerImpl(options,
                   UFilterPicker::Utilities::toString(identifier),
                   std::move(detector),
                   std::move(trigger),
                   std::move(logger),
                   nominalSamplingRate)
    {
    }
*/

    PickerImpl(const PickerOptions &options,
               const UDataPacketServiceAPI::V1::StreamIdentifier &identifier, //std::string &identifierString,
               std::unique_ptr<Detector> &&detector,
               std::unique_ptr<ThresholdTrigger> &&trigger,
               std::shared_ptr<spdlog::logger> logger,
               const double nominalSamplingRate) :
        mOptions(options),
        mIdentifier(identifier),
        mIdentifierString(UFilterPicker::Utilities::toString(identifier)),
        mDetector(std::move(detector)),
        mTrigger(std::move(trigger)),
        mLogger(std::move(logger)),
        mNominalSamplingRate(nominalSamplingRate)
    {
        if (mDetector == nullptr)
        {
            throw std::invalid_argument("Detector is null");
        }
        if (mTrigger == nullptr)
        {
            throw std::invalid_argument("Trigger is null");
        }
        if (!mDetector->isInitialized())
        {
            throw std::invalid_argument("Detector is not initialized");
        }
        if (!mTrigger->isInitialized())
        {
            throw std::invalid_argument("Trigger is not initialized");
        }
        if (mIdentifierString.empty())
        {
            throw std::runtime_error("Identifier string is empty");
        }
        if (mNominalSamplingRate <= 0)
        {
            throw std::invalid_argument(
               "Nominal sampling rate must be positive");
        }
        if (mLogger == nullptr)
        {
            //NOLINTBEGIN(misc-include-cleaner)
            mLogger = spdlog::stdout_color_mt(mIdentifierString
                                            + "-picker-console");
            //NOLINTEND(misc-include-cleaner)
        }
        mMetricsKeyName = UFilterPicker::Metrics::toKeyName(mIdentifier);
        if (mMetricsKeyName.empty())
        {
            throw std::runtime_error("Unable to generate key name");
        }
        mMaxLatency = mOptions.getMaximumLatency();
        mMaxFutureTime = mOptions.getMaximumFutureTime();
        mGapToleranceInSamples = mOptions.getGapTolerance();
        mFilterGroupDelay = mDetector->getGroupDelay();
        mBurnInTime = mFilterGroupDelay*mOptions.getBurnInFactor();
        mInitialized = true;
        SPDLOG_LOGGER_INFO(mLogger,
                           "Made detector for {}",
                           mIdentifierString);
    }

    void apply(const UDataPacketServiceAPI::V1::Packet &packet)
    {
        const auto now
            = UFilterPicker::Utilities::getNow<std::chrono::microseconds> ();
        // Verify we have the right stream
        const auto thisIdentifierString = Utilities::toString(packet); // Throws
        if (thisIdentifierString != mIdentifierString)
        {   
            throw std::invalid_argument("Incorrect stream identifier - got "
                                      + thisIdentifierString
                                      + " but expecting " 
                                      + mIdentifierString);
        }   
        // Verify sampling rates didn't change on me
        if (!packet.has_sampling_rate())
        {   
            throw std::invalid_argument("Packet does not have sampling rate");
        }   
        const auto samplingRate = packet.sampling_rate();
        if (!Utilities::consistentSamplingRate(mNominalSamplingRate,
                                               samplingRate))
        {   
            throw std::invalid_argument("Sampling rates differ - got "
                                 + std::to_string(samplingRate)
                                 + " but expecting " 
                                 + std::to_string(mNominalSamplingRate));
        }   
        // Empty packet?
        const auto nSamples = packet.number_of_samples();
        if (nSamples < 1)
        {   
            throw std::invalid_argument("No data for "
                                      + thisIdentifierString);
        }   
        // Data
        auto samples = Utilities::toDoubleVector(packet, mSwapBytes);
        if (samples.empty())
        {
            throw std::invalid_argument("Packet has samples but no data");
        }
        // Timing information
        const auto startTime
            = Utilities::getStartTime<std::chrono::microseconds> (packet);
        if (startTime < now - mMaxLatency)
        {
            SPDLOG_LOGGER_WARN(mLogger, "Data is too latent - skipping");
            return;
        }
        const auto endTime
            = Utilities::getEndTime<std::chrono::microseconds> (packet);
std::cout << std::setprecision(16) << mIdentifierString << " " << startTime.count()*1.e-6 << " " << endTime.count()*1.e-6 << std::endl;
        if (endTime > now + mMaxFutureTime)
        {
            throw std::invalid_argument("Will not process future packets");
        }

        // First packet is kinda easy
        if (mFirstPacket)
        {
            mFirstSampleTime = startTime;
            mFirstPacket = false;
        }
        else
        {
            if (startTime < mLastSampleTime)
            {
                SPDLOG_LOGGER_DEBUG(mLogger,
                                    "Expired packet detected for {}; skipping",
                                    mIdentifierString);
                return;
            }

            int gapSize{0};
            try
            {
                // Use sampling rate from previous packet to estimate when
                // it thinks the next sample should be 
                gapSize = Utilities::getGapSizeInSamples(startTime,
                                                         mLastSamplingRate,
                                                         mLastSampleTime);
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error("Failed to estimate gap because "
                                       + std::string {e.what()});
            } 
            // Gap - reset
            if (gapSize > mGapToleranceInSamples)
            {
                SPDLOG_LOGGER_WARN(mLogger,
                    "Gap of {} samples detected for {} (packet start time {}, last sample time {}); resetting",
                    gapSize,
                    mIdentifierString,
                    startTime.count(), mLastSampleTime.count());
                mMetrics.incrementDetectorResetsCounter(mMetricsKeyName);
                mDetector->resetInitialConditions();
                mTrigger->resetInitialConditions();
                mFirstSampleTime = startTime;
            } 
            else if (gapSize < 0)
            {
                // TODO should attempt to strip the packet data until we
                // reach the next valid sample start time
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Negative timing detected for {}- resetting",
                                   mIdentifierString);
                mMetrics.incrementDetectorResetsCounter(mMetricsKeyName);
                mDetector->resetInitialConditions();
                mTrigger->resetInitialConditions();
                mFirstSampleTime = startTime;
            }
        }
        // Update timings
        mLastSamplingRate = samplingRate;
        mLastSampleTime = endTime;

        // Apply the detector
        auto characteristicFunction = mDetector->apply(samples);

        // If the detector is burned in then apply the trigger
        bool runTrigger{false};
        auto shiftedPacketStartTime = startTime - mFilterGroupDelay;
        if (startTime - mFirstSampleTime > mBurnInTime)
        {
            runTrigger = true;
        }
        else
        {
/*
            if (endTime - mFirstSampleTime > mBurnInTime)
            {
                
                runTrigger = true;
            }
*/
        }
 
        if (runTrigger)
        {
            //if (endTime - mFirstSampleTime > mBurnInTime &&
            //    endTime - mFirstSampleTime > mFilterGroupDelay)
            auto picks = mTrigger->apply(characteristicFunction,
                                         shiftedPacketStartTime,
                                         samplingRate);
            if (!picks.empty())
            {
                auto nPicks = static_cast<int> (picks.size());
                mMetrics.incrementPicksCounter(mMetricsKeyName, nPicks);
            }
            //return std::optional<UDataPacketServiceAPI::V1::Packet> (result);
        }
    }
    PickerOptions mOptions;
    UDataPacketServiceAPI::V1::StreamIdentifier mIdentifier;
    std::string mIdentifierString;
    std::unique_ptr<Detector> mDetector;
    std::unique_ptr<ThresholdTrigger> mTrigger;
    std::shared_ptr<spdlog::logger> mLogger;
    UFilterPicker::Metrics::MetricsSingleton &mMetrics
    {   
        UFilterPicker::Metrics::MetricsSingleton::getInstance()
    };  
    std::string mMetricsKeyName;
    std::chrono::microseconds mFilterGroupDelay;
    std::chrono::microseconds mFirstSampleTime{0};
    std::chrono::microseconds mLastSampleTime{0}; 
    std::chrono::microseconds mMaxFutureTime{std::chrono::seconds {0}};
    std::chrono::microseconds mMaxLatency{std::chrono::minutes {5}};
    std::chrono::microseconds mBurnInTime{std::chrono::seconds {10}};
    double mNominalSamplingRate{0};
    double mLastSamplingRate{0};
    int mGapToleranceInSamples{0};
    bool mSwapBytes{std::endian::native == std::endian::little ? false : true};
    bool mFirstPacket{true};
    bool mInitialized{false};
};

/// Constructor
Picker::Picker
(
    const PickerOptions &options,
    const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier,
    std::unique_ptr<UFilterPicker::Detector> &&detector,
    std::unique_ptr<UFilterPicker::ThresholdTrigger> &&trigger,
    std::shared_ptr<spdlog::logger> logger,
    double nominalSamplingRate
) :
    pImpl(std::make_unique<PickerImpl> (options,
                                        streamIdentifier,
                                        std::move(detector),
                                        std::move(trigger),
                                        std::move(logger),
                                        nominalSamplingRate))
{
}

/// Destructor
Picker::~Picker() = default;

/// Initialized
bool Picker::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Stream identifier
std::string Picker::getIdentifierString() const
{
    if (!isInitialized()){throw std::runtime_error("Picker not initialized");}
    return pImpl->mIdentifierString;
}

void Picker::apply(const UDataPacketServiceAPI::V1::Packet &packet)
{
    if (!isInitialized())
    {
        throw std::runtime_error("Picker not initialized");
    }
    // Verify we have the right stream
    pImpl->apply(packet);
}

