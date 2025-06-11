#include "IPCServer.hpp"
#include "Logger.hpp"
#include <iostream>

// Updated constructor without P2PSystem dependency
IPCServer::IPCServer()
    : getStunInfoCallback(nullptr)
    , shutdownCallback(nullptr)
    , startConnectionCallback(nullptr)
    , stopConnectionCallback(nullptr) {}

IPCServer::~IPCServer()
{
    if (server)
    {
        ShutdownServer();
    }
}

void IPCServer::setGetStunInfoCallback(GetStunInfoCallback callback) {
    getStunInfoCallback = callback;
}

void IPCServer::setShutdownCallback(ShutdownCallback callback) {
    shutdownCallback = callback;
}

void IPCServer::setStartConnectionCallback(StartConnectionCallback callback) {
    startConnectionCallback = callback;
}

void IPCServer::setStopConnectionCallback(StopConnectionCallback callback) {
    stopConnectionCallback = callback;
}

void IPCServer::RunServer(const std::string& serverAddress)
{
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    server = builder.BuildAndStart();
    if (server)
    {
        SYSTEM_LOG_INFO("[IPCServer] listening on {}", serverAddress);
        // server->Wait(); // Blocking call, might run in a separate thread or handle differently
    }
    else
    {
        SYSTEM_LOG_ERROR("[IPCServer] Failed to start [IPCServer] on {}", serverAddress);
    }
}

void IPCServer::ShutdownServer() {
    if (server)
    {
        server->Shutdown();
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        server->Wait();
        server = nullptr;
        SYSTEM_LOG_INFO("[IPCServer] shut down completely.");
    }
}

grpc::Status IPCServer::GetStunInfo(
    grpc::ServerContext* context,
    const peerbridge::StunInfoRequest* request,
    peerbridge::StunInfoResponse* reply)
{
    SYSTEM_LOG_INFO("[IPCServer]: GetStunInfo called");

    // Check if callback is set
    if (!getStunInfoCallback)
    {
        std::string errorMsg = "STUN info callback not set.";
        reply->set_public_ip("");
        reply->set_public_port(0);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error: {}", errorMsg);
        return grpc::Status(grpc::StatusCode::INTERNAL, errorMsg);
    }

    // Call the callback to get STUN info
    auto [publicIP, publicPort] = getStunInfoCallback();

    if (!publicIP.empty() && publicPort > 0)
    {
        reply->set_public_ip(publicIP);
        reply->set_public_port(publicPort);
        reply->set_error_message("");
        SYSTEM_LOG_INFO("[IPCServer]: Returning STUN info - IP: {}, Port: {}", publicIP, publicPort);
        return grpc::Status::OK;
    }
    else
    {
        // This could happen if STUN discovery failed or hasn't run yet.
        std::string errorMsg = "STUN information not available or discovery failed.";
        reply->set_public_ip("");
        reply->set_public_port(0);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error getting STUN info: {}", errorMsg);
        return grpc::Status(grpc::StatusCode::UNAVAILABLE, errorMsg);
    }
}

grpc::Status IPCServer::StopProcess(
    grpc::ServerContext* context,
    const peerbridge::StopProcessRequest* request,
    peerbridge::StopProcessResponse* reply)
{
    SYSTEM_LOG_INFO("[IPCServer]: StopProcess called");
    
    // Check if callback is set
    if (!shutdownCallback)
    {
        std::string errorMsg = "Shutdown callback not set.";
        reply->set_success(false);
        reply->set_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error: {}", errorMsg);
        return grpc::Status(grpc::StatusCode::INTERNAL, errorMsg);
    }
    
    bool force = request->force();
    SYSTEM_LOG_INFO("[IPCServer]: Initiating shutdown (force={})", force);
    
    // Call the callback to initiate shutdown
    shutdownCallback(force);
    
    reply->set_success(true);
    reply->set_message("Shutdown initiated");
    
    return grpc::Status::OK;
}

grpc::Status IPCServer::StartConnection(
    grpc::ServerContext* context,
    const peerbridge::StartConnectionRequest* request,
    peerbridge::StartConnectionResponse* reply)
{
    SYSTEM_LOG_INFO("[IPCServer]: StartConnection called");

    // Check if callback is set
    if (!startConnectionCallback)
    {
        std::string errorMsg = "Start connection callback not set.";
        reply->set_success(false);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error: {}", errorMsg);
        return grpc::Status(grpc::StatusCode::INTERNAL, errorMsg);
    }

    // Check for testing failure flag
    if (request->should_fail())
    {
        std::string errorMsg = "Connection failed due to shouldFail flag being set.";
        reply->set_success(false);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_INFO("[IPCServer]: Simulating failure due to shouldFail flag");
        return grpc::Status::OK;
    }

    // Convert peerInfo from repeated string to vector
    std::vector<std::string> peerInfo;
    for (int i = 0; i < request->peer_info_size(); i++)
    {
        peerInfo.push_back(request->peer_info(i));
    }

    int self_index = request->self_index();

    // Display peer info for testing
    SYSTEM_LOG_INFO("[IPCServer]: Received peer info:");
    for (size_t i = 0; i < peerInfo.size(); i++)
    {
        SYSTEM_LOG_INFO("[IPCServer] {}: {}", i, peerInfo[i]);
    }

    // Call the callback to start the connection
    // bool success = startConnectionCallback(peerInfo, self_index);
    bool success = true;

    reply->set_success(success);
    if (success)
    {
        reply->set_error_message("");
        SYSTEM_LOG_INFO("[IPCServer]: Connection setup successful");
    }
    else 
    {
        reply->set_error_message("Failed to set up connection with peers");
        SYSTEM_LOG_ERROR("[IPCServer]: Connection setup failed");
    }

    return grpc::Status::OK;
}

grpc::Status IPCServer::StopConnection(
    grpc::ServerContext* context,
    const peerbridge::StopConnectionRequest* request,
    peerbridge::StopConnectionResponse* reply)
{
    SYSTEM_LOG_INFO("[IPCServer]: StopConnection called");

    // Check if callback is set
    if (!stopConnectionCallback)
    {
        std::string errorMsg = "Stop connection callback not set.";
        reply->set_success(false);
        reply->set_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error: {}", errorMsg);
        return grpc::Status(grpc::StatusCode::INTERNAL, errorMsg);
    }

    // Call the callback to stop the connection
    bool success = false;
    try
    {
        // success = stopConnectionCallback();
        success = true;
    }
    catch (const std::exception& e)
    {
        SYSTEM_LOG_ERROR("[IPCServer]: Error in stop connection callback: {}", e.what());
        success = false;
        reply->set_success(success);
        reply->set_message(e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }

    reply->set_success(success);
    if (success)
    {
        reply->set_message("Connection stopped successfully");
        SYSTEM_LOG_INFO("[IPCServer]: Connection stopped successfully");
    }
    else
    {
        reply->set_message("Failed to stop connection");
        SYSTEM_LOG_ERROR("[IPCServer]: Failed to stop connection");
    }

    return grpc::Status::OK;
}

// Example RPC method implementation
// grpc::Status IPCServer::YourMethod(
//      grpc::ServerContext* context, 
//      const peerbridge::RequestType* request, 
//      peerbridge::ResponseType* reply)
//{
//     return grpc::Status::OK;
// } 