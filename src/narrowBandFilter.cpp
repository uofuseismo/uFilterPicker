#include <stdexcept>
#include <string>
#include <cmath>
#include <vector>
#include <memory>
#include <utility>
#include <numbers>
#include <cmath>
#include <algorithm>
#include "uFilterPicker/narrowBandFilter.hpp"
#include <uSignal/vector.hpp>
#include <uSignal/filterRepresentations/secondOrderSections.hpp>
#include <uSignal/filterDesign/infiniteImpulseResponse/digital.hpp>
#include <uSignal/filterRepresentations/infiniteImpulseResponse.hpp>
//#include <uSignal/filterRepresentations/finiteImpulseResponse.hpp>
//#include <uSignal/filterImplementations/finiteImpulseResponse.hpp>
#include <uSignal/filterImplementations/secondOrderSections.hpp>
//#include <uSignal/filterDesign/finiteImpulseResponse/windowBased.hpp>
#include <uSignal/filterDesign/response.hpp>

#define REAL_TIME true

using namespace UFilterPicker;

/*
namespace
{
std::chrono::microseconds 
orderToGroupDelay(const int order, const double samplingRate)
{
    auto dt = 1./samplingRate;
    auto groupDelay = static_cast<int> (order/2);
    auto iGroupDelayMuS = static_cast<int64_t> (groupDelay*dt*1000000);
    return std::chrono::microseconds {iGroupDelayMuS};
}
}
*/

