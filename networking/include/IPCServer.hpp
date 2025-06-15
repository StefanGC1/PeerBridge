#pragma once

#include "SystemStateManager.hpp"
#include "NetworkConfigManager.hpp"
#include "interfaces/IIPCServer.hpp"
#include <grpcpp/grpcpp.h>
#include "peerbridge.grpc.pb.h"
#include <functional>
#include <string>
#include <sodium.h>

class IPCServer final : public peerbridge::PeerBridgeService::Service, public IIPCServer
{
public:
    IPCServer(
        std::shared_ptr<ISystemStateManager>,
        std::shared_ptr<INetworkConfigManager>);
    ~IPCServer();

    void RunServer(const std::string&) override;
    void ShutdownServer() override;

    // Callback setters
    void setGetStunInfoCallback(GetStunInfoCallback) override;
    void setShutdownCallback(ShutdownCallback) override;

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

    std::shared_ptr<ISystemStateManager> stateManager;
    std::shared_ptr<INetworkConfigManager> networkConfigManager;

    // Callbacks
    GetStunInfoCallback getStunInfoCallback;
    ShutdownCallback shutdownCallback;
}; 