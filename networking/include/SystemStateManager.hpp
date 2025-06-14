#pragma once

#include <mutex>
#include <chrono>
#include <atomic>
#include <queue>
#include <optional>
#include <variant>
#include <string>
#include <map>
#include <boost/asio/ip/udp.hpp>
#include <sodium/crypto_box.h>

// System states
enum class SystemState
{
    IDLE,
    CONNECTING,
    CONNECTED,
    SHUTTING_DOWN
};

// Network event types, used for transitions
enum class NetworkEvent
{
    INITIALIZE_CONNECTION,
    DISCONNECT_ALL_REQUESTED,
    PEER_CONNECTED,
    PEER_DISCONNECTED,
    ALL_PEERS_DISCONNECTED,
    SHUTDOWN_REQUESTED
};

inline std::string toString(SystemState state)
{
    switch (state) 
    {
        case SystemState::IDLE: return "IDLE";
        case SystemState::CONNECTING: return "CONNECTING";
        case SystemState::CONNECTED: return "CONNECTED";
        case SystemState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        default: return "UNKNOWN";
    }
}

// Event data to be sent by the network module
struct NetworkEventData
{
    // pair<self_index, Map vIP -> ( pair<peer IP, peer port>, public key)>
    using SelfIndexAndPeerMap = std::pair<int, std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>>>;
    NetworkEvent event;
    std::variant<
        std::monostate,
        std::string,
        SelfIndexAndPeerMap> data;
    std::chrono::steady_clock::time_point timestamp; // UNUSED
    
    // Constructor for events with string data
    NetworkEventData(NetworkEvent e, const std::string& endpoint) 
        : event(e), data(endpoint), timestamp(std::chrono::steady_clock::now()) {}
    
    // Constructor for events without data
    NetworkEventData(NetworkEvent e) 
        : event(e), data(std::monostate{}), timestamp(std::chrono::steady_clock::now()) {}

    // Constructor for events with peer map
    NetworkEventData(NetworkEvent e, const SelfIndexAndPeerMap& peerMap)
        : event(e), data(peerMap), timestamp(std::chrono::steady_clock::now()) {}
};

// Manages the overall system state
class SystemStateManager
{
public:
    SystemStateManager();

    // System state management
    void setState(SystemState state);
    SystemState getState() const;
    bool isInState(SystemState state) const;

    // Event queue
    void queueEvent(const NetworkEventData& event);
    std::optional<NetworkEventData> getNextEvent();
    bool hasEvents() const;
    
private:
    std::atomic<SystemState> currentState;

    std::queue<NetworkEventData> eventQueue;
    mutable std::mutex eventMutex;

    bool isValidTransition(SystemState from, SystemState to) const;
};

// Tracks information about a peer connection
class PeerConnectionInfo
{
public:
    using SharedKey = std::array<uint8_t, crypto_box_BEFORENMBYTES>;

    PeerConnectionInfo();
    PeerConnectionInfo(const boost::asio::ip::udp::endpoint&);
    PeerConnectionInfo(const boost::asio::ip::udp::endpoint&, const PeerConnectionInfo::SharedKey&);
    
    // Last active time (receive timestamp)
    void updateActivity();
    bool hasTimedOut(int = 10) const;
    
    // Connection state
    void setConnected(bool);
    bool isConnected() const;

    // Access peer endpoint
    boost::asio::ip::udp::endpoint getPeerEndpoint() const;
    
    // Access last activity time for monitoring
    std::chrono::steady_clock::time_point getLastActivity() const;
    
    // Access shared key for encryption
    const SharedKey& getSharedKey() const;
    
private:
    std::chrono::steady_clock::time_point lastActivity;
    bool connected;
    boost::asio::ip::udp::endpoint peerEndpoint;
    SharedKey sharedKey;
};