class NarrowBandFilter::NarrowBandFilterImpl
{
public:
    void design(const int order,
                const std::pair<double, double> &passBand,
                const double samplingRate)
    {
        mOrder = order;
        mLowCorner = passBand.first;
        mHighCorner = passBand.second;
        mNominalSamplingRate = samplingRate;
        mNyquistFrequency = mNominalSamplingRate/2;
        auto normalizedLowCorner = mLowCorner/mNyquistFrequency;
        auto normalizedHighCorner = mHighCorner/mNyquistFrequency;
        constexpr auto isRealTime{true};
        /* 
        namespace UFD
            = USignal::FilterDesign::FiniteImpulseResponse::WindowBased;
        constexpr auto window{UFD::Window::OptimalBlackman};
        auto normalizedLowCorner = mLowCorner/nyquistFrequency;
        auto normalizedHighCorner = mHighCorner/nyquistFrequency;
        auto firFilter
            = UFD::bandPass<double>
              (mOrder,
               std::pair {normalizedLowCorner, normalizedHighCorner},
               window);
        mFIRFilterDesign
            = std::make_unique
              <
                 USignal::FilterRepresentations::FiniteImpulseResponse<double>
              > (std::move(firFilter));
        constexpr auto isRealTime{true};
        constexpr auto implementation
        {
            USignal::FilterImplementations::FiniteImpulseResponse<double>::
               Implementation::Direct
        };
        mFIRFilter
            = std::make_unique
              <
                 USignal::FilterImplementations::FiniteImpulseResponse<double>
              > (*mFIRFilterDesign, implementation, isRealTime);
        mGroupDelay = ::orderToGroupDelay(mOrder, mNominalSamplingRate);
        mInitialized = true;
        */

        auto zpk
            = USignal::FilterDesign::InfiniteImpulseResponse::Digital::
              createButterworthBandpass(
                  order,
                  std::pair {normalizedLowCorner, normalizedHighCorner});
        mSOSFilterDesign
            = std::make_unique<USignal::FilterRepresentations::
              SecondOrderSections<double>>
                  (zpk,
                   USignal::FilterRepresentations::
                      SecondOrderSections<double>::PairingStrategy::Nearest);
        mSOSFilter
            = std::make_unique<USignal::FilterImplementations::
                               SecondOrderSections<double>>
              (*mSOSFilterDesign, isRealTime);
        //mGroupDelay = std::chrono::milliseconds {180};
        mInitialized = true;
    }
    [[nodiscard]] std::vector<double> computeResponse(
        const std::vector<double> &frequencies) const
    {
        constexpr double pi{std::numbers::pi_v<double>};
        USignal::Vector<double> normalizedFrequencies(frequencies.size());
        for (int i = 0; i < static_cast<int> (frequencies.size()); ++i)
        {
            normalizedFrequencies[i] = (frequencies[i]*pi)/mNyquistFrequency;
        }
        const USignal::FilterRepresentations::InfiniteImpulseResponse<double>
            ba{*mSOSFilterDesign};
        auto h
            = USignal::FilterDesign::Response::computeDigital(
                  ba, normalizedFrequencies);
        std::vector<double> magnitudeSpectra(h.size(), 0);;
        for (int i = 0; i < static_cast<int> (h.size()); ++i)
        {   
            magnitudeSpectra[i] = std::abs(h[i]);
        }   
        return magnitudeSpectra;
    }
    /*
    std::vector<double> computeResponse(const int nFrequencies = 500) const
    {
        
        // \pi is upper half plane of unit circle
        const auto df = (M_PI - 0)/static_cast<double> (nFrequencies - 1);
        //USignal::Vector<double> normalizedFrequencies(nFrequencies);
        std::vector<double> frequencies(nFrequencies);
        const double nyquistFrequency{mNominalSamplingRate/2};
        for (int i = 0; i < nFrequencies; ++i)
        {
            auto normalizedFrequency
                = static_cast<double> (0 + static_cast<double> (i)*df);
            frequencies[i] = (normalizedFrequency/M_PI)*nyquistFrequency;
        }
        return computeResponse(frequencies);
        USignal::FilterRepresentations::InfiniteImpulseResponse<double>
            ba{*mSOSFilterDesign};
        auto h
            = USignal::FilterDesign::Response::computeDigital(
                  ba, normalizedFrequencies);
        std::vector<double> magnitudeSpectra(h.size(), 0);;
        for (int i = 0; i < static_cast<int> (h.size()); ++i)
        {
            magnitudeSpectra[i] = std::abs(h[i]);
            std::cout << frequencies[i] << " " << magnitudeSpectra[i] << std::endl;
        }
        return magnitudeSpectra;
    }
    */
    NarrowBandFilterImpl() = default;
    NarrowBandFilterImpl(const NarrowBandFilterImpl &impl)
    {
        *this = impl;
    }
    NarrowBandFilterImpl(NarrowBandFilterImpl &&impl) noexcept
    {
        *this = std::move(impl);
    }
    NarrowBandFilterImpl& operator=(const NarrowBandFilterImpl &impl)
    {
        if (&impl == this){return *this;}
        if (impl.mSOSFilterDesign != nullptr && impl.mInitialized)
        {
            constexpr auto isRealTime{true};
            mSOSFilterDesign
                = std::make_unique< 
                  USignal::FilterRepresentations::SecondOrderSections<double>
                  > (*impl.mSOSFilterDesign);
            mSOSFilter
                = std::make_unique<
                  USignal::FilterImplementations::SecondOrderSections<double>
                  > (*mSOSFilterDesign, isRealTime);
        }
        /*
        if (impl.mFIRFilterDesign != nullptr && impl.mInitialized)
        {
            constexpr auto isRealTime{true};
            constexpr auto implementation
            {
                USignal::FilterImplementations::FiniteImpulseResponse<double>::
                   Implementation::Direct
            };
            mFIRFilterDesign
                = std::make_unique< 
                  USignal::FilterRepresentations::FiniteImpulseResponse<double>
                  > (*impl.mFIRFilterDesign);
            mFIRFilter
                = std::make_unique<
                  USignal::FilterImplementations::FiniteImpulseResponse<double>
                  > (*mFIRFilterDesign, implementation, isRealTime);
        }
        */
        mNominalSamplingRate = impl.mNominalSamplingRate;
        mLowCorner = impl.mLowCorner;
        mHighCorner = impl.mHighCorner;
        mOrder = impl.mOrder;
        //mGroupDelay = impl.mGroupDelay;
        mInitialized = impl.mInitialized;
        return *this;
    }
    NarrowBandFilterImpl& operator=(NarrowBandFilterImpl &&impl) noexcept
    {
        if (&impl == this){return *this;}
        mSOSFilterDesign = std::move(impl.mSOSFilterDesign);
        mSOSFilter = std::move(impl.mSOSFilter);
        //impl.mFIRFilterDesign = std::move(impl.mFIRFilterDesign);
        //impl.mFIRFilter = std::move(impl.mFIRFilter);
        mNominalSamplingRate = impl.mNominalSamplingRate;
        mLowCorner = impl.mLowCorner;
        mHighCorner = impl.mHighCorner;
        mOrder = impl.mOrder;
        //mGroupDelay = impl.mGroupDelay;
        mInitialized = impl.mInitialized;
        return *this;
    }
//private:
    std::unique_ptr
    <
        USignal::FilterRepresentations::SecondOrderSections<double>
    > mSOSFilterDesign{nullptr};
    std::unique_ptr
    <   
        USignal::FilterImplementations::SecondOrderSections<double>
    > mSOSFilter{nullptr};
    /*
    std::unique_ptr
    <
        USignal::FilterRepresentations::FiniteImpulseResponse<double>
    > mFIRFilterDesign{nullptr};
    std::unique_ptr
    <
        USignal::FilterImplementations::FiniteImpulseResponse<double>
    > mFIRFilter{nullptr};
    */
    double mNominalSamplingRate{100};
    double mLowCorner{4};
    double mHighCorner{10};
    double mNyquistFrequency{50};
    int mOrder{4};
    /*
    std::chrono::microseconds mGroupDelay
    {
        ::orderToGroupDelay(mOrder, mNominalSamplingRate)
    };
    */
    bool mInitialized{false};
};

