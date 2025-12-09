#include <iostream>
#include <vector>
#include <cmath>
#include <uSignal/waterLevelTrigger.hpp>
#include <uSignal/vector.hpp>
#include "uFilterPicker/thresholdTrigger.hpp"

using namespace UFilterPicker;

class ThresholdTrigger::ThresholdTriggerImpl
{
public:
    explicit ThresholdTriggerImpl(
        const std::pair<double, double> &onAndOffThreshold) :
        mOnAndOffThreshold(onAndOffThreshold)
    {
        if (mOnAndOffThreshold.first <= 0)
        {
            throw std::invalid_argument(
                "onAndOffThreshold.first must be positive");
        }
        if (mOnAndOffThreshold.second <= 0)
        {
            throw std::invalid_argument(
                "onAndOffThreshold.second must be positive");
        }
        constexpr bool realTime{true};
        USignal::WaterLevelTriggerOptions options{mOnAndOffThreshold};
        mWaterLevelThreshold
            = std::make_unique<USignal::WaterLevelTrigger<double>>
              (options, realTime);
        mInitialized = true;
    }
    ThresholdTriggerImpl& operator=(ThresholdTriggerImpl &&impl) noexcept
    {
        if (&impl == this){return *this;}
        mOnAndOffThreshold = impl.mOnAndOffThreshold;
        mWaterLevelThreshold = std::move(mWaterLevelThreshold);
        mInitialized = impl.mInitialized;
        return *this;
    }   
    /*
    ThresholdTriggerImpl& operator=(const ThresholdTriggerImpl &impl)
    {
        if (&impl == this){return *this;}
        mOnAndOffThreshold = impl.mOnAndOffThreshold;
        if (impl.mWaterLevelThreshold != nullptr)
        {
            constexpr bool realTime{true};
            USignal::WaterLevelTriggerOptions options {mOnAndOffThreshold};
            mWaterLevelThreshold
                = std::make_unique<USignal::WaterLevelTrigger<double>> 
                  (options, realTime);
        }
        mInitialized = impl.mInitialized;
        return *this;
    }
    */
    std::unique_ptr<USignal::WaterLevelTrigger<double>>
       mWaterLevelThreshold{nullptr};
    std::pair<double, double> mOnAndOffThreshold;
    std::vector<uint8_t> mTriggerSignal;
    uint8_t mLastThresholdSample{0};
    bool mInitialized{false};
};

/// Constructor
ThresholdTrigger::ThresholdTrigger(
    const std::pair<double, double> &onAndOffThreshold) :
    pImpl(std::make_unique<ThresholdTriggerImpl> (onAndOffThreshold))
{
}

/// Move constructor
ThresholdTrigger::ThresholdTrigger(ThresholdTrigger &&trigger) noexcept
{
    *this = std::move(trigger);
}

/// Move assignment
ThresholdTrigger& 
ThresholdTrigger::operator=(ThresholdTrigger &&trigger) noexcept
{
    if (&trigger == this){return *this;}
    pImpl = std::move(trigger.pImpl);
    return *this;
}

/// Initialized?
bool ThresholdTrigger::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Reset initial conditions on gap
void ThresholdTrigger::resetInitialConditions()
{
    if (isInitialized())
    {
        throw std::runtime_error("Threshold detector not initialized");
    }
    pImpl->mWaterLevelThreshold->resetInitialConditions();
    pImpl->mTriggerSignal.clear();
    pImpl->mLastThresholdSample = 0;
}

/*
/// Apply the filter
std::vector<uint8_t> ThresholdTrigger::apply(const std::vector<double> &x)
{
    if (isInitialized())
    {   
        throw std::runtime_error("Threshold detector not initialized");
    }   
    std::vector<double> xCopy{x};
    return apply(std::move(xCopy));
}

std::vector<uint8_t> ThresholdTrigger::apply(std::vector<double> &&x)
{
    if (isInitialized())
    {   
        throw std::runtime_error("Threshold detector not initialized");
    }   
    std::vector<uint8_t> y;
    if (x.empty()){return y;}
    USignal::Vector<double> xIn(std::move(x));
    pImpl->mWaterLevelThreshold->setInput(std::move(xIn));
    pImpl->mWaterLevelThreshold->apply();
    const auto yRef = pImpl->mWaterLevelThreshold->getOutputReference();
    y.resize(yRef.size(), 0);
    for (int i = 0; i < static_cast<int> (y.size()); ++i)
    {
        y[i] = yRef[i] > 0 ? 1 : 0;
    }
    return y;
}
*/

/// Destructor
ThresholdTrigger::~ThresholdTrigger() = default;

std::vector<std::chrono::microseconds>
ThresholdTrigger::apply(const std::vector<double> &characteristicFunction,
                     const std::chrono::microseconds &packetStartTime,
                     const double packetSamplingRate)
{
    auto xCopy = characteristicFunction;
    return apply(std::move(xCopy), packetStartTime, packetSamplingRate);
}

std::vector<std::chrono::microseconds>
ThresholdTrigger::apply(std::vector<double> &&characteristicFunction,
    const std::chrono::microseconds &packetStartTime,
    const double packetSamplingRate)
{
    if (!isInitialized())
    {   
        throw std::invalid_argument("Threshold trigger not initialized");
    }
    if (packetSamplingRate <= 0)
    {
        throw std::invalid_argument("Packet sampling rate must be positive");
    }
    auto samplingPeriodMuS
        = std::chrono::microseconds
          {
             static_cast<int64_t> (std::round(1000000/packetSamplingRate) )
          };
    USignal::Vector<double> xIn(std::move(characteristicFunction));
    pImpl->mWaterLevelThreshold->setInput(std::move(xIn));
    pImpl->mWaterLevelThreshold->apply();
    const auto yRef = pImpl->mWaterLevelThreshold->getOutputReference();
    std::vector<std::chrono::microseconds> picks; 
    if (yRef.empty()){return picks;}
    // Compute picks
    std::vector<uint8_t> y(yRef.size(), 0);
    auto lastThresholdSample = pImpl->mLastThresholdSample;
    picks.reserve(4);
    for (int i = 0; i < static_cast<int> (y.size()); ++i)
    {
        y[i] = yRef[i] > 0 ? 1 : 0;
        if (y[i] == 1 && lastThresholdSample == 0)
        {
            picks.push_back(packetStartTime + i*samplingPeriodMuS);
        }
        lastThresholdSample = y[i];
    }
    pImpl->mTriggerSignal = std::move(y);
    return picks;
}

std::vector<uint8_t> ThresholdTrigger::getOutput() const
{
    if (!isInitialized())
    {
        throw std::invalid_argument("Threshold trigger not initialized");
    }
    return pImpl->mTriggerSignal;
}
