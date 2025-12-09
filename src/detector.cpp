#include <iostream>
#include <thread>
#include <cmath>
#include <vector>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/pipeline.hpp"

using namespace UFilterPicker;

class Detector::DetectorImpl
{
public:
    std::vector<std::unique_ptr<Pipeline>> mPipelines;
    Detector::Strategy mStrategy{Detector::Strategy::Max};
    std::chrono::microseconds mStartUpDuration{0};
    std::chrono::microseconds mGroupDelay{0};
    double mNominalSamplingRate{100};
    int mMaskFirstNSamples{0};
    int mMaskCounter{0};
    bool mInitialized{false};
};

/// Constructor
Detector::Detector(std::vector<std::unique_ptr<Pipeline>> &&pipelines,
                   const Detector::Strategy strategy) :
    pImpl(std::make_unique<DetectorImpl> ())
{
    if (pipelines.empty())
    {
        throw std::invalid_argument("No pipelines");
    }
    std::chrono::microseconds groupDelay{0};
    double samplingRate{-1};
    for (const auto &p : pipelines)
    {
        if (!p->isInitialized())
        {
            throw std::invalid_argument("Pipeline not initialized");
        }
        auto df = p->getNominalSamplingRate();
        if (samplingRate < 0){samplingRate = df;}
        if (std::abs(samplingRate - df) > 1.e-8)
        {
            throw std::invalid_argument("Inconsistent sampling rates");
        }
        pImpl->mStartUpDuration
            = std::max(pImpl->mStartUpDuration, p->getStartUpDuration());
        groupDelay = std::max(groupDelay, p->getGroupDelay());
    }
    pImpl->mPipelines = std::move(pipelines);
    pImpl->mStrategy = strategy;
    pImpl->mNominalSamplingRate = samplingRate;
    auto maskDuration 
        = pImpl->mStartUpDuration.count()*1.e-6*pImpl->mNominalSamplingRate;
    pImpl->mMaskFirstNSamples
        = std::max(0, static_cast<int> (std::ceil(maskDuration)));
    pImpl->mGroupDelay = groupDelay;
    //std::cout << pImpl->mMaskFirstNSamples << std::endl;
    pImpl->mInitialized = true;
}

/// Apply
std::vector<double> Detector::apply(const std::vector<double> &x)
{
    std::vector<double> result;
    if (!isInitialized())
    {
        throw std::invalid_argument("Detector not initialized");
    }
    if (x.empty()){return result;}
    for (auto &pipeline : pImpl->mPipelines)
    {
        auto characteristicFunction = pipeline->apply(x);
        if (!characteristicFunction.empty())
        {
            if (result.empty())
            {
                result = characteristicFunction;
            }
            else
            {
                if (characteristicFunction.size() != result.size())
                {
                    spdlog::warn("Inconsistent cf/result size - using min");
                }
                const auto cfPtr = characteristicFunction.data();
                auto resultPtr = result.data();
                auto n
                    = static_cast<int>
                      (std::min(result.size(), characteristicFunction.size()));
                if (pImpl->mStrategy == Detector::Strategy::Max)
                {
                    for (int i = 0; i < n; ++i)
                    {
                        resultPtr[i] = std::max(resultPtr[i], cfPtr[i]);
                    }
                }
                else if (pImpl->mStrategy == Detector::Strategy::Average)
                {
                    for (int i = 0; i < n; ++i)
                    {
                        resultPtr[i] = resultPtr[i] + cfPtr[i];
                    }
                }
                else
                {
#ifndef NDEBUG
                    assert(false);
#else
                    throw std::runtime_error("Unhandled strategy");
#endif
                }
            }
        }
        // Pass the characteristic function through the threshold detector
    }
    // Normalize average
    if (pImpl->mStrategy == Detector::Strategy::Average)
    {
        auto nPipelines = static_cast<int> (pImpl->mPipelines.size());
        double factor{1./ std::max(1, nPipelines)};
        std::transform(result.begin(), result.end(), result.begin(),
                       [factor](const auto x)
                       {
                          return factor*x;
                       });
    }
    // May need to mask
    if (pImpl->mMaskCounter < pImpl->mMaskFirstNSamples)
    {
        for (int i = 0; i < static_cast<int> (result.size()); ++i)
        {
            result[i] = 0;
            pImpl->mMaskCounter = pImpl->mMaskCounter + 1;
            if (pImpl->mMaskCounter == pImpl->mMaskFirstNSamples){break;}
        }
    }
    return result;
}

/// Sampling rate
double Detector::getNominalSamplingRate() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Detector not initialized");
    }   
    return pImpl->mNominalSamplingRate;
}

/// Group delay
std::chrono::microseconds Detector::getGroupDelay() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Detector not initialized");
    }
    return pImpl->mGroupDelay;
}

/// Initialized?
bool Detector::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Reset initial conditions
void Detector::resetInitialConditions()
{
    if (!isInitialized())
    {
        throw std::runtime_error("Detector not initialized");
    }
    pImpl->mMaskCounter = 0;
    for (auto &pipeline : pImpl->mPipelines)
    {
        pipeline->resetInitialConditions();
    }
}

/// Destructor
Detector::~Detector() = default;
