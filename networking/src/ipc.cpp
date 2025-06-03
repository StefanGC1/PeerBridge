#include "ipc.hpp"
#include <iostream>

// Updated constructor without P2PSystem dependency
IPCServer::IPCServer()
    : onGetStunInfo_(nullptr), onShutdown_(nullptr) {}

IPCServer::~IPCServer() {
    ShutdownServer();
}

void IPCServer::setGetStunInfoCallback(GetStunInfoCallback callback) {
    onGetStunInfo_ = callback;
}

void IPCServer::setShutdownCallback(ShutdownCallback callback) {
    onShutdown_ = callback;
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

    // Check if callback is set
    if (!onGetStunInfo_) {
        std::string error_msg = "STUN info callback not set.";
        reply->set_public_ip("");
        reply->set_public_port(0);
        reply->set_error_message(error_msg);
        std::cerr << "IPCServer: Error: " << error_msg << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, error_msg);
    }

    // Call the callback to get STUN info
    auto [public_ip, public_port] = onGetStunInfo_();

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

grpc::Status IPCServer::StopProcess(grpc::ServerContext* context,
                                    const peerbridge::StopProcessRequest* request,
                                    peerbridge::StopProcessResponse* reply) {
    std::cout << "IPCServer: StopProcess called" << std::endl;
    
    // Check if callback is set
    if (!onShutdown_) {
        std::string error_msg = "Shutdown callback not set.";
        reply->set_success(false);
        reply->set_message(error_msg);
        std::cerr << "IPCServer: Error: " << error_msg << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, error_msg);
    }
    
    bool force = request->force();
    std::cout << "IPCServer: Initiating shutdown (force=" << (force ? "true" : "false") << ")" << std::endl;
    
    // Call the callback to initiate shutdown
    onShutdown_(force);
    
    reply->set_success(true);
    reply->set_message("Shutdown initiated");
    
    return grpc::Status::OK;
}

// Example RPC method implementation
// grpc::Status IPCServer::YourMethod(grpc::ServerContext* context, const peerbridge::RequestType* request, peerbridge::ResponseType* reply) {
//     // Your implementation here
//     return grpc::Status::OK;
// } 