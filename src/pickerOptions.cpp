#include <stdexcept>
#include <chrono>
#include <memory>
#include <utility>
#include "uFilterPicker/pickerOptions.hpp"

using namespace UFilterPicker;

class PickerOptions::PickerOptionsImpl
{
public:
    std::chrono::microseconds mMaximumLatency{std::chrono::minutes{15}};
    std::chrono::microseconds mMaximumFutureTime{0};
    int mGapTolerance{5}; 
    int mBurnInFactor{3};
};

/// Constructor
PickerOptions::PickerOptions() :
    pImpl(std::make_unique<PickerOptionsImpl> ())
{
}

/// Copy constructor
PickerOptions::PickerOptions(const PickerOptions &options)
{
    *this = options;
}

/// Move constructor
PickerOptions::PickerOptions(PickerOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
PickerOptions &PickerOptions::operator=(const PickerOptions &options)
{
    if (&options == this){return *this;}
    pImpl = std::make_unique<PickerOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
PickerOptions &PickerOptions::operator=(PickerOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}

/// Destructor
PickerOptions::~PickerOptions() = default;

/// The gap tolerance
void PickerOptions::setGapTolerance(const int gapTolerance)
{
    if (gapTolerance < 0)
    {
        throw std::invalid_argument("Gap tolerance must be positive");
    }
    pImpl->mGapTolerance = gapTolerance;
}

int PickerOptions::getGapTolerance() const noexcept
{
    return pImpl->mGapTolerance;
}

/// Maximum future time
void PickerOptions::setMaximumFutureTime(
    const std::chrono::microseconds &maxFutureTime) noexcept
{
    pImpl->mMaximumFutureTime = maxFutureTime;
}

std::chrono::microseconds PickerOptions::getMaximumFutureTime() const noexcept
{
    return pImpl->mMaximumFutureTime;
}

/// Maximum latency
void PickerOptions::setMaximumLatency(
    const std::chrono::microseconds &maxLatency)
{
    if (maxLatency.count() <= 0)
    { 
        throw std::invalid_argument("Max latency must be positive");
    }
    pImpl->mMaximumLatency = maxLatency;
}

std::chrono::microseconds PickerOptions::getMaximumLatency() const noexcept
{
    return pImpl->mMaximumLatency;
}

/// Burn in factor
void PickerOptions::setBurnInFactor(const int factor)
{
    if (factor < 0)
    {
        throw std::invalid_argument("Burn-in factor must be non-negative");
    }
    pImpl->mBurnInFactor = factor;
}

int PickerOptions::getBurnInFactor() const noexcept
{
    return pImpl->mBurnInFactor;
}
