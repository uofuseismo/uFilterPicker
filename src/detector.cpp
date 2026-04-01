//#include <iostream>
#include <cmath>
#include <vector>
#include <chrono>
#include <utility>
#include <algorithm>
#include <stdexcept>
#include <memory>
#ifndef NDEBUG
#include <cassert>
#endif
#include <spdlog/spdlog.h>
#include "uFilterPicker/detector.hpp"
#include "uFilterPicker/pipeline.hpp"
#include "uFilterPicker/envelope.hpp"
#include "uFilterPicker/narrowBandFilter.hpp"
#include "uFilterPicker/characteristicFunction.hpp"

using namespace UFilterPicker;

namespace
{

struct Lengths
{
    int envelope;
    int characteristicFunction;
};

std::unique_ptr<UFilterPicker::Pipeline>
    createPipeline(const int butterworthOrder,
                   const std::pair<double, double> &passband,
                   const Lengths &lengths, //const int envelopeLength,
                                           //const int characteristicFunctionLength,
                   const double samplingRate)
{
    auto narrowBandFilter
        = std::make_unique<UFilterPicker::NarrowBandFilter>
          (butterworthOrder, passband, samplingRate);
    constexpr double kaiserBeta{8}; // Unused
    const UFilterPicker::EnvelopeOptions
        envelopeOptions{lengths.envelope, kaiserBeta};
    auto envelope
        = std::make_unique<UFilterPicker::Envelope> (envelopeOptions); //Length);
    auto characteristicFunction
        = std::make_unique<UFilterPicker::CharacteristicFunction>
          (lengths.characteristicFunction);
    auto pipeline
        = std::make_unique<UFilterPicker::Pipeline> (std::move(narrowBandFilter),
                                                     std::move(envelope),
                                                     std::move(characteristicFunction)); 
    return pipeline;
} 

std::unique_ptr<UFilterPicker::Detector>
    createDetector(const int butterworthOrder,
                   const std::vector< std::pair<double, double> > &passbands,
                   const ::Lengths &lengths,
                   const double samplingRate)
{
    std::vector<std::unique_ptr<UFilterPicker::Pipeline>> pipelines;
    for (const auto &passband : passbands)
    {
        auto pipeline = ::createPipeline(butterworthOrder,
                                         passband,
                                         lengths,
                                         samplingRate);
        pipelines.push_back(std::move(pipeline));
    }
    auto detector
        = std::make_unique<UFilterPicker::Detector> (std::move(pipelines));
    return detector;
}

}

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
    auto startupDurationSeconds
        = static_cast<double> (pImpl->mStartUpDuration.count())*1.e-6;
    auto maskDurationSamples 
        = startupDurationSeconds*pImpl->mNominalSamplingRate; // s*1/s -> samples
    pImpl->mMaskFirstNSamples
        = std::max(0, static_cast<int> (std::ceil(maskDurationSamples)));
    pImpl->mGroupDelay = groupDelay;
    //std::cout << pImpl->mMaskFirstNSamples << std::endl;
    pImpl->mInitialized = true;
}

/// Apply
std::vector<double> Detector::apply(const std::vector<int> &xIn)
{
    std::vector<double> x;
    x.resize(xIn.size());
    std::copy(xIn.begin(), xIn.end(), x.begin());
    return apply(x);
}

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

std::unique_ptr<UFilterPicker::Detector>
    Detector::create100HzBroadband()
{
    constexpr int butterworthOrder{5};
    const std::vector< std::pair<double, double> > passBands
    {
                                        { 3 - 1,  8 + 3}, 
                                        { 8 - 3, 13 + 3}, 
                                        {13 - 3, 18 + 3}, 
                                        {18 - 3, 23 + 3}, 
                                        {23 - 3, 28 + 3}, 
                                        {28 - 3, 33 + 3}
    };
    constexpr int envelopeLength{400};
    constexpr int characteristicFunctionLength{400};
    const ::Lengths lengths{envelopeLength, characteristicFunctionLength};
    constexpr double samplingRate{100};
    auto detector = ::createDetector(butterworthOrder, //5,
                                     passBands,
                                     lengths,
                                     samplingRate);
     return detector;
}

std::unique_ptr<UFilterPicker::Detector>
    Detector::create40HzBroadband()
{
    constexpr int butterworthOrder{5};
    const std::vector< std::pair<double, double> > passBands
    {   
                                        { 3 - 1,  8 + 3}, 
                                        { 8 - 3, 13 + 3}, 
                                        {13 - 3, 18 + 3}, 
                                        {18 - 3, 23 + 3}, 
                                        {23 - 3, 28 + 3}//,
                                        //{28 - 3, 33 + 3}
    };  
    constexpr int envelopeLength{400};
    constexpr int characteristicFunctionLength{400};
    const ::Lengths lengths{envelopeLength, characteristicFunctionLength};
    constexpr double samplingRate{40};
    auto detector = ::createDetector(butterworthOrder, //5,
                                     passBands,
                                     lengths,
                                     samplingRate);
     return detector;
}

