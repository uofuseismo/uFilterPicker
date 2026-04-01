#ifndef UFILTER_PICKER_CHARACTERISTIC_FUNCTION_HPP
#define UFILTER_PICKER_CHARACTERISTIC_FUNCTION_HPP
#include <memory>
#include <vector>
namespace UFilterPicker
{
/// @brief Computes the characteristic function of a signal.
///        This is accomplished by effectively computing a Z-score of the
///        current sample w.r.t. to a previous window of samples.
class CharacteristicFunction
{
public:
    explicit CharacteristicFunction(const int windowInSamples);

    [[nodiscard]] int getWindowLength() const;
    [[nodiscard]] bool isInitialized() const noexcept;
    /// @brief Computes the characteristic function of the input signal.
    /// @param[in] x   The input signal, likely an envelope, of which to
    ///                compute teh chararacteristic function.
    [[nodiscard]] std::vector<double> apply(const std::vector<double> &x);
    [[nodiscard]] std::vector<double> getOutput() const;
    void resetInitialConditions();

    ~CharacteristicFunction();
    CharacteristicFunction(const CharacteristicFunction &) = delete;
    CharacteristicFunction& operator=(const CharacteristicFunction &) = delete;
     
    CharacteristicFunction() = delete;
private:
    class CharacteristicFunctionImpl;
    std::unique_ptr<CharacteristicFunctionImpl> pImpl;
};
}
#endif

