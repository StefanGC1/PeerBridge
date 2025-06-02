#include "ipc.hpp"
#include <iostream>

// Constructor now takes P2PSystem reference
IPCServer::IPCServer(P2PSystem& p2p_system)
    : p2p_system_(p2p_system) {}

IPCServer::~IPCServer() {
    ShutdownServer();
}

void IPCServer::RunServer(const std::string& server_address) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    server_ = builder.BuildAndStart();
    if (server_) {
        std::cout << "IPC Server listening on " << server_address << std::endl;
        // server_->Wait(); // Blocking call, might run in a separate thread or handle differently
    } else {
        std::cerr << "Failed to start IPC Server on " << server_address << std::endl;
    }
}

void IPCServer::ShutdownServer() {
    if (server_) {
        server_->Shutdown();
        // Optionally, wait for server to shutdown completely
        // server_->Wait(); 
        std::cout << "IPC Server shut down." << std::endl;
    }
}

grpc::Status IPCServer::GetStunInfo(grpc::ServerContext* context,
                                      const peerbridge::StunInfoRequest* request,
                                      peerbridge::StunInfoResponse* reply) {
    std::cout << "IPCServer: GetStunInfo called" << std::endl;

    // Assuming P2PSystem has methods to get the discovered public IP and port.
    // These would be populated after P2PSystem::initialize() calls discoverPublicAddress().
    std::string public_ip = p2p_system_.getPublicIP();
    int public_port = p2p_system_.getPublicPort();

    if (!public_ip.empty() && public_port > 0) {
        reply->set_public_ip(public_ip);
        reply->set_public_port(public_port);
        reply->set_error_message("");
        std::cout << "IPCServer: Returning STUN info - IP: " << public_ip << ", Port: " << public_port << std::endl;
        return grpc::Status::OK;
    } else {
        // This could happen if STUN discovery failed or hasn't run yet.
        std::string error_msg = "STUN information not available or discovery failed.";
        reply->set_public_ip("");
        reply->set_public_port(0);
        reply->set_error_message(error_msg);
        std::cerr << "IPCServer: Error getting STUN info: " << error_msg << std::endl;
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, error_msg);
    }
}

// Example RPC method implementation
// grpc::Status IPCServer::YourMethod(grpc::ServerContext* context, const peerbridge::RequestType* request, peerbridge::ResponseType* reply) {
//     // Your implementation here
//     return grpc::Status::OK;
// } 