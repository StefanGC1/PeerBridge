#include "Utils.hpp"
#include "P2PSystem.hpp"
#include "Logger.hpp"
#include <iostream>
#include <vector>
#include <sstream>

namespace {
// REMOVE LATER
inline void dumpMulticastPacket(const std::vector<uint8_t>& buf,
                                const std::string& textPrefix)
{
    if (buf.size() < 34) return;                  // IPv4 + UDP header minimal
    const uint8_t  ihl   = (buf[0] & 0x0F) * 4;   // usually 20
    const uint16_t dport = (buf[ihl+2] << 8) | buf[ihl+3];

    std::ostringstream out;
    size_t payloadStart = ihl + 8;
    size_t payloadLen   = buf.size() - payloadStart;
    size_t printLen     = std::min(payloadLen, static_cast<size_t>(50));

     out << textPrefix
        << " MC-LAN • printed size " << payloadLen << " B  | ";

    // payload starts after UDP header (ihl + 8)
    for (size_t i = 0; i < printLen; ++i) {
        uint8_t c = buf[payloadStart + i];
        out << (std::isprint(c) ? static_cast<char>(c) : '.');
    }

    if (payloadLen > printLen) {
        out << "...";  // indicate truncation
    }

    // NETWORK_TRAFFIC_LOG("[Network] Multicast packet: {}", out.str());
}
}

// Helper struct for IP header
struct IPPacket
{
    uint8_t version4__ihl4;
    uint8_t typeOfService;
    uint16_t totalLength;
    uint16_t identification;
    uint16_t flags2__fragmentOffset14;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t headerChecksum;
    uint32_t sourceIp;
    uint32_t destIp;
    // uint32_t Options
};

P2PSystem::P2PSystem() 
    : running(false)
    , publicPort(0)
    , peerPort(0)
{
    stateManager = std::make_shared<SystemStateManager>();
    networkConfigManager = std::make_shared<NetworkConfigManager>();
}

P2PSystem::~P2PSystem()
{
    shutdown();
    if (monitorThread.joinable())
    {
        monitorThread.join();
    }
}

bool P2PSystem::startIPCServer(const std::string& serverAddress)
{
    // Create IPC server if it doesn't exist
    if (!ipcServer)
        ipcServer = std::make_unique<IPCServer>(stateManager, networkConfigManager);

    ipcServer->setGetStunInfoCallback([this]() -> IPCServer::StunInfo
    {
        return {this->publicIp, this->publicPort, this->publicKey};
    });

    ipcServer->setShutdownCallback([this](bool force)
    {
        // Initiate process shutdown
        if (force)
        {
            // Force immediate exit
            SYSTEM_LOG_INFO("[System] Force shutdown requested, exiting immediately");
            std::_Exit(0);
        } else
        {
            // Graceful shutdown
            SYSTEM_LOG_INFO("[System] Graceful shutdown requested");
            
            // Queue shutdown event
            stateManager->queueEvent(NetworkEvent::SHUTDOWN_REQUESTED);
        }
    });
    
    // Run the server in a separate thread
    ipcServerThread = std::thread([this, serverAddress]()
    {
        ipcServer->RunServer(serverAddress);
    });
    
    return true;
}

void P2PSystem::stopIPCServer()
{
    try
    {
        if (ipcServer)
        {
            ipcServer->ShutdownServer();
            if (ipcServerThread.joinable())
            {
                ipcServerThread.join();
            }
            ipcServer = nullptr;
            SYSTEM_LOG_INFO("[System] IPC Server stopped");
        }
    }
    catch (const std::exception& e)
    {
        SYSTEM_LOG_ERROR("[System] Error stopping IPC Server: {}", e.what());
    }
}

bool P2PSystem::initialize(int localPort)
{
    running = true;
    stateManager->setState(SystemState::IDLE);

    /*
    *   STUN PROCEDURE SETUP
    */

    // Discover public address for NAT traversal
    if (!discoverPublicAddress())
    {
        SYSTEM_LOG_ERROR("[System] Failed to do STUN and discover public address.");
        return false;
    }

    /*
    *   IPC SERVER SETUP
    */

   // Start IPC server
    std::string ipcServerAddress = "0.0.0.0:50051";
    if (!startIPCServer(ipcServerAddress))
    {
        SYSTEM_LOG_ERROR("[System] Failed to start IPC server. Exiting.");
        return false;
    }
    SYSTEM_LOG_INFO("[System] IPC Server started on {}", ipcServerAddress);

    /*
    *   TUN INTERFACE SETUP
    */

    // Initialize TUN interface
    if (!tunInterface)
        tunInterface = std::make_unique<TunInterface>();
    if (!tunInterface->initialize("PeerBridge"))
    {
        SYSTEM_LOG_ERROR("[System] Failed to initialize TUN interface");
        return false;
    }
    
    // Register packet callback from TUN interface
    tunInterface->setPacketCallback([this](const std::vector<uint8_t>& packet)
    {
        boost::asio::post(networkModule->getIOContext(), [this, packet = std::move(packet)]()
        {
            networkModule->processPacketFromTun(packet);
        });
    });

    networkConfigManager->setNarrowAlias(tunInterface->getNarrowAlias());

    /*
    *   NETWORK MODULE SETUP
    */

    // Create networking class, using the socket from STUN to preserve NAT binding
    if (!networkModule)
        networkModule = std::make_unique<UDPNetwork>(
            std::move(stunService->getSocket()),
            stunService->getContext(),
            stateManager,
            networkConfigManager);
    
    // Set up network callbacks for P2P connection
    networkModule->setMessageCallback([this](std::vector<uint8_t> packet)
    {
        if (tunInterface && tunInterface->isRunning())
            tunInterface->sendPacket(std::move(packet));
    });
    
    // Start UDP network
    if (!networkModule->startListening(localPort))
    {
        SYSTEM_LOG_ERROR("[System] Failed to start UDP network");
        return false;
    }

    /*
    *   ENCRYPTION SETUP
    */

    if (sodium_init() == -1)
    {
        SYSTEM_LOG_ERROR("[System] Failed to initialize libsodium encryption");
        return false;
    }

    if (crypto_box_keypair(publicKey.data(), secretKey.data()) == -1)
    {
        SYSTEM_LOG_ERROR("[System] Failed to generate encryption keypair");
        return false;
    }
    SYSTEM_LOG_INFO("[System] Encryption keypair generated");

    SYSTEM_LOG_INFO("[System] Test printing first 5 bytes of public key:");
    SYSTEM_LOG_INFO(
        "[System] Public key (first 5): {:02X} {:02X} {:02X} {:02X} {:02X}",
        static_cast<unsigned>(publicKey[0]),
        static_cast<unsigned>(publicKey[1]),
        static_cast<unsigned>(publicKey[2]),
        static_cast<unsigned>(publicKey[3]),
        static_cast<unsigned>(publicKey[4]));

    // TODO: REMOVE
    SYSTEM_LOG_INFO("[System] Test printing first 5 bytes of secret key:");
    SYSTEM_LOG_INFO(
        "[System] Secret key (first 5): {:02X} {:02X} {:02X} {:02X} {:02X}",
        static_cast<unsigned>(secretKey[0]),
        static_cast<unsigned>(secretKey[1]),
        static_cast<unsigned>(secretKey[2]),
        static_cast<unsigned>(secretKey[3]),
        static_cast<unsigned>(secretKey[4]));

    /*
    *   MONITOR LOOP
    */

    // Start monitoring loop
    if (shouldRunMonitorThread)
        monitorThread = std::thread([this]()
        {
            while (running && !stateManager->isInState(SystemState::SHUTTING_DOWN))
            {
                this->monitorLoop();
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        });

    SYSTEM_LOG_INFO("[System] P2P System initialized successfully");
    
    return true;
}

void P2PSystem::monitorLoop()
{
    // Process all pending events
    while (auto event = stateManager->getNextEvent())
    {
        handleNetworkEvent(*event);
    }
    
    // Add a additional health checks if necessary
}

void P2PSystem::handleNetworkEvent(const NetworkEventData& event)
{
    SystemState currentState = stateManager->getState();
    
    switch (event.event)
    {
        case NetworkEvent::INITIALIZE_CONNECTION:
        {
            SYSTEM_LOG_INFO("[SYSTEM] Received initialize connection event");
            if (currentState != SystemState::IDLE)
            {
                SYSTEM_LOG_ERROR("[System] Cannot initialize connection in state {}", toString(currentState));
                break;
            }
            auto variantPtr = std::get_if<NetworkEventData::SelfIndexAndPeerMap>(&event.data);
            if (!variantPtr)
            {
                SYSTEM_LOG_ERROR("[System] Invalid connection data, received type {}", event.data.index());
                break;
            }
            initializeConnectionData(*variantPtr);
            break;
        }

        case NetworkEvent::PEER_CONNECTED:
        {
            SYSTEM_LOG_INFO("[SYSTEM] Received peer connected event");
            if (currentState == SystemState::CONNECTING) 
            {
                if (!startNetworkInterface()) {
                    SYSTEM_LOG_ERROR("[System] Failed to start network interface");\
                    stopConnection();
                    break;
                }
                stateManager->setState(SystemState::CONNECTED);
                SYSTEM_LOG_INFO("[System] Peer connected successfully");
            }
            break;
        }

        case NetworkEvent::PEER_DISCONNECTED:
        {
            SYSTEM_LOG_INFO("[SYSTEM] Received peer disconnected event");
            if (auto variantPtr = std::get_if<std::string>(&event.data)) {
                SYSTEM_LOG_WARNING("[System] Peer disconnected: {}", *variantPtr);
            } else {
                SYSTEM_LOG_WARNING("[System] Peer disconnected");
            }
            break;
        }

        case NetworkEvent::ALL_PEERS_DISCONNECTED:
        {
            SYSTEM_LOG_INFO("[SYSTEM] Received all peers disconnected event");
            if (currentState == SystemState::CONNECTED)
            {
                SYSTEM_LOG_WARNING("[System] All peers disconnected");
                stopConnection();
            }
            break;
        }

        case NetworkEvent::DISCONNECT_ALL_REQUESTED:
        {
            SYSTEM_LOG_INFO("[SYSTEM] Received disconnect all requested event");
            stopConnection();
            break;
        }

        case NetworkEvent::SHUTDOWN_REQUESTED:
        {
            if (currentState != SystemState::SHUTTING_DOWN)
            {
                SYSTEM_LOG_INFO("[SYSTEM] Received shutdown requested event");
                shutdown();
            }
            break;
        }
    }
}

void P2PSystem::initializeConnectionData(
    const NetworkEventData::SelfIndexAndPeerMap& selfIndexAndPeerMap)
{
    int selfIndex = selfIndexAndPeerMap.first;
    auto peerMap = selfIndexAndPeerMap.second;

    std::vector<std::string> configVirtualPeerIps;
    for (const auto& [virtualIp, _] : peerMap)
    {
        configVirtualPeerIps.push_back(utils::uint32ToIp(virtualIp));
    }

    uint32_t selfIp = utils::ipToUint32(
        networkConfigManager->getSetupConfig().IP_SPACE + std::to_string(selfIndex + 1));
    currentConnectionConfig = {
        .selfVirtualIp = utils::uint32ToIp(selfIp),
        .peerVirtualIps = configVirtualPeerIps
    };

    stateManager->setState(SystemState::CONNECTING);

    // Call startConnection from networkModule with post
    boost::asio::post(networkModule->getIOContext(), [this, selfIp, selfIndexAndPeerMap]()
    {
        networkModule->startConnection(selfIp, this->secretKey, selfIndexAndPeerMap.second);
    });
}

bool P2PSystem::discoverPublicAddress()
{
    if (!stunService)
        stunService = std::make_unique<StunClient>();
    
    stunService->setStunServer("stun.l.google.com", "19302");
    auto publicAddr = stunService->discoverPublicAddress();
    if (!publicAddr)
    {
        SYSTEM_LOG_ERROR("[System] Failed to discover public address via STUN");
        return false;
    }
    
    publicIp = publicAddr->ip;
    publicPort = publicAddr->port;

    SYSTEM_LOG_INFO("[System] Public address: {}:{}", publicIp, std::to_string(publicPort));
    
    return true;
}

bool P2PSystem::startNetworkInterface()
{
    if (!isConnected() || stateManager->getState() != SystemState::CONNECTING)
    {
        SYSTEM_LOG_WARNING("[System] Cannot configure interface, not connected to a peer");
        return false;
    }

    networkConfigManager->configureInterface(currentConnectionConfig);

    // Start packet processing
    if (!tunInterface->startPacketProcessing()) {
        SYSTEM_LOG_ERROR("[System] Failed to start packet processing");
        return false;
    }

    SYSTEM_LOG_INFO("[System] Packet processing thread started");
    return true;
}

bool P2PSystem::isConnected() const
{
    return networkModule ? networkModule->isConnected() : false;
}

/*
* Disconnect and shutdown
*/

void P2PSystem::stopNetworkInterface()
{
    if (tunInterface && tunInterface->isRunning())
    {
        tunInterface->stopPacketProcessing();
        networkConfigManager->resetInterfaceConfiguration(currentConnectionConfig.peerVirtualIps);
        currentConnectionConfig = {};
        SYSTEM_LOG_INFO("[System] Network interface stopped and configuration reset");
    }
}

// Stop current connection but keep system running
void P2PSystem::stopConnection()
{
    // Stop the network connection, queue handler to IOContext
    if (networkModule)
    {
        boost::asio::post(networkModule->getIOContext(), [this]()
        {
            networkModule->stopConnection();
        });
    }
    // Stop the network interface
    stopNetworkInterface();
    
    // Update system state
    stateManager->setState(SystemState::IDLE);
    
    SYSTEM_LOG_INFO("[System] Connection stopped, system ready for new connections");
}

// Full system shutdown
void P2PSystem::shutdown()
{
    if (!running)
    {
        SYSTEM_LOG_WARNING("[System] System is already shutting down");
        return;
    }

    stateManager->setState(SystemState::SHUTTING_DOWN);

    // First stop any active connections
    if (networkModule)
    {
        boost::asio::post(networkModule->getIOContext(), [this]()
        {
            networkModule->shutdown();
        });
    }

    // Give the IOThread time to stop
    // COMMENT WHILE TESTING?
    // std::this_thread::sleep_for(std::chrono::seconds(2));

    // Stop the network interface
    stopNetworkInterface();
    
    // Close the TUN interface
    if (tunInterface)
    {
        tunInterface->close();
    }

    // Stop the IPC Server and thread
    stopIPCServer();

    // Stop main thread sleep
    running = false;
    
    SYSTEM_LOG_INFO("[System] System shut down successfully");
}

void P2PSystem::cleanup()
{
    if (running)
    {
        SYSTEM_LOG_WARNING("[System] System is still running, cannot initiate cleanup");
        return;
    }
    
    SYSTEM_LOG_INFO("[System] Cleaning up resources, stopping monitor thread");
    if (monitorThread.joinable())
    {
        monitorThread.join();
    }
    SYSTEM_LOG_INFO("[System] Resources cleaned up");
}

// A bit of a bandaid fix :)
bool P2PSystem::isRunning() const
{
    return running;
}

// A bit of a bandaid fix :)
void P2PSystem::setRunning(bool value)
{
    running = value;
}