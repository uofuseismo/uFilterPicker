#include <memory>
#include <functional>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <uDataPacketServiceAPI/v1/packet.pb.h>
#include "uFilterPicker/grpcClient.hpp"
#include "uFilterPicker/grpcClientOptions.hpp"

using namespace UFilterPicker;

class GRPCClient::GRPCClientImpl
{
public:

//private:
    GRPCClientOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::function<void (UDataPacketServiceAPI::V1::Packet &&)> mCallback;
};

/// Destructor
GRPCClient::~GRPCClient() = default;
