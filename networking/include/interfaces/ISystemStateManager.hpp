#pragma once

#include <variant>
#include <map>
#include <chrono>
#include <optional>
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

class ISystemStateManager
{
public:
    virtual ~ISystemStateManager() = default;

    virtual void setState(SystemState state) = 0;
    virtual SystemState getState() const = 0;
    virtual bool isInState(SystemState state) const = 0;

    virtual void queueEvent(const NetworkEventData& event) = 0;
    virtual std::optional<NetworkEventData> getNextEvent() = 0;
    virtual bool hasEvents() const = 0;
};