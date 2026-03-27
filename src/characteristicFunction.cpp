#include <vector>
#include <utility>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <cmath>
#ifndef NDEBUG
#include <cassert>
#endif
#include <uSignal/filterRepresentations/finiteImpulseResponse.hpp>
#include <uSignal/filterImplementations/finiteImpulseResponse.hpp>
#include <uSignal/vector.hpp>
#include "uFilterPicker/characteristicFunction.hpp"

using namespace UFilterPicker;

class CharacteristicFunction::CharacteristicFunctionImpl
{
public:
    CharacteristicFunctionImpl(const int windowSize)
    {
        if (windowSize <= 0)
        {
            throw std::invalid_argument("windowSize = "
                                      + std::to_string(windowSize)
                                      + " must be positive");
        }
        const double average{1./windowSize};
        USignal::Vector<double> movingAverage(windowSize, average);
        // We compare the current sample with the running statistics
        // up to, but not including, the current sample.  This zero
        // effectively creates a lag in the FIR filter by zero'ing
        // out the contribution of the current sample to the running
        // averages.
        movingAverage.push_back(0); // Don't want to compare current sample
        const USignal::FilterRepresentations::FiniteImpulseResponse<double>
            firFilter(movingAverage);
        constexpr bool isRealTime{true};
        constexpr auto implementation
        {
            USignal::FilterImplementations::FiniteImpulseResponse<double>::
               Implementation::Direct
        };
        mAverage
           = std::make_unique
             <
                 USignal::FilterImplementations::FiniteImpulseResponse<double>
             > (firFilter, implementation, isRealTime);
        mAverageInputSquared
           = std::make_unique
             <
                 USignal::FilterImplementations::FiniteImpulseResponse<double>
             > (firFilter, implementation, isRealTime);
        mWindowSize = windowSize;
        // In our standard deviation computation we compute averages
        // (all factors of 1/n) but we really want 1/n-1 so to fix that
        // we multiply n/(n - 1) x 1/n to obtain 1/n-1.
        if (windowSize > 1)
        {
            mBesselCorrection
                = static_cast<double> (windowSize)
                 /static_cast<double> (windowSize - 1);
        }
        mInitialized = true;
    }
    void transform(std::vector<double> *y) const
    {
        const auto x = mAverage->getInputReference();
        std::vector<double> xIn(x.size());
        std::copy(x.begin(), x.end(), xIn.data());
        constexpr bool isApply{false};
        transform(xIn, y, isApply);
    } 
    void transform(const std::vector<double> &x, std::vector<double> *y,
                   const bool isApply) const
    {
        const auto yAverage = mAverage->getOutputReference();
        const auto yAverage2 = mAverageInputSquared->getOutputReference();
#ifndef NDEBUG
        assert(x.size() == yAverage.size());
        assert(yAverage.size() == yAverage2.size());
#endif
        constexpr double zero{0};
        y->resize(yAverage.size(), zero);
        if (yAverage.empty()){return;}
        auto yPtr = y->data();
        // Maybe we're still in the filter warmup
        int iStart{0};
        if (mExceededWindow < mWindowSize)
        {
            if (isApply)
            {
                mStartMask = 0;
                for (int i = 0; i < static_cast<int> (y->size()); ++i)
                {
                    mStartMask = i;
                    if (mExceededWindow < mWindowSize)
                    {
                        mExceededWindow = mExceededWindow + 1;
                    }
                    else
                    {
                        break;
                    }
                }
                iStart = mStartMask;
                //std::cout << iStart << " " << mExceededWindow << " " << mWindowSize << std::endl;
            }
        }
        else
        {
            // Could be trying to get old output
            if (!isApply)
            {
                iStart = mStartMask;
            }
            else
            {
                iStart = 0;
            }
        }
        // Compute Env(i) - E[Env(i)]/std(Env(i))
        // Note that, var(Env) = E[Env^2] - E[Env]^2
        for (int i = iStart; i < static_cast<int> (y->size()); ++i)
        {
            auto numerator = x[i] - yAverage[i];
            auto var = std::max(1.e-10,
                                yAverage2[i] - yAverage[i]*yAverage[i]);
            yPtr[i] = numerator/std::sqrt(mBesselCorrection*var);
        }
    }
    std::unique_ptr
    <
        USignal::FilterImplementations::FiniteImpulseResponse<double>
    > mAverage;
    std::unique_ptr
        <USignal::FilterImplementations::FiniteImpulseResponse<double>
    > mAverageInputSquared;
    double mBesselCorrection{1};
    int mWindowSize{0};
    mutable int mExceededWindow{0};
    mutable int mStartMask{0};
    bool mInitialized{false};
};

