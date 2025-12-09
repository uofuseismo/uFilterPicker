#include <string>
#include <chrono>
#include "uFilterPicker/options.hpp"

using namespace UFilterPicker;

class Options::OptionsImpl
{
public:
    std::vector<std::pair<double, double>> mPassBands
    {
        std::pair<double, double> (1, 3),
        std::pair<double, double> (3, 8),
        std::pair<double, double> (8, 15),
        std::pair<double, double> (15, 25),
        std::pair<double, double> (25, 35)
    };
    double mNominalSamplingRate{0};
    int mGapToleranceInSamples{5};
};

/// Constructor
Options::Options() :
    pImpl(std::make_unique<OptionsImpl> ())
{
}

/// Sampling rate
void Options::setNominalSamplingRate(const double samplingRate)
{
    if (samplingRate <= 0)
    {
        throw std::invalid_argument("Nominal sampling rate "
                                  + std::to_string(samplingRate)
                                  + " must be positive");
    }
    pImpl->mNominalSamplingRate = samplingRate;
}

/// Destructor
Options::~Options() = default;
