#ifndef UFILTER_PICKER_PUBLISHER_HPP
#define UFILTER_PICKER_PUBLISHER_HPP
#include <memory>
#include <future>
#include <spdlog/logger.h>
namespace UFilterPickerPickBrokerAPI::V1
{
 class Pick;
}
namespace UFilterPicker
{
 class PublisherOptions;
}
namespace UFilterPicker
{
/// @class Publisher publisher.hpp
/// @brief Publishes picks to the uFilterPicker's pick broker for downstream
///        consumption.
/// @copyright Ben Baker (University of Utah) distributed under the MIT NO AI
///            license.
class Publisher
{
public:
    /// @brief Constructor.
    /// @param[in] options   The options defining the publisher.
    /// @param[in] logger    The application's logger.
    Publisher(const PublisherOptions &options,
              std::shared_ptr<spdlog::logger> logger);

    /// @brief Starts the publisher thread.
    std::future<void> start();

    /// @brief Enqueues a pick to send to the broker.
    /// @param[in,out] pick   The pick to send to the broker.
    ///                       On exit, pick's behavior is undefined.
    void enqueue(UFilterPickerPickBrokerAPI::V1::Pick &&pick);
    /// @brief Enqueues a pick to send to the broker.
    /// @param[in] pick  The pick to send to the broker.
    void enqueue(const UFilterPickerPickBrokerAPI::V1::Pick &pick);
 
    /// @brief Stops the publisher thread.
    void stop();

    /// @brief Destructor.
    ~Publisher();

    Publisher() = delete;
private:
    class PublisherImpl;
    std::unique_ptr<PublisherImpl> pImpl;
};
}
#endif