/// Constructor
CharacteristicFunction::CharacteristicFunction(const int windowSize) :
    pImpl(std::make_unique<CharacteristicFunctionImpl> (windowSize))
{
}

/// Initialized?
bool CharacteristicFunction::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Window length
int CharacteristicFunction::getWindowLength() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Characteristic function not initialized");
    }   
    return pImpl->mWindowSize;
}

/// Apply
std::vector<double> CharacteristicFunction::apply(const std::vector<double> &x)
{
    if (!isInitialized())
    {
        throw std::runtime_error("Characteristic function not initialized");
    }
    std::vector<double> y;
    if (x.empty()){return y;}
    // Update the average
    USignal::Vector<double> xIn(x);
    pImpl->mAverage->setInput(std::move(xIn));
    pImpl->mAverage->apply();
    // Update the input^2 average
    USignal::Vector<double> xIn2(x);
    std::transform(xIn2.begin(), xIn2.end(), xIn2.begin(),
                   [](const auto &xi){return xi*xi;});
    pImpl->mAverageInputSquared->setInput(std::move(xIn2));
    pImpl->mAverageInputSquared->apply();
    constexpr bool isApply{true};
    pImpl->transform(x, &y, isApply);
/*
    const auto yAverage = pImpl->mAverage->getOutputReference();
    const auto yAverage2 = pImpl->mAverageInputSquared->getOutputReference();
#ifndef NDEBUG
    assert(yAverage.size() == yAverage2.size());
#endif
    constexpr double zero{0};
    y.resize(yAverage.size(), zero);
    // Regardless, we are going to compute Env(i) - E[Env(i)]/std(Env(i))
    // Note that, var(Env) = E[Env^2] - E[Env]^2
    double besselCorrection = pImpl->mBesselCorrection;
    if (pImpl->mExceededWindow >= pImpl->mWindowSize)
    {
        for (int i = 0 ; i < static_cast<int> (y.size()); ++i)
        {
            auto numerator = x[i] - yAverage[i];
            auto var = std::max(1.e-10, yAverage2[i] - yAverage[i]*yAverage[i]);
            y[i] = numerator/std::sqrt(besselCorrection*var); 
        }
    }
    else
    {
        for (int i = 0 ; i < static_cast<int> (y.size()); ++i)
        {
            if (pImpl->mExceededWindow < pImpl->mWindowSize)
            {
                y[i] = 0;
                pImpl->mExceededWindow = pImpl->mExceededWindow + 1;
            }
            else
            {
                auto numerator = x[i] - yAverage[i];
                auto var = std::max(1.e-10, yAverage2[i] - yAverage[i]*yAverage[i]);
                y[i] = numerator/std::sqrt(besselCorrection*var);
            }
        }
    }
*/
    return y;
}

/// Gets the output
std::vector<double> CharacteristicFunction::getOutput() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error(
           "CharacteristicFunction filter not initialized");
    }   
    std::vector<double> y;
    pImpl->transform(&y);
    return y;
}

/// Reset the initial conditions
void CharacteristicFunction::resetInitialConditions()
{
    if (!isInitialized())
    {
        throw std::runtime_error(
           "CharacteristicFunction filter not initialized");
    }
    pImpl->mAverage->resetInitialConditions();
    pImpl->mAverageInputSquared->resetInitialConditions();
    pImpl->mExceededWindow = 0;
    pImpl->mStartMask = 0;
}

/// Destructor
CharacteristicFunction::~CharacteristicFunction() = default;


