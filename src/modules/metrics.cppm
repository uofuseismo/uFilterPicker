module;
#include <utility>
#include <memory>
#include <string>
//#include <mutex>
#include <opentelemetry/nostd/shared_ptr.h>
//#include <opentelemetry/metrics/meter.h>
#include <opentelemetry/metrics/meter_provider.h>
//#include <opentelemetry/metrics/provider.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#ifdef WITH_OTLP_GRPC
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#endif
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>

#include <cctype>
#include <cstdint>
#include <map>
#include <exception>
#include <stdexcept>
//#include <algorithm>
#include <opentelemetry/nostd/variant.h>
#include <opentelemetry/metrics/observer_result.h>
//#include <uDataPacketServiceAPI/v1/packet.pb.h>
//#include <uDataPacketServiceAPI/v1/stream_identifier.pb.h>
#include "uFilterPicker/metrics.hpp"
//#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
//#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
//#include <opentelemetry/sdk/metrics/view/view_factory.h>


export module Metrics;
import FilterPickerOptions;
import OTelOptions;

namespace
{

bool metricsInitialized{false};

void initializeHTTP(
    const bool exportMetrics,
    // NOLINTNEXTLINE(misc-include-cleaner)
    const UFilterPicker::OTelOptions::HTTPMetrics &otelHTTPMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpHttpMetricExporterOptions exporterOptions;
    exporterOptions.url = otelHTTPMetricsOptions.url
                        + otelHTTPMetricsOptions.suffix;
    //exporterOptions.console_debug = debug != "" && debug != "0" && debug != "no";
    exporterOptions.content_type
        = otel::exporter::otlp::HttpRequestContentType::kBinary;

    auto exporter
        = otel::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
             exporterOptions);
    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelHTTPMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelHTTPMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}

#ifdef WITH_OTLP_GRPC
void initializeGRPC(
    const bool exportMetrics,
    // NOLINTNEXTLINE(misc-include-cleaner)
    const UFilterPicker::OTelOptions::GRPCMetrics &otelGRPCMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpGrpcMetricExporterOptions exporterOptions;
    exporterOptions.endpoint = otelGRPCMetricsOptions.url;
    exporterOptions.use_ssl_credentials = false;
    if (!otelGRPCMetricsOptions.certificatePath.empty())
    {
        exporterOptions.use_ssl_credentials = true;
        exporterOptions.ssl_credentials_cacert_path
           = otelGRPCMetricsOptions.certificatePath;
    }
    auto exporter
        = otel::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
             exporterOptions);

    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelGRPCMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelGRPCMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}
#endif
}

namespace UFilterPicker::Metrics
{

export
void initialize(
    // NOLINTNEXTLINE(misc-include-cleaner)
    const UFilterPicker::Options::ProgramOptions &options)
{
    if (options.exportMetricsWithHTTP)
    {   
        return ::initializeHTTP(options.exportMetrics,
                                options.otelHTTPMetricsOptions);
    }   
    else
    {   
#ifdef WITH_OTLP_GRPC
        return ::initializeGRPC(options.exportMetrics,
                                options.otelGRPCMetricsOptions);
#else
        throw std::runtime_error("Recompile with Conan and OTLP_GRPC");
#endif
    }   
}
export void cleanup()
{
    if (metricsInitialized)
    {
        const std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
        opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
    }
    metricsInitialized = false;
}

/*
export
[[nodiscard]]
std::string toKeyName(
     const UDataPacketServiceAPI::V1::StreamIdentifier &identifier)
{
     const auto &network = identifier.network();
     if (network.empty()){throw std::runtime_error("Network is empty");}
     const auto &station = identifier.station();
     if (station.empty()){throw std::runtime_error("Station is empty");}
     const auto &channel = identifier.channel();
     if (channel.empty()){throw std::runtime_error("Channel is empty");}
     const auto &locationCode = identifier.location_code();

     auto result = network + "_"
                 + station + "_"
                 + channel;
     if (!locationCode.empty()){result = result + "_" + locationCode;}
     std::transform(result.begin(), result.end(), result.begin(), ::tolower);
     return result;
}


export 
[[nodiscard]]
std::string toKeyName(const UDataPacketServiceAPI::V1::Packet &packet)
{
     return toKeyName(packet.stream_identifier());
}

export 
class MetricsSingleton
{
public:
    [[maybe_unused]] static MetricsSingleton &getInstance()
    {   
        std::mutex mutex;
        const std::scoped_lock lock{mutex};
        static MetricsSingleton instance;
        return instance;
    }   

    void incrementDetectorResets(const std::string &key)
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mResetsCounterMap.find(key);
        if (idx == mResetsCounterMap.end())
        {
            mResetsCounterMap.insert( std::pair {key, 1} );
        }
        else
        {
            idx->second = idx->second + 1;
        }
    }

    [[nodiscard]]
    std::map<std::string, int64_t> getDetectorResetsCounters() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        return mResetsCounterMap;
    }

    void incrementPicks(const std::string &key)
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        auto idx = mPicksCounterMap.find(key);
        if (idx == mPicksCounterMap.end())
        {
            mPicksCounterMap.insert( std::pair {key, 1} );
        }
        else
        {
            idx->second = idx->second + 1;
        }
    }

    [[nodiscard]] 
    std::map<std::string, int64_t> getPicksCounters() const noexcept
    {
        const std::lock_guard<std::mutex> lock(mMutex);
        return mPicksCounterMap;
    }

private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    std::map<std::string, int64_t> mResetsCounterMap;
    std::map<std::string, int64_t> mPicksCounterMap;
    mutable std::mutex mMutex;

};

export void initializeSingleton()
{
    MetricsSingleton::getInstance();
}
*/

export
void observeDetectorResets(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        try
        {
            auto &instance = MetricsSingleton::getInstance();
            auto map = instance.getDetectorResetsCounters();
            for (const auto &item : map)
            {
                try
                {
                    const auto key = item.first;
                    const auto value = item.second;
                    const std::map<std::string, std::string>
                        attribute{ {"stream", item.first} };
                    observer->Observe(value, attribute);
                }
                catch (const std::exception &e) 
                {
                    throw std::runtime_error("Problem observing result for "
                                           + item.first + ": " 
                                           + std::string {e.what()});
                }
            }
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Problem observing detector resets: "
                                   + std::string {e.what()});
        }
    }
}

export
void observePicks(
    opentelemetry::metrics::ObserverResult observerResult,
    void *)
{
    if (opentelemetry::nostd::holds_alternative
        <
            opentelemetry::nostd::shared_ptr
            <
                opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult))
    {
        auto observer = opentelemetry::nostd::get
        <
            opentelemetry::nostd::shared_ptr
            <
               opentelemetry::metrics::ObserverResultT<int64_t>
            >
        > (observerResult);
        try
        {
            auto &instance = MetricsSingleton::getInstance();
            auto map = instance.getPicksCounters();
            for (const auto &item : map)
            {
                try
                {
                    const auto key = item.first;
                    const auto value = item.second;
                    const std::map<std::string, std::string>
                        attribute{ {"stream", item.first} };
                    observer->Observe(value, attribute);
                }
                catch (const std::exception &e) 
                {
                    throw std::runtime_error("Problem observing result for "
                                           + item.first + ": " 
                                           + std::string {e.what()});
                }
            }
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error("Problem observing picks: "
                                   + std::string {e.what()});
        }
    }
}



}

