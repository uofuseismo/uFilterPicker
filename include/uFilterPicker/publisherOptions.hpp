#ifndef UFILTER_PICKER_PUBLISHER_OPTIONS_HPP
#define UFILTER_PICKER_PUBLISHER_OPTIONS_HPP
#include <memory>
namespace UFilterPicker
{
 class GRPCClientOptions;
}
namespace UFilterPicker
{
/// @class PublisherOptions publisherOptions.hpp
/// @brief Sets the options defining the pick publisher's behavior.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class PublisherOptions
{
public:
    /// @brief Constructor.
    PublisherOptions();
    /// @brief Copy constructor.
    PublisherOptions(const PublisherOptions &options);
    /// @brief Move constructor.
    PublisherOptions(PublisherOptions &&options) noexcept;

    /// @brief Sets the gRPC client options.
    /// @param[in] options  The gRPC client options.
    void setGRPCOptions(const GRPCClientOptions &options);
    /// @result The gRPC client options.
    /// @throws std::runtime_error if \c hasGRPCOptions() is false.
    [[nodiscard]] GRPCClientOptions getGRPCOptions() const;
    /// @result True indicates the gRPC options were set.
    [[nodiscard]] bool hasGRPCOptions() const noexcept;

    /// @brief Sets the maximum queue size.  After the queue is full the oldest
    ///        picks will not be sent.
    /// @param[in] maximumQueueSize  The maximum of pick messages that can
    ///                              be buffered.
    /// @throws std::invalid_argument if maximumQueueSize is not positive.
    void setMaximumQueueSize(int maximumQueueSize);
    /// @result The maximum queue size. 
    [[nodiscard]] int getMaximumQueueSize() const noexcept;

    /// @brief Destructor.
    ~PublisherOptions();
    /// @brief Copy assignment.
    PublisherOptions& operator=(const PublisherOptions &options);
    /// @brief Move assignment.
    PublisherOptions& operator=(PublisherOptions &&options) noexcept;
private:
    class PublisherOptionsImpl;
    std::unique_ptr<PublisherOptionsImpl> pImpl;
};
}
#endif
