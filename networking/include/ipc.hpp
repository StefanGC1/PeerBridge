#ifndef IPC_HPP
#define IPC_HPP

#include <grpcpp/grpcpp.h>
#include "peerbridge.grpc.pb.h"
#include "p2p_system.hpp" // Include P2PSystem header

// Forward declare P2PSystem if not passing by reference/pointer in constructor immediately
// class P2PSystem;

class IPCServer final : public peerbridge::PeerBridgeService::Service {
public:
    // Constructor now takes a reference to P2PSystem
    explicit IPCServer(P2PSystem& p2p_system);
    ~IPCServer();

    void RunServer(const std::string& server_address);
    void ShutdownServer();

    // RPC method implementation for GetStunInfo
    grpc::Status GetStunInfo(grpc::ServerContext* context, 
                             const peerbridge::StunInfoRequest* request, 
                             peerbridge::StunInfoResponse* reply) override;

private:
    // Example RPC method implementation (to be defined in .proto and implemented here)
    // grpc::Status YourMethod(grpc::ServerContext* context, const peerbridge::RequestType* request, peerbridge::ResponseType* reply) override;

    std::unique_ptr<grpc::Server> server_;
    P2PSystem& p2p_system_; // Reference to the P2P system
};

#endif // IPC_HPP 