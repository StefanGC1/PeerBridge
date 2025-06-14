#pragma once

#include "SystemStateManager.hpp"
#include "NetworkConfigManager.hpp"
#include <grpcpp/grpcpp.h>
#include "peerbridge.grpc.pb.h"
#include <functional>
#include <string>
#include <sodium.h>

class IPCServer final : public peerbridge::PeerBridgeService::Service {
public:
    struct StunInfo
    {
        std::string publicIp;
        int publicPort;
        std::array<uint8_t, crypto_box_PUBLICKEYBYTES>& publicKey;
    };
    // Define callback types
    using GetStunInfoCallback = std::function<StunInfo()>;
    using ShutdownCallback = std::function<void(bool)>;

    IPCServer(
        std::shared_ptr<SystemStateManager>,
        NetworkConfigManager&);
    ~IPCServer();

    void RunServer(const std::string&);
    void ShutdownServer();

    // Callback setters
    void setGetStunInfoCallback(GetStunInfoCallback);
    void setShutdownCallback(ShutdownCallback);

    // RPC method implementation for GetStunInfo
    grpc::Status GetStunInfo(
        grpc::ServerContext*, 
        const peerbridge::StunInfoRequest*, 
        peerbridge::StunInfoResponse*) override;
                             
    // RPC method implementation for StopProcess
    grpc::Status StopProcess(
        grpc::ServerContext*,
        const peerbridge::StopProcessRequest*,
        peerbridge::StopProcessResponse*) override;
                            
    // RPC method implementation for StartConnection
    grpc::Status StartConnection(
        grpc::ServerContext*,
        const peerbridge::StartConnectionRequest*,
        peerbridge::StartConnectionResponse* reply) override;
                                
    // RPC method implementation for StopConnection
    grpc::Status StopConnection(
        grpc::ServerContext*,
        const peerbridge::StopConnectionRequest* ,
        peerbridge::StopConnectionResponse*) override;

    // RPC method implementation for GetConnectionStatus
    grpc::Status GetConnectionStatus(
        grpc::ServerContext*,
        const peerbridge::GetConnectionStatusRequest*,
        peerbridge::GetConnectionStatusResponse*) override;

private:
    std::unique_ptr<grpc::Server> server;

    std::shared_ptr<SystemStateManager> stateManager;
    NetworkConfigManager& networkConfigManager;

    // Callbacks
    GetStunInfoCallback getStunInfoCallback;
    ShutdownCallback shutdownCallback;
}; 