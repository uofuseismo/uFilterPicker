module;
#include <string>
#include <chrono>
#include <filesystem>

export module OTelOptions;

namespace UFilterPicker::OTelOptions
{

export struct GRPCMetrics
{
    std::string url{"localhost:4317"};
    std::chrono::milliseconds exportInterval{std::chrono::seconds {15}};
    std::chrono::milliseconds exportTimeOut{500};
    std::filesystem::path certificatePath; // Path to the cert file
};

export struct GRPCLog
{
    std::string url{"localhost:4317"};
    std::chrono::milliseconds exportTimeOut{500};
    std::filesystem::path certificatePath; // Path to the cert file
};


export struct HTTPMetrics
{
    std::string url{"localhost:4318"};
    std::chrono::milliseconds exportInterval{std::chrono::seconds {15}};
    std::chrono::milliseconds exportTimeOut{500};
    std::string suffix{"/v1/metrics"};
};

export struct HTTPLog
{
    std::string url{"localhost:4318"};
    std::filesystem::path certificatePath;
    std::string suffix{"/v1/logs"};
};

}
