#ifndef UFILTER_PICKER_ENVELOPE_HPP
#define UFILTER_PICKER_ENVELOPE_HPP
#include <memory>
#include <vector>
namespace UFilterPicker
{
class EnvelopeOptions
{
public:
    /// The filter order.  This will have order + 1 coefficients.
    int order;
    /// The beta in the Kaiser FIR window-based filter design.
    double beta;
};
/// @class Envelope
/// @brief Computes the envelope of a signal by computing the Hilbert transform
///        via Type III Hilbert transformer FIR filter then takes the absolute
///        value.
/// @copyright Ben Baker (Unviersity of Utah) distributed under the MIT license.
class Envelope
{
public:
    /// @param[in] order   The filter order.  This will have order + 1
    ///                    coefficients.
    /// @param[in] beta    The beta in the Kaiser FIR window-based filter
    ///                    design.
    explicit Envelope(const EnvelopeOptions &options); //const int order, const double beta = 8);
    [[nodiscard]] int getOrder() const;
    /// @result True indicates the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;
    /// @brief Computes the envelope of the input signal.
    /// @param[in] x   The signal of which to compute the envelope.
    /// @result The envelope of x.
    [[nodiscard]] std::vector<double> apply(const std::vector<double> &x);
    [[nodiscard]] std::vector<double> apply(std::vector<double> &&x);
    /// @Result The output of the envelope filter.  This is for debugging.
    [[nodiscard]] std::vector<double> getOutput() const;
    void resetInitialConditions();

    ~Envelope();
    Envelope() = delete;
private:
    class EnvelopeImpl;
    std::unique_ptr<EnvelopeImpl> pImpl;
};
}
#endif
