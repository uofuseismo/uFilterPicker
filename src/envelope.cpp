#include "uFilterPicker/envelope.hpp"
#include <uSignal/vector.hpp>
#include <uSignal/transforms/hilbert/finiteImpulseResponse.hpp>

using namespace UFilterPicker;

class Envelope::EnvelopeImpl
{
public:
    EnvelopeImpl(const int orderIn, const double beta)
    {
        USignal::Transforms::Hilbert::FiniteImpulseResponseOptions options;
        auto order = orderIn;
        if (order%2 == 1){order = order + 1;}
        options.setOrder(order);
        options.setBeta(beta);
        constexpr bool isRealTime{true};
        mTransformer
            = std::make_unique
              <
                 USignal::Transforms::Hilbert::FiniteImpulseResponse<double>
              > (options, isRealTime); 
        mOptions = options;
        mInitialized = true;
    }
    void transform(std::vector<double> *y) const
    {
        const auto yAnalytic = mTransformer->getOutputReference();
        if (yAnalytic.empty()){return;} 
        y->resize(yAnalytic.size());
        const auto yAnalyticPtr = yAnalytic.data();
        auto yPtr = y->data();
        for (int i = 0; i < static_cast<int> (y->size()); ++i)
        {   
            yPtr[i] = std::abs(yAnalyticPtr[i]);
        } 
    }
    EnvelopeImpl() = delete;
//private:
    USignal::Transforms::Hilbert::FiniteImpulseResponseOptions mOptions; 
    std::unique_ptr<
        USignal::Transforms::Hilbert::FiniteImpulseResponse<double>
    > mTransformer{nullptr};
    bool mInitialized{false};
};

/// Constructor
Envelope::Envelope(const int order, const double beta) :
    pImpl(std::make_unique<EnvelopeImpl> (order, beta))
{
}

/// Filter order
int Envelope::getOrder() const
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Envelope not initialized");
    }
    return pImpl->mOptions.getOrder();
}

/// Initialized?
bool Envelope::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Apply the envelope filter
std::vector<double> Envelope::apply(const std::vector<double> &x) 
{
    if (!isInitialized())
    {
        throw std::runtime_error("Envelope not initialized");
    }
    auto xCopy = x;
    return apply(std::move(xCopy));
}

std::vector<double> Envelope::apply(std::vector<double> &&x)
{
    if (!isInitialized())
    {
        throw std::invalid_argument("Envelope not initialized");
    }
    std::vector<double> y;
    if (x.empty()){return y;}
    USignal::Vector<double> xIn(std::move(x));
    pImpl->mTransformer->setInput(std::move(xIn));
    pImpl->mTransformer->apply();
    pImpl->transform(&y);
    /*
    const auto yAnalytic = pImpl->mTransformer->getOutputReference();
    if (yAnalytic.empty()){return y;}
    y.resize(yAnalytic.size());
    const auto yAnalyticPtr = yAnalytic.data();
    auto yPtr = y.data();
    for (int i = 0; i < static_cast<int> (y.size()); ++i)
    {
        yPtr[i] = std::abs(yAnalyticPtr[i]);
    }
    */
    return y;
}

/// Reset the initial conditions
void Envelope::resetInitialConditions()
{
    if (!isInitialized())
    {   
        throw std::runtime_error("Envelope filter not initialized");
    }   
    pImpl->mTransformer->resetInitialConditions();
}

std::vector<double> Envelope::getOutput() const
{
    if (!isInitialized())
    {
        throw std::runtime_error("Narrow band filter not initialized");
    }
    std::vector<double> y;
    pImpl->transform(&y);
    return y;
}


/// Destructor
Envelope::~Envelope() = default;