NarrowBandFilter::NarrowBandFilter(
    const int order,
    const std::pair<double, double> &passBand,
    const double samplingRate) :
    pImpl(std::make_unique<NarrowBandFilterImpl> ())
{
    if (order <= 4)
    {
        throw std::invalid_argument("order = " 
           + std::to_string(order) + " must be at least 4");
    }
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("samplingRate = "
           + std::to_string(samplingRate) + " must be positive"); 
    }
    if (passBand.first <= 0)
    {
        throw std::invalid_argument("passBand.first must be positive");
    }
    if (passBand.second >= samplingRate/2)
    {
        throw std::invalid_argument(
            "passBand.second " + std::to_string(passBand.second)
          + " cannot exceed nyquist frequency "
          + std::to_string(samplingRate/2));
    } 
    if (passBand.second <= passBand.first)
    {
        throw std::invalid_argument(
           "passBand.first " + std::to_string(passBand.first)
         + " must be less than passBand.second "
         + std::to_string(passBand.second));
    }
    if (order%2 == 1)
    {
    //    spdlog::warn("Bumping order up by 1");
    } 
    int orderIn = order;
    if (orderIn%2 == 1){orderIn = orderIn + 1;}
    pImpl->design(orderIn, 
                  passBand,
                  samplingRate);
}

/// Destructor
NarrowBandFilter::~NarrowBandFilter() = default;

/// Compute response
std::pair<std::vector<double>, std::vector<double>>
NarrowBandFilter::computeResponse(
    const int nFrequencies) const
{
    if (nFrequencies < 2)
    {
        throw std::invalid_argument("At least two frequencies required");
    }
    const double lowFrequency{0};
    const double highFrequency{pImpl->mNyquistFrequency};
    const double df = (highFrequency- lowFrequency)/(nFrequencies - 1);
    std::vector<double> frequencies(nFrequencies);
    for (int i = 0; i < nFrequencies; ++i)
    {
        frequencies[i] = std::min(std::max(0.0, i*df),
                                  pImpl->mNyquistFrequency);
    } 
    auto response = computeResponse(frequencies);
    return std::pair {frequencies, response};
}

std::vector<double> NarrowBandFilter::computeResponse(
    const std::vector<double> &frequencies) const
{
    std::vector<double> spectra;
    if (frequencies.empty())
    {
        return spectra;
    }
    if (frequencies.size() == 1)
    {
        if (frequencies.at(0) < 0)
        {
            throw std::invalid_argument("Frequency must be positive");
        }
        if (frequencies.at(0) > pImpl->mNyquistFrequency)
        {
            throw std::invalid_argument("Frequency cannot exceed Nyquist");
        }
    }
    auto [fMin, fMax]
        = std::minmax_element(frequencies.begin(), frequencies.end());
    if (*fMin < 0)
    {
        throw std::invalid_argument("Smallest frequency must be positive");
    }
    if (*fMax > pImpl->mNyquistFrequency)
    {
        throw std::invalid_argument("Highest frequency cannot exceed nyquist");
    }
    return pImpl->computeResponse(frequencies);
}

/// Initialized
bool NarrowBandFilter::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Filter passband
std::pair<double, double> NarrowBandFilter::getPassBand() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Filter not initialized");
    }
    return std::pair {pImpl->mLowCorner, pImpl->mHighCorner};
}

/// Gets the filter order
int NarrowBandFilter::getOrder() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Filter not initialized");
    }   
    return pImpl->mOrder;
}

/// Gets the number of cascades
int NarrowBandFilter::getNumberOfCascades() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Filter not initialized");
    }
    return pImpl->mSOSFilterDesign->getNumberOfSections();
}

/// Sampling rate
double NarrowBandFilter::getNominalSamplingRate() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Filter not initialized");
    }   
    return pImpl->mNominalSamplingRate;
}


/*
/// Gets the group delay
std::chrono::microseconds NarrowBandFilter::getGroupDelay() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Filter not initialized");
    }   
    return pImpl->mGroupDelay;
}
*/

/// Copy assignment
NarrowBandFilter& NarrowBandFilter::operator=(const NarrowBandFilter &filter)
{
    if (&filter == this){return *this;}
    pImpl = std::make_unique<NarrowBandFilterImpl> (*filter.pImpl);
    return *this;
}

/// Copy assignment
NarrowBandFilter& 
NarrowBandFilter::operator=(NarrowBandFilter &&filter) noexcept
{
    if (&filter == this){return *this;}
    pImpl = std::move(filter.pImpl);
    return *this;
}

/// Apply the filter
std::vector<double> NarrowBandFilter::apply(const std::vector<double> &x)
{
    if (!isInitialized())
    {
        throw std::runtime_error("Narrow band filter not initialized");
    }   
    std::vector<double> xCopy{x};
    return apply(std::move(xCopy));
}

std::vector<double> NarrowBandFilter::apply(std::vector<double> &&x)
{
    if (!isInitialized())
    {
        throw std::runtime_error("Narrow band filter not initialized");
    }
    std::vector<double> y;
    if (x.empty()){return y;}
    USignal::Vector<double> xIn(x); //std::move(x));

    pImpl->mSOSFilter->setInput(std::move(xIn));
    pImpl->mSOSFilter->apply();
    auto yOut = pImpl->mSOSFilter->getOutputReference();
/*
    pImpl->mFIRFilter->setInput(std::move(xIn));
    pImpl->mFIRFilter->apply();
    auto yOut = pImpl->mFIRFilter->getOutputReference();
*/
    if (!yOut.empty())
    {
        y.resize(yOut.size());
        const auto yOutPtr = yOut.data();
        auto yPtr = y.data();
        std::copy(yOutPtr, yOutPtr + yOut.size(), yPtr);
    }
    return y;
}

std::vector<double> NarrowBandFilter::getOutput() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Narrow band filter not initialized");
    }   
    std::vector<double> y;
    auto yOut = pImpl->mSOSFilter->getOutputReference();
    if (!yOut.empty())
    {   
        y.resize(yOut.size());
        const auto yOutPtr = yOut.data();
        auto yPtr = y.data();
        std::copy(yOutPtr, yOutPtr + yOut.size(), yPtr);
    }   
    return y; 
}

/// Reset filter
void NarrowBandFilter::resetInitialConditions()
{
    if (!isInitialized())
    {
        throw std::runtime_error("Narrow band filter not initialized");
    }
    pImpl->mSOSFilter->resetInitialConditions();
}
