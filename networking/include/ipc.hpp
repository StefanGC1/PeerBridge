#ifndef IPC_HPP
#define IPC_HPP

#include <grpcpp/grpcpp.h>
#include "peerbridge.grpc.pb.h"
#include <functional>
#include <string>

// Remove direct dependency on P2PSystem
// Forward declarations
class P2PSystem;

class IPCServer final : public peerbridge::PeerBridgeService::Service {
public:
    // Define callback types
    using GetStunInfoCallback = std::function<std::pair<std::string, int>()>;
    using ShutdownCallback = std::function<void(bool)>; // bool parameter for force flag
    
    // Constructor without P2PSystem dependency
    IPCServer();
    ~IPCServer();

    void RunServer(const std::string& server_address);
    void ShutdownServer();

    // Callback setters
    void setGetStunInfoCallback(GetStunInfoCallback callback);
    void setShutdownCallback(ShutdownCallback callback);

    // RPC method implementation for GetStunInfo
    grpc::Status GetStunInfo(grpc::ServerContext* context, 
                             const peerbridge::StunInfoRequest* request, 
                             peerbridge::StunInfoResponse* reply) override;
                             
    // RPC method implementation for StopProcess
    grpc::Status StopProcess(grpc::ServerContext* context,
                            const peerbridge::StopProcessRequest* request,
                            peerbridge::StopProcessResponse* reply) override;

private:
    std::unique_ptr<grpc::Server> server_;
    
    // Callbacks
    GetStunInfoCallback onGetStunInfo_;
    ShutdownCallback onShutdown_;
};

#endif // IPC_HPP 