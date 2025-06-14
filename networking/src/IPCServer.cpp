#include "IPCServer.hpp"
#include "SystemStateManager.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include <iostream>
#include <vector>
#include <map>

// Updated constructor without P2PSystem dependency
IPCServer::IPCServer(
    std::shared_ptr<SystemStateManager> stateManager,
    NetworkConfigManager& networkConfigManager)
    : getStunInfoCallback(nullptr)
    , shutdownCallback(nullptr)
    , stateManager(stateManager)
    , networkConfigManager(networkConfigManager)
{}

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

void IPCServer::RunServer(const std::string& serverAddress)
{
    grpc::ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(this);

    server = builder.BuildAndStart();
    if (server)
    {
        SYSTEM_LOG_INFO("[IPCServer] listening on {}", serverAddress);
        server->Wait();
    }
    else
    {
        SYSTEM_LOG_ERROR("[IPCServer] Failed to start [IPCServer] on {}", serverAddress);
    }
}

void IPCServer::ShutdownServer() {
    if (server)
    {
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        server->Shutdown(deadline);
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
    StunInfo stunInfo = getStunInfoCallback();

    if (!stunInfo.publicIp.empty() && stunInfo.publicPort > 0 && stunInfo.publicKey.size() == crypto_box_PUBLICKEYBYTES)
    {
        reply->set_public_ip(stunInfo.publicIp);
        reply->set_public_port(stunInfo.publicPort);
        reply->set_public_key(stunInfo.publicKey.data(), stunInfo.publicKey.size());
        reply->set_error_message("");
        SYSTEM_LOG_INFO("[IPCServer]: Returning STUN info - IP: {}, Port: {}, and public key", stunInfo.publicIp, stunInfo.publicPort);
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

    // Check for testing failure flag
    if (request->should_fail())
    {
        std::string errorMsg = "Connection failed due to shouldFail flag being set.";
        reply->set_success(false);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_INFO("[IPCServer]: Simulating failure due to shouldFail flag");
        return grpc::Status::OK;
    }

    std::vector<std::string> peerInfo;
    for (int i = 0; i < request->peer_info_size(); i++)
    {
        peerInfo.push_back(request->peer_info(i));
    }

    int self_index = request->self_index();
    SYSTEM_LOG_INFO("[IPCServer]: Self index: {}", self_index);

    // TODO: Check for existance of self_index and that size of list is >= 2
    SYSTEM_LOG_INFO("[IPCServer]: Received peer info:");
    for (size_t i = 0; i < peerInfo.size(); i++)
    {
        SYSTEM_LOG_INFO("[IPCServer] {}: {}", i, peerInfo[i]);
    }

    NetworkConfigManager::SetupConfig setupConfig = networkConfigManager.getSetupConfig();
    auto peerMap = utils::parsePeerInfo(peerInfo, setupConfig.IP_SPACE, self_index);

    if (peerMap.empty())
    {
        std::string errorMsg = "No valid peer info received.";
        reply->set_success(false);
        reply->set_error_message(errorMsg);
        SYSTEM_LOG_ERROR("[IPCServer]: Error: {}", errorMsg);
        return grpc::Status::OK;
    }

    SYSTEM_LOG_INFO("[IPCServer]: Parsed following peer map:");
    for (const auto& [virtualIp, publicIpAndPort] : peerMap)
    {
        SYSTEM_LOG_INFO("[IPCServer] Virtual IP: {}, Public IP: {}, Port: {}",
            utils::uint32ToIp(virtualIp),
            utils::uint32ToIp(publicIpAndPort.first),
            publicIpAndPort.second);
    }

    SYSTEM_LOG_INFO("[IPCServer]: Queueing initialize connection event");
    stateManager->queueEvent(NetworkEventData(NetworkEvent::INITIALIZE_CONNECTION, std::make_pair(self_index, peerMap)));
    bool success = true;

    reply->set_success(success);
    if (success)
    {
        reply->set_error_message("");
        SYSTEM_LOG_INFO("[IPCServer]: Connection initialized successfully");
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

    SYSTEM_LOG_INFO("[IPCServer]: Queueing disconnect all event");
    bool success = false;
    stateManager->queueEvent(NetworkEventData(NetworkEvent::DISCONNECT_ALL_REQUESTED));
    success = true;

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

grpc::Status IPCServer::GetConnectionStatus(
    grpc::ServerContext* context,
    const peerbridge::GetConnectionStatusRequest* request,
    peerbridge::GetConnectionStatusResponse* reply)
{
    // TODO: Implement this
    return grpc::Status::OK;
}

// Example RPC method implementation
// grpc::Status IPCServer::SomeEvent(
//      grpc::ServerContext* context, 
//      const peerbridge::RequestType* request, 
//      peerbridge::ResponseType* reply)
//{
//     return grpc::Status::OK;
// } 