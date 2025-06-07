#pragma once
#include "stun.hpp"
#include "networking.hpp"
#include "tun_interface.hpp"
#include "ipc.hpp"
// Forward declarations for IPCServer to avoid circular includes
class IPCServer;

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <unordered_map>

// Forward declarations
struct IPPacket;

class P2PSystem {
public:
    P2PSystem();
    ~P2PSystem();
    
    // Initialization
    bool initialize(const std::string& server_url, const std::string& username, int local_port = 0);
    
    // Connection management
    bool connectToPeer(const std::string& peer_username);
    bool startConnection(const std::vector<std::string>& peer_info, int self_index);
    void disconnect();
    
    // Network interface
    bool startNetworkInterface();
    void stopNetworkInterface();
    
    // Status
    bool isConnected() const;
    bool isRunning() const;
    bool isHost() const;
    std::string getPublicIP() const;   // Getter for public IP
    int getPublicPort() const;       // Getter for public port
    
    // Connection request handling
    void acceptIncomingRequest();
    void rejectIncomingRequest();
    
    // Set callbacks
    using StatusCallback = std::function<void(const std::string&)>;
    using ConnectionCallback = std::function<void(bool, const std::string&)>;
    using ConnectionRequestCallback = std::function<void(const std::string&)>;
    
    void setStatusCallback(StatusCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setConnectionRequestCallback(ConnectionRequestCallback callback);
    
private:
    // IPC Server management
    bool startIPCServer(const std::string& server_address);
    void stopIPCServer();

    // Network discovery
    bool discoverPublicAddress();
    
    // Handler methods
    void handleConnectionRequest(const std::string& from);
    void handlePeerInfo(const std::string& username, const std::string& ip, int port);
    void handleConnectionInit(const std::string& username, const std::string& ip, int port);
    void handleNetworkData(const std::vector<uint8_t>& data);
    void handlePacketFromTun(const std::vector<uint8_t>& packet);
    void handleConnectionChange(bool connected);
    
    // IP helpers
    void assignIPAddresses();
    std::string uint32ToIp(uint32_t ipAddress);
    
    // P2P VPN Setup
    bool setupVirtualInterface();
    
    // Packet analysis and forwarding
    bool forwardPacketToPeer(const std::vector<uint8_t>& packet);
    bool deliverPacketToTun(const std::vector<uint8_t>& packet);
    
    // Members
    std::string username_;
    std::string pending_request_from_;
    std::atomic<bool> running_;
    std::atomic<bool> is_host_;
    std::mutex packet_mutex_;
    
    // Virtual network configuration
    static constexpr const char* VIRTUAL_NETWORK = "10.0.0.0";
    static constexpr const char* VIRTUAL_NETMASK = "255.255.255.0";
    static constexpr const char* HOST_IP = "10.0.0.1";
    static constexpr const char* CLIENT_IP = "10.0.0.2";
    
    std::string local_virtual_ip_;
    std::string peer_virtual_ip_;
    
    // Components
    StunClient stun_;
    std::unique_ptr<UDPNetwork> network_;
    std::unique_ptr<TunInterface> tun_;
    std::unique_ptr<IPCServer> ipc_server_;  // Added IPCServer as a member
    std::thread ipc_thread_;                  // Thread for running the IPC server
    
    // P2P network thread
    std::thread packet_handling_thread_;
    
    // Public address
    std::string public_ip_;
    int public_port_;
    
    // Peer info
    std::string peer_username_;
    std::string peer_ip_;
    int peer_port_;
    
    // Callbacks
    StatusCallback on_status_;
    ConnectionCallback on_connection_;
    ConnectionRequestCallback on_connection_request_;
}; 