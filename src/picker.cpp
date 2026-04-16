#include <bit>
#include <memory>
#include <chrono>
#include <utility>
#include <string>
#include <stdexcept>
#include <exception>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include <spdlog/logger.h>
#include "uFilterPicker/picker.hpp"
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/thresholdTrigger.hpp"
#include "uFilterPicker/utilities.hpp"
#include "uFilterPicker/metrics.hpp"

using namespace UFilterPicker;

class Picker::PickerImpl
{
public:
    PickerImpl(const UDataPacketServiceAPI::V1::StreamIdentifier &identifier,
               std::unique_ptr<Detector> &&detector,
               std::unique_ptr<ThresholdTrigger> &&trigger,
               std::shared_ptr<spdlog::logger> logger,
               const double nominalSamplingRate) :
        PickerImpl(UFilterPicker::Utilities::toString(identifier),
                   std::move(detector),
                   std::move(trigger),
                   std::move(logger),
                   nominalSamplingRate)
    {
    }

    PickerImpl(const std::string &identifierString,
               std::unique_ptr<Detector> &&detector,
               std::unique_ptr<ThresholdTrigger> &&trigger,
               std::shared_ptr<spdlog::logger> logger,
               const double nominalSamplingRate) :
        mIdentifierString(identifierString),
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

        mFilterGroupDelay = mDetector->getGroupDelay();
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
        // Apply the detector
        const auto startTime
            = Utilities::getStartTime<std::chrono::microseconds> (packet);
        if (startTime > now)
        {
            throw std::invalid_argument("Will not process future packets");
        }
        const auto endTime
            = Utilities::getEndTime<std::chrono::microseconds> (packet);
        if (endTime < now - mMaxLatency)
        {
            SPDLOG_LOGGER_WARN(mLogger, "Data is too latent - skipping");
            return;
        }

        if (mFirstPacket)
        {
            auto characteristicFunction = mDetector->apply(samples);
            mLastSamplingRate = samplingRate;
            mLastSampleTime = endTime;
            mFirstPacket = false;
        }
        else
        {
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
            // Probably a clock error
            if (gapSize > mGapToleranceInSamples)
            {
                SPDLOG_LOGGER_WARN(mLogger, "Gap detected - resetting");
                mDetector->resetInitialConditions();
                mTrigger->resetInitialConditions();
                mLastSamplingRate = samplingRate;
                mLastSampleTime = endTime;
                return;
            } 
            // Standard
            mLastSamplingRate = samplingRate;
            mLastSampleTime = endTime;
        }
    }

    std::string mIdentifierString;
    std::unique_ptr<Detector> mDetector;
    std::unique_ptr<ThresholdTrigger> mTrigger;
    std::shared_ptr<spdlog::logger> mLogger;
    UFilterPicker::Metrics::MetricsSingleton &mMetrics
    {   
        UFilterPicker::Metrics::MetricsSingleton::getInstance()
    };  
    std::chrono::microseconds mFilterGroupDelay;
    std::chrono::microseconds mLastSampleTime{0}; 
    std::chrono::microseconds mMaxLatency{std::chrono::minutes {5}};
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
    const UDataPacketServiceAPI::V1::StreamIdentifier &streamIdentifier,
    std::unique_ptr<UFilterPicker::Detector> &&detector,
    std::unique_ptr<UFilterPicker::ThresholdTrigger> &&trigger,
    std::shared_ptr<spdlog::logger> logger,
    double nominalSamplingRate
) :
    pImpl(std::make_unique<PickerImpl> (streamIdentifier,
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

