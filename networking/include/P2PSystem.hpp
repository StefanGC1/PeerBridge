#pragma once
#include "Stun.hpp"
#include "IPCServer.hpp"
#include "NetworkingModule.hpp"
#include "TUNInterface.hpp"
#include "NetworkConfigManager.hpp"
#include "SystemStateManager.hpp"
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <unordered_map>
#include <sodium.h>

// Forward declarations
struct IPPacket;

class P2PSystem
{
public:
    P2PSystem();
    ~P2PSystem();
    
    // Initialization
    bool initialize(int = 0);
    
    // Disconnect and complete shutdown
    // TODO: FOR *1, maybe make a peer-based stopConnection
    void stopConnection();
    void shutdown();
    void cleanup();
    
    // Network interface
    bool startNetworkInterface();
    void stopNetworkInterface();
    
    // Status
    bool isConnected() const;
    bool isRunning() const;
    void setRunningFalse();
    
    // Connection monitoring
    void monitorLoop();
    void handleNetworkEvent(const NetworkEventData&);
    
private:
    // IPC Server
    bool startIPCServer(const std::string&);
    void stopIPCServer();

    // Network discovery
    bool discoverPublicAddress();

    // Initialize connection
    void initializeConnectionData(
        const NetworkEventData::SelfIndexAndPeerMap&);

    // Data
    NetworkConfigManager::ConnectionConfig currentConnectionConfig;

    // TO REMOVE
    std::atomic<bool> running;
    
    std::string localVirtualIp;
    // TODO: REFACTORIZE FOR *1, KEEP virtual_ip -> public_ip map
    std::string peerVirtualIp;

    std::string publicIp;
    int publicPort;

    std::string peerUsername;
    std::string peerIp;
    int peerPort;

    // State management
    std::shared_ptr<SystemStateManager> stateManager;
    std::thread monitorThread;
    
    // Components
    NetworkConfigManager networkConfigManager;
    StunClient stunService;
    std::unique_ptr<UDPNetwork> networkModule;
    std::unique_ptr<TunInterface> tunInterface;

    std::unique_ptr<IPCServer> ipcServer;
    std::thread ipcServerThread;

    // Encryption
    using PublicKey = std::array<uint8_t, crypto_box_PUBLICKEYBYTES>;
    using SecretKey = std::array<uint8_t, crypto_box_SECRETKEYBYTES>;

    PublicKey publicKey;
    SecretKey secretKey;
}; 