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
}

P2PSystem::~P2PSystem()
{
    shutdown();
}

bool P2PSystem::startIPCServer(const std::string& serverAddress)
{
    // Create IPC server if it doesn't exist
    if (!ipcServer)
    {
        ipcServer = std::make_unique<IPCServer>(stateManager, networkConfigManager);
        
        // Set up callbacks
        ipcServer->setGetStunInfoCallback([this]() -> std::pair<std::string, int>
        {
            return {this->publicIp, this->publicPort};
        });
        
        // Add shutdown callback
        ipcServer->setShutdownCallback([this](bool force)
        {
            // Initiate process shutdown
            if (force)
            {
                // Force immediate exit
                SYSTEM_LOG_INFO("[System] Force shutdown requested, exiting immediately");
                exit(0);
            } else
            {
                // Graceful shutdown
                SYSTEM_LOG_INFO("[System] Graceful shutdown requested");
                
                // Call our shutDown method instead of just changing running_
                this->shutdown();
            }
        });
        
        // Add start connection callback
        ipcServer->setStartConnectionCallback([this](const std::vector<std::string>& peerInfo, int selfIndex) -> bool
        {
            // TO DO: IMPLEMENT FOR *1 OR REMOVE
            return true;
        });
        
        // Add stop connection callback
        ipcServer->setStopConnectionCallback([this]() -> bool
        {
            // Disconnect from peers but keep the process running
            // TO DO: IMPLEMENT FOR *1 OR REMOVE
            return true;
        });
    }
    
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

    networkConfigManager.setNarrowAlias(tunInterface->getNarrowAlias());

    /*
    *   NETWORK MODULE SETUP
    */

    // Create networking class, using the socket from STUN to preserve NAT binding
    networkModule = std::make_unique<UDPNetwork>(
        std::move(stunService.getSocket()),
        stunService.getContext(),
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
    
    // Start monitoring loop
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
            SYSTEM_LOG_INFO("[SYSTEM] Received shutdown requested event");
            shutdown();
            break;
        }
    }
}

void P2PSystem::initializeConnectionData(
    const std::pair<int, std::map<uint32_t, std::pair<std::uint32_t, int>>>& selfIndexAndPeerMap)
{
    int selfIndex = selfIndexAndPeerMap.first;
    auto peerMap = selfIndexAndPeerMap.second;

    std::vector<std::string> configVirtualPeerIps;
    for (const auto& [virtualIp, _] : peerMap)
    {
        configVirtualPeerIps.push_back(utils::uint32ToIp(virtualIp));
    }

    uint32_t selfIp = utils::ipToUint32(
        networkConfigManager.getSetupConfig().IP_SPACE + std::to_string(selfIndex + 1));
    currentConnectionConfig = {
        .selfVirtualIp = utils::uint32ToIp(selfIp),
        .peerVirtualIps = configVirtualPeerIps
    };

    stateManager->setState(SystemState::CONNECTING);

    // Call startConnection from networkModule with post
    boost::asio::post(networkModule->getIOContext(), [this, selfIp, selfIndexAndPeerMap]()
    {
        networkModule->startConnection(selfIp, selfIndexAndPeerMap.second);
    });
}

bool P2PSystem::discoverPublicAddress()
{
    auto publicAddr = stunService.discoverPublicAddress();
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

    networkConfigManager.configureInterface(currentConnectionConfig);

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
        networkConfigManager.resetInterfaceConfiguration(currentConnectionConfig.peerVirtualIps);
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
    // First stop any active connections
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
    running = false;
    stateManager->setState(SystemState::SHUTTING_DOWN);
    
    // Stop the network interface
    stopNetworkInterface();
    tunInterface->close();
    
    // Stop the network
    if (networkModule)
    {
        networkModule->shutdown();
    }
    
    // Close the TUN interface
    if (tunInterface)
    {
        tunInterface->close();
    }
    
    // Stop the IPC Server and thread
    stopIPCServer();
    if (ipcServerThread.joinable())
    {
        ipcServerThread.join();
    }

    if (monitorThread.joinable())
    {
        monitorThread.join();
    }
    
    SYSTEM_LOG_INFO("[System] System shut down successfully");
}