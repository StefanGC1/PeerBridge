#pragma once
#include "SystemStateManager.hpp"
#include "NetworkConfigManager.hpp"
#include "interfaces/INetworkModule.hpp"
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>
#include <optional>
#include <boost/asio.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/steady_timer.hpp>

// Helper class for peer connection information
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


/* ====================================================================================================== */


class UDPNetwork : public IUDPNetwork
{
public:
    
    UDPNetwork(
        std::unique_ptr<boost::asio::ip::udp::socket>,
        boost::asio::io_context&,
        std::shared_ptr<ISystemStateManager>,
        std::shared_ptr<INetworkConfigManager>);
    ~UDPNetwork();
    
    // Setup and connection
    bool startListening(int port) override;
    bool startConnection(
        uint32_t,
        const std::array<uint8_t, crypto_box_SECRETKEYBYTES>&,
        std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>>) override;
    
    // Disconnet and shutdown
    void stopConnection() override;
    void shutdown() override;
    
    bool isConnected() const override;
    
    // Async operations, sending to peer, queued by TUNInterface
    void processPacketFromTun(const std::vector<uint8_t>&) override;
    bool sendMessage(
        const std::vector<uint8_t>& data,
        const boost::asio::ip::udp::endpoint& peerEndpoint,
        const std::array<uint8_t, crypto_box_BEFORENMBYTES>& sharedKey) override;
    void setMessageCallback(MessageCallback callback) override;
    
    // Get local information
    // int getLocalPort() const;
    // std::string getLocalAddress() const;

    // External handle to IOContext
    boost::asio::io_context& getIOContext() override;

private:
     // Packet types
    enum class PacketType : uint8_t {
        HOLE_PUNCH = 0x01,
        HEARTBEAT = 0x02,
        MESSAGE = 0x03,
        ACK = 0x04,
        DISCONNECT = 0x05
    };

    // Async operations, receiving from peer, sending to TUNInterface
    void startAsyncReceive();
    void handleReceiveFrom(
        const boost::system::error_code&,
        std::size_t, 
        std::shared_ptr<std::vector<uint8_t>>, 
        std::shared_ptr<boost::asio::ip::udp::endpoint>);
    void processReceivedData(
        std::size_t, 
        std::shared_ptr<std::vector<uint8_t>>, 
        std::shared_ptr<boost::asio::ip::udp::endpoint>);
    void deliverPacketToTun(const std::vector<uint8_t>);
    void handleSendComplete(
        const boost::system::error_code&,
        std::size_t, uint32_t,
        const boost::asio::ip::udp::endpoint&);
    
    // Disconnect handlers
    void handleDisconnect(boost::asio::ip::udp::endpoint, bool = false);
    void sendDisconnectNotification(const boost::asio::ip::udp::endpoint&);

    // UDP hole punching
    void startHolePunchingProcess();
    void sendHolePunchPacket(const boost::asio::ip::udp::endpoint&);
    
    // Connection management
    void checkAllConnections();
    void notifyConnectionEvent(NetworkEvent, const std::string& = "");

    // Keep-alive functionality
    void startKeepAliveTimer();
    void stopKeepAliveTimer();
    void handleKeepAlive(const boost::system::error_code&);

    // Custom header
    uint32_t attachCustomHeader(
        const std::shared_ptr<std::vector<uint8_t>>&,
        PacketType,
        std::optional<uint32_t> = std::nullopt);
    
    // Constants
    static constexpr size_t MAX_PACKET_SIZE = 65507;
    static constexpr uint16_t PROTOCOL_VERSION = 1;
    static constexpr uint32_t MAGIC_NUMBER = 0x12345678;

    std::atomic<bool> running;
    int localPort;
    std::string localAddress;

    // ASIO and IO context objects
    std::unique_ptr<boost::asio::ip::udp::socket> socket;
    boost::asio::io_context& ioContext;
    std::thread ioThread;
    boost::asio::steady_timer keepAliveTimer;
    
    // Ack tracking
    std::atomic<uint32_t> nextSeqNumber;
    std::unordered_map<uint32_t, std::chrono::time_point<std::chrono::steady_clock>> pendingAcks;
    std::mutex pendingAcksMutex;
    
    // Peer connection management
    std::map<uint32_t, std::pair<std::uint32_t, int>> virtualIpToPublicIp;
    std::map<uint32_t, PeerConnectionInfo> publicIpToPeerConnection;
    uint32_t selfVirtualIp;
    
    // State manager for event queuing
    std::shared_ptr<ISystemStateManager> stateManager;
    std::shared_ptr<INetworkConfigManager> networkConfigManager;
    
    // Callbacks
    MessageCallback onMessageCallback;
};
