#include <cstdint>
#include <chrono>
#include <memory>
#include <utility>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <vector>
#ifndef NDEBUG
#include <cassert>
#endif
#include "uFilterPicker/pipeline.hpp"
#include "uFilterPicker/narrowBandFilter.hpp"
#include "uFilterPicker/envelope.hpp"
#include "uFilterPicker/characteristicFunction.hpp"

using namespace UFilterPicker;

namespace
{
struct StartUpParameters
{
    double df; // Sampling rate
    double lowCorner; // Low corner
    int envelopeOrder; // Order of envelope
    int cfOrder;  // Order of characteristic function
};
std::chrono::microseconds
    estimateStartUp(const StartUpParameters &parms)
                    //const double df,
                    //const double lowCorner,
                    //const int envelopeOrder,
                    //const int cfOrder)
{
    auto filterStartUpS = 5./parms.lowCorner; // Give it a few cycles to clear out
    auto dtMicroSeconds = static_cast<int64_t> (std::round(1000000/parms.df));
    auto envelopeStartUp = parms.envelopeOrder*dtMicroSeconds; 
    auto cfStartUp = parms.cfOrder*dtMicroSeconds;
    const std::chrono::microseconds sosStartUp
    {
        static_cast<int64_t> (std::round(filterStartUpS*1.e6))
    };
    const std::chrono::microseconds firStartUp{cfStartUp + envelopeStartUp};
    return std::max(sosStartUp, firStartUp);
}
}

class Pipeline::PipelineImpl
{
public:

    std::unique_ptr<NarrowBandFilter> mFilter{nullptr};
    std::unique_ptr<Envelope> mEnvelope{nullptr};
    std::unique_ptr<CharacteristicFunction> mCharacteristicFunction{nullptr};
    std::chrono::microseconds mStartUp{0};
    std::chrono::microseconds mGroupDelay{0};
    double mNominalSamplingRate{100};
    bool mInitialized{false};
};

Pipeline::Pipeline(
    std::unique_ptr<NarrowBandFilter> &&narrowBandFilter,
    std::unique_ptr<Envelope> &&envelope,
    std::unique_ptr<CharacteristicFunction> &&characteristicFunction) :
    pImpl(std::make_unique<PipelineImpl> ())
{
    if (!narrowBandFilter->isInitialized())
    {
        throw std::invalid_argument("Narrow band filter not initialized");
    }
    if (!envelope->isInitialized())
    {
        throw std::invalid_argument("Envelope is not initialized");
    }
    if (!characteristicFunction->isInitialized())
    {
        throw std::invalid_argument("Characteristic function not initialized");
    }
    pImpl->mFilter = std::move(narrowBandFilter);
    pImpl->mEnvelope = std::move(envelope);
    pImpl->mCharacteristicFunction = std::move(characteristicFunction);
    pImpl->mNominalSamplingRate = pImpl->mFilter->getNominalSamplingRate();
    auto lowCorner = pImpl->mFilter->getPassBand().first;
    auto envelopeOrder = pImpl->mEnvelope->getOrder();
#ifndef NDEBUG
    assert(envelopeOrder%2 == 0); // Order forced to be even so odd number of coeffs
#endif
    auto cfOrder = pImpl->mCharacteristicFunction->getWindowLength() - 1;
    const StartUpParameters startUpParms{pImpl->mNominalSamplingRate,
                                         lowCorner,
                                         envelopeOrder,
                                         cfOrder};
    pImpl->mStartUp = ::estimateStartUp(startUpParms);
                                        //pImpl->mNominalSamplingRate,
                                        //lowCorner,
                                        //envelopeOrder,
                                        //cfOrder);
    // The group delay:
    // (1) The IIR filter burns a few samples to get going.  Each cascade
    //     has three coefficients.  But one sample is the `current' sample.
    //     So the delay line is 2*nCascades.
    // (2) The envelope's group delay is it's length/2 = (order + 1)/2.
    //     For example, if there's 5 coefficients, then the order is 4.
    //     Moreover, assumed the coefficients are {0, 0, 1, 0, 0}
    //     Then a filtered signal looks like:
    //     y[n] = x[n] b[0] + x[n-1] b[1] + x[n-2] b[2] + x[n-3] b[3]  + x[n-4] b[4]
    //          = x[n-2] b[2].
    //     i.e., a delay of 2 samples.  With a sampling period of T we then have
    //     the delay as  T*(order + 1)/2 which simplifies for to T*(order/2) 
    //     for even orders.
    // (3) The characteristic function looks backwards so it doesn't introduce
    //     a delay.
    auto nCascades = pImpl->mFilter->getNumberOfCascades();
    const std::chrono::microseconds approximateFilterGroupDelay
    {
        static_cast<int64_t>
            (std::round(1000000*(2*nCascades)/pImpl->mNominalSamplingRate))
    };
    // Filter is symmetric so the group delay is (N - 1)/2*T = (Order)/2/df
    const double envelopeGroupDelayS
        = (0.5*envelopeOrder)/pImpl->mNominalSamplingRate;
    const std::chrono::microseconds envelopeGroupDelay
    {
        static_cast<int64_t>
            (std::round(1000000*envelopeGroupDelayS)) // to microseconds
    };
    pImpl->mGroupDelay = approximateFilterGroupDelay + envelopeGroupDelay;
    
    //std::cout << pImpl->mStartUp.count()*1.e-6 << std::endl;
    pImpl->mInitialized = true;
}

/// Initialized?
bool Pipeline::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Apply
std::vector<double> Pipeline::apply(const std::vector<double> &x) 
{
    if (!isInitialized())
    {
        throw std::runtime_error("Pipeline not initialized");
    }
    auto xCopy = x;
    return apply(std::move(xCopy));
}

std::vector<double> Pipeline::apply(std::vector<double> &&x)
{
    if (!isInitialized())
    {
        throw std::runtime_error("Pipeline not initialized");
    }
    std::vector<double> result;
    if (x.empty()){return result;}
    result = pImpl->mFilter->apply(std::move(x));
    result = pImpl->mEnvelope->apply(std::move(result));
    result = pImpl->mCharacteristicFunction->apply(result);
    return result;
}

std::chrono::microseconds Pipeline::getStartUpDuration() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Pipeline not initialized");
    }   
    return pImpl->mStartUp;
}

std::chrono::microseconds Pipeline::getGroupDelay() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Pipeline not initialized");
    }   
    return pImpl->mGroupDelay;
}

std::vector<double> Pipeline::getStageOutput(
    const Pipeline::ProcessingStage stage) const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Pipeline not initialized");
    }
    if (stage == Pipeline::ProcessingStage::Filter)
    {
        return pImpl->mFilter->getOutput();
    }
    else if (stage == Pipeline::ProcessingStage::Envelope)
    {
        return pImpl->mEnvelope->getOutput();
    }
    else if (stage == Pipeline::ProcessingStage::CharacteristicFunction)
    {
        return pImpl->mCharacteristicFunction->getOutput();
    }
    throw std::runtime_error("Unhandled processing stage");
} 

/// Nominal sampling rate
double Pipeline::getNominalSamplingRate() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Pipeline not initialized");
    }   
    return pImpl->mNominalSamplingRate;
}

/// Reset
void Pipeline::resetInitialConditions()
{
    if (!isInitialized())
    {
        throw std::runtime_error("Pipeline not initialized");
    }
    pImpl->mFilter->resetInitialConditions();
    pImpl->mEnvelope->resetInitialConditions();
    pImpl->mCharacteristicFunction->resetInitialConditions();
}

/// Destructor
Pipeline::~Pipeline() = default;

