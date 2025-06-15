#include "NetworkingModule.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <cstring>
#include <boost/asio/ip/address_v6.hpp>

// Not used anymore
PeerConnectionInfo::PeerConnectionInfo() : connected(false)
{
    updateActivity();
}

// Not used anymore
PeerConnectionInfo::PeerConnectionInfo(const boost::asio::ip::udp::endpoint& endpoint) : connected(false)
{
    peerEndpoint = endpoint;
    updateActivity();
}

PeerConnectionInfo::PeerConnectionInfo(
    const boost::asio::ip::udp::endpoint& endpoint,
    const PeerConnectionInfo::SharedKey& sharedKey) : connected(false)
{
    peerEndpoint = endpoint;
    this->sharedKey = sharedKey;
    updateActivity();
}

void PeerConnectionInfo::updateActivity()
{
    lastActivity = std::chrono::steady_clock::now();
}

bool PeerConnectionInfo::hasTimedOut(int timeoutSeconds) const
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity).count();
    return (elapsed > timeoutSeconds) && connected;
}

void PeerConnectionInfo::setConnected(bool connected_)
{
    connected = connected_;
    if (connected_)
    {
        updateActivity();
    }
}

bool PeerConnectionInfo::isConnected() const {
    return connected;
}

std::chrono::steady_clock::time_point PeerConnectionInfo::getLastActivity() const {
    return lastActivity;
}

boost::asio::ip::udp::endpoint PeerConnectionInfo::getPeerEndpoint() const
{
    return peerEndpoint;
}

const PeerConnectionInfo::SharedKey& PeerConnectionInfo::getSharedKey() const
{
    return sharedKey;
}


/* ====================================================================================================== */


UDPNetwork::UDPNetwork(
    std::unique_ptr<boost::asio::ip::udp::socket> socket,
    boost::asio::io_context& context,
    std::shared_ptr<ISystemStateManager> stateManager,
    std::shared_ptr<INetworkConfigManager> networkConfigManager) 
    : running(false)
    , localPort(0)
    , nextSeqNumber(0)
    , socket(std::move(socket))
    , ioContext(context)
    , stateManager(stateManager)
    , networkConfigManager(networkConfigManager)
    , keepAliveTimer(ioContext)
{
}

UDPNetwork::~UDPNetwork()
{
    shutdown();
}

bool UDPNetwork::startListening(int port)
{
    try
    {
        if (ioContext.stopped())
        {
            NETWORK_LOG_INFO("[Network] IO context is stopped, restarting...");
            ioContext.restart();
        }
        // Get local endpoint information
        boost::asio::ip::udp::endpoint local_endpoint = socket->local_endpoint();
        localAddress = local_endpoint.address().to_string();
        localPort = local_endpoint.port();
        
        // Increase socket buffer sizes for high-throughput scenarios
        boost::asio::socket_base::send_buffer_size sendBufferOption(4 * 1024 * 1024); // 4MB
        boost::asio::socket_base::receive_buffer_size recvBufferOption(4 * 1024 * 1024); // 4MB
        socket->set_option(sendBufferOption);
        socket->set_option(recvBufferOption);

        // Set running flag to true
        running = true;

        // Start async receiving
        NETWORK_LOG_INFO("[Network] Starting async receive");
        startAsyncReceive();
        NETWORK_LOG_INFO("[Network] Async receive started");
        
        // Start IO thread to handle asynchronous operations
        if (!ioThread.joinable())
        {
            NETWORK_LOG_INFO("[Network] Starting IOContext thread");
            ioThread = std::thread([this]()
            {
                // Set thread priority to time-critical
                #ifdef _WIN32
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
                #endif
                try
                {
                    NETWORK_LOG_INFO("[Network] IO thread started, running io context");
                    // This will keep running tasks until the work guard is reset / destroyed
                    size_t handlers_run = ioContext.run();
                    NETWORK_LOG_INFO("[Network] IO context finished running, {} handlers executed", handlers_run);
                }
                catch (const std::exception& e)
                {
                    NETWORK_LOG_ERROR("[Network] IO thread error: {}", e.what());
                }
                NETWORK_LOG_WARNING("[Network] IO thread finished running, shutting down");
            });
        }

        SYSTEM_LOG_INFO("[Network] Listening on UDP {}:{}", localAddress, localPort);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Network] Failed to start UDP listener: " << e.what() << std::endl;
        return false;
    }
}

bool UDPNetwork::startConnection(
    uint32_t selfIp,
    const std::array<uint8_t, crypto_box_SECRETKEYBYTES>& selfSecretKey,
    std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>> virtualIpToPublicIpPortAndKey)
{
    for (const auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
    {
        if (connectionInfo.isConnected())
        {
            SYSTEM_LOG_ERROR("[Network] Already connected to a peer: {}", publicIp);
            return false;
        }
    }

    selfVirtualIp = selfIp;

    // construct virtualIpToPublicIp and publicIpToPeerConnection
    std::map<uint32_t, std::pair<std::uint32_t, int>> _virtualIpToPublicIp;
    for (const auto& [virtualIp, publicIpPortAndKey] : virtualIpToPublicIpPortAndKey)
    {
        uint32_t publicIp = publicIpPortAndKey.first.first;
        int port = publicIpPortAndKey.first.second;
        std::array<uint8_t, crypto_box_PUBLICKEYBYTES> publicKey = publicIpPortAndKey.second;

        boost::asio::ip::address addr = boost::asio::ip::make_address(utils::uint32ToIp(publicIp));
        boost::asio::ip::udp::endpoint peerEndpoint(addr, port);
        PeerConnectionInfo::SharedKey sharedKey;
        
        if (crypto_box_beforenm(sharedKey.data(), publicKey.data(), selfSecretKey.data()) == -1)
        {
            SYSTEM_LOG_ERROR("[Network] Failed to construct shared key for peer {}", utils::uint32ToIp(publicIp));
            continue;
        }
        _virtualIpToPublicIp[virtualIp] = publicIpPortAndKey.first;
        publicIpToPeerConnection[publicIp] = PeerConnectionInfo(peerEndpoint, sharedKey);

        SYSTEM_LOG_INFO(
            "[Network] Constructed shared key for peer {}: {:02X} {:02X} {:02X} {:02X} {:02X}",
            utils::uint32ToIp(publicIp),
            static_cast<unsigned>(sharedKey[0]),
            static_cast<unsigned>(sharedKey[1]),
            static_cast<unsigned>(sharedKey[2]),
            static_cast<unsigned>(sharedKey[3]),
            static_cast<unsigned>(sharedKey[4]));
    }
    virtualIpToPublicIp = _virtualIpToPublicIp;

    // Start hole punching process
    startHolePunchingProcess();

    // Start keep-alive timer
    startKeepAliveTimer();

    return true;
}

void UDPNetwork::startHolePunchingProcess()
{
    // Send initial hole punching packets
    for (int i = 0; i < 5; i++)
    {
        for (const auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
        {
            sendHolePunchPacket(connectionInfo.getPeerEndpoint());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void UDPNetwork::sendHolePunchPacket(const boost::asio::ip::udp::endpoint& peerEndpoint)
{
    try
    {
        NETWORK_LOG_INFO("[Network] Sending hole-punch / keep-alive packet to peer: {}", peerEndpoint.address().to_string());
        // Create hole-punch packet with shared ownership
        auto packet = std::make_shared<std::vector<uint8_t>>(16);

        // Attach custom header
        attachCustomHeader(packet, PacketType::HOLE_PUNCH);
        
        // Send packet asynchronously
        socket->async_send_to(
            boost::asio::buffer(*packet), peerEndpoint,
            [packet](const boost::system::error_code& error, std::size_t bytesSent)
            {
                if (error && error != boost::asio::error::operation_aborted && 
                    error != boost::asio::error::would_block &&
                    error.value() != 10035) // WSAEWOULDBLOCK
                    {
                    NETWORK_LOG_ERROR("[Network] Error sending hole-punch packet: {}, with error code: {}", error.message(), error.value());
                }
            });
    }
    catch (const std::exception& e)
    {
        SYSTEM_LOG_ERROR("[Network] Error preparing hole-punch packet: {}", e.what());
        NETWORK_LOG_ERROR("[Network] Error preparing hole-punch packet: {}", e.what());
    }
}

void UDPNetwork::checkAllConnections()
{
    if (publicIpToPeerConnection.empty() || virtualIpToPublicIp.empty())
    {
        SYSTEM_LOG_WARNING("[Network] No more connections active, closing connection...");
        NETWORK_LOG_WARNING("[Network] No more connections active, closing connection...");
        stateManager->queueEvent(NetworkEventData(NetworkEvent::ALL_PEERS_DISCONNECTED));
        return;
    }

    for (auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
    {
        if (!connectionInfo.isConnected())
        {
            continue;
        }

        if (connectionInfo.hasTimedOut(20))
        {
            // Time out peer after 20 seconds of inactivity
            auto last_activity = connectionInfo.getLastActivity();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_activity).count();
            
            SYSTEM_LOG_ERROR("[Network] Connection timeout. No packets received for {} seconds (threshold: 20s).", elapsed);
            NETWORK_LOG_ERROR("[Network] Connection timeout. No packets received for {} seconds (threshold: 20s).", elapsed);
            
           // Mark as disconnected
            connectionInfo.setConnected(false);

            // Remove the peer after a couple of seconds
            boost::asio::steady_timer timer(ioContext);
            timer.expires_after(std::chrono::seconds(2));
            timer.async_wait([this, publicIp](const boost::system::error_code& error)
            {
                if (error != boost::asio::error::operation_aborted)
                {
                    auto it = publicIpToPeerConnection.find(publicIp);
                    if (it != publicIpToPeerConnection.end() &&
                        it->second.isConnected())
                    {
                        NETWORK_LOG_ERROR("[Network] Peer miraculously reconnected after timeout");
                        return;
                    }
                    
                    // Remove the peer
                    uint32_t ipToRemove = 0;
                    for (const auto& [virtualIp, publicIpAndPort] : virtualIpToPublicIp)
                    {
                        if (publicIpAndPort.first == publicIp)
                        {
                            ipToRemove = virtualIp;
                            break;
                        }
                    }

                    if (ipToRemove == selfVirtualIp)
                    {
                        NETWORK_LOG_ERROR("[Network] How did we get here? Cannot remove self from peer list");
                        return;
                    }

                    if (ipToRemove != 0)
                    {
                        publicIpToPeerConnection.erase(publicIp);
                        virtualIpToPublicIp.erase(ipToRemove);
                    }
                }
            });
        }
    }
}

void UDPNetwork::processPacketFromTun(const std::vector<uint8_t>& packet)
{
    if (!running || !socket)
    {
        NETWORK_LOG_ERROR("[Network] Cannot process packet from tun: socket not available or system not running (disconnected)");
        return;
    }

    // Extract source and destination IPs for filtering
    uint32_t srcIp = (packet[12] << 24) | (packet[13] << 16) | (packet[14] << 8) | packet[15];
    uint32_t dstIp = (packet[16] << 24) | (packet[17] << 16) | (packet[18] << 8) | packet[19];

    // Forward packets that are meant for peer OR are broadcast/multicast packets
    uint32_t BROADCAST_IP = utils::ipToUint32(networkConfigManager->getSetupConfig().IP_SPACE + std::to_string(255));

    const auto& virtualIpMapIter = virtualIpToPublicIp.find(dstIp);
    bool isForPeer = (virtualIpMapIter != virtualIpToPublicIp.end());
    bool isBroadcast = (dstIp == BROADCAST_IP) || (dstIp == NetworkConstants::BROADCAST_IP2);
    bool isMulticast = (dstIp >> 28) == 14; // 224.0.0.0/4 (first octet 224-239)

    if (!isForPeer && !isBroadcast && !isMulticast)
    {
        // Drop packet not meant for peer
        return;
    }

    // We know this exists in the map cause we checked it above
    if (isForPeer)
    {
        uint32_t peerPublicIp = virtualIpMapIter->second.first;
        if (publicIpToPeerConnection.find(peerPublicIp) == publicIpToPeerConnection.end())
        {
            NETWORK_LOG_ERROR(
                "[Network] Critical error: Peer entry found in virtualIpToPublicIp but not found in connectionInfo map: {}", peerPublicIp);
            return;
        }
        auto& peerConnection = publicIpToPeerConnection[peerPublicIp];
        boost::asio::ip::udp::endpoint peerEndpoint = peerConnection.getPeerEndpoint();
        // Send the packet to the peer
        sendMessage(
            packet,
            peerEndpoint,
            peerConnection.getSharedKey());
    }
    else if (isBroadcast || isMulticast)
    {
        for (const auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
        {
            sendMessage(
                packet,
                connectionInfo.getPeerEndpoint(),
                connectionInfo.getSharedKey());
        }
    }
}

// TODO: REFACTOR FOR *1, FOR MULTIPLE PEERS
bool UDPNetwork::sendMessage(
    const std::vector<uint8_t>& dataToSend,
    const boost::asio::ip::udp::endpoint& peerEndpoint,
    const std::array<uint8_t, crypto_box_BEFORENMBYTES>& sharedKey)
{ 
    try
    {
        constexpr size_t CUSTOM_HEADER_SIZE = 16;
        constexpr size_t NONCE_LENGTH = crypto_box_NONCEBYTES;
        constexpr size_t MAC_LENGTH = crypto_box_MACBYTES;
        
        // Calculate total packet size: header (16 bytes) + nonce (24 bytes) + mac (16 bytes) + message
        size_t winTunPacketSize = dataToSend.size();
        size_t wholePacketSize = CUSTOM_HEADER_SIZE + NONCE_LENGTH + MAC_LENGTH + winTunPacketSize;

        if (wholePacketSize > MAX_PACKET_SIZE)
        {
            NETWORK_LOG_ERROR("[Network] Message too large, max size is {}",
                (MAX_PACKET_SIZE - CUSTOM_HEADER_SIZE - NONCE_LENGTH - MAC_LENGTH));
            return false;
        }

        // Create packet with shared ownership for async operation
        auto packet = std::make_shared<std::vector<uint8_t>>(wholePacketSize);

        // Attach custom header
        uint32_t seq = attachCustomHeader(packet, PacketType::MESSAGE);
        
        // Set wintun packet length
        (*packet)[12] = (winTunPacketSize >> 24) & 0xFF;
        (*packet)[13] = (winTunPacketSize >> 16) & 0xFF;
        (*packet)[14] = (winTunPacketSize >> 8) & 0xFF;
        (*packet)[15] = winTunPacketSize & 0xFF;

        // Packet pointer position helpers for encryption
        uint8_t* basePos = packet->data();
        uint8_t* noncePos = basePos + CUSTOM_HEADER_SIZE;
        uint8_t* macPos = noncePos + NONCE_LENGTH;
        uint8_t* encrPos = macPos + MAC_LENGTH;

        // std::memcpy(packet->data() + 16, dataToSend.data(), dataToSend.size());
        std::memcpy(encrPos, dataToSend.data(), winTunPacketSize);

        // Encrypt
        randombytes_buf(noncePos, NONCE_LENGTH);
        crypto_box_easy_afternm(
            macPos, // dest
            encrPos, // source
            winTunPacketSize, // message len
            noncePos, // nonce
            sharedKey.data()); // shared key

        // Track for acknowledgment
        {
            std::lock_guard<std::mutex> ack_lock(pendingAcksMutex);
            pendingAcks[seq] = std::chrono::steady_clock::now();
        }

        /*
        * PACKET STRUCTURE: CUSTOM HEADER (16 bytes) + NONCE (24 bytes) then (MAC (16 bytes) + MESSAGE)
        */
        
        // Send packet asynchronously
        socket->async_send_to(
            boost::asio::buffer(*packet), peerEndpoint,
            [this, packet, seq, peerEndpoint](const boost::system::error_code& error, std::size_t bytesSent)
            {
                this->handleSendComplete(error, bytesSent, seq, peerEndpoint);
            });
        
        return true;
    }
    catch (const std::exception& e)
    {
        SYSTEM_LOG_ERROR("[Network] Send preparation error: {}", e.what());
        NETWORK_LOG_ERROR("[Network] Send preparation error: {}", e.what());
        return false;
    }
}

void UDPNetwork::handleSendComplete(
    const boost::system::error_code& error,
    std::size_t bytesSent,
    uint32_t seq,
    const boost::asio::ip::udp::endpoint& peerEndpoint)
{
    if (error)
    {
        if (error == boost::asio::error::would_block || 
            error == boost::asio::error::try_again ||
            error.value() == 10035) // WSAEWOULDBLOCK
        {

            NETWORK_LOG_INFO("[Network] Send buffer full");

            boost::asio::post(ioContext, [this, seq]()
            {
                // No resend, just log that we're dropping the packet
                NETWORK_LOG_INFO("[Network] Dropping packet due to send buffer limits: seq={}", seq);
                
                // Remove from pending acks
                std::lock_guard<std::mutex> lock(pendingAcksMutex);
                pendingAcks.erase(seq);
            });
        }
        else
        {
            SYSTEM_LOG_ERROR("[Network] Send error: {}, with error code: {}", error.message(), error.value());
            NETWORK_LOG_ERROR("[Network] Send error: {}, with error code: {}", error.message(), error.value());
            
            // Disconnect on fatal errors, not temporary ones
            if (error != boost::asio::error::operation_aborted)
            {
                boost::asio::post(ioContext, [this, peerEndpoint]() {this->handleDisconnect(peerEndpoint); });
            }
        }
    }
}

void UDPNetwork::setMessageCallback(MessageCallback callback)
{
    onMessageCallback = std::move(callback);
}

// int UDPNetwork::getLocalPort() const
// {
//     return localPort;
// }

// std::string UDPNetwork::getLocalAddress() const
// {
//     return localAddress;
// }

void UDPNetwork::startAsyncReceive()
{
    if (!socket) {
        NETWORK_LOG_ERROR("[Network] startAsyncReceive: socket is null!");
        return;
    }
    
    if (!socket->is_open()) {
        NETWORK_LOG_ERROR("[Network] startAsyncReceive: socket is not open!");
        return;
    }
    
    // Create buffer for each receive operation
    auto receiveBuffer = std::make_shared<std::vector<uint8_t>>(MAX_PACKET_SIZE);
    auto senderEndpoint = std::make_shared<boost::asio::ip::udp::endpoint>();
    
    socket->async_receive_from(
        boost::asio::buffer(*receiveBuffer), *senderEndpoint,
        [this, receiveBuffer, senderEndpoint](const boost::system::error_code& error, std::size_t bytesTransferred)
        {
            this->handleReceiveFrom(error, bytesTransferred, receiveBuffer, senderEndpoint);
        }
    );
}

void UDPNetwork::handleReceiveFrom(
    const boost::system::error_code& error,
    std::size_t bytesTransferred,
    std::shared_ptr<std::vector<uint8_t>> receiveBuffer,
    std::shared_ptr<boost::asio::ip::udp::endpoint> senderEndpoint)
{
    if (socket && socket->is_open() && error != boost::asio::error::operation_aborted)
    {
        startAsyncReceive(); // Continuously queue up another startAsyncReceive
    }

    if (!error)
    {
        processReceivedData(bytesTransferred, receiveBuffer, senderEndpoint);
    }
    else if (error != boost::asio::error::operation_aborted)
    {
        // Handle error but don't terminate unless it's fatal
        NETWORK_LOG_ERROR("[Network] Receive error: {} (code: {})", error.message(), error.value());
        
        if (error == boost::asio::error::would_block || 
            error == boost::asio::error::try_again ||
            error.value() == 10035) // WSAEWOULDBLOCK
        {
            // Recoverable errors
            NETWORK_LOG_WARNING("[Network] Recoverable receive error: {} (code: {}), continuing", error.message(), error.value());
        }
        else
        {
            // Fatal errors
            NETWORK_LOG_ERROR("[Network] Fatal receive error: {} (code: {}), disconnecting", error.message(), error.value());
            handleDisconnect(*senderEndpoint);
        }
    }
}

void UDPNetwork::processReceivedData(
    std::size_t bytesTransferred,
    std::shared_ptr<std::vector<uint8_t>> receiveBuffer,
    std::shared_ptr<boost::asio::ip::udp::endpoint> senderEndpoint)
{
    constexpr size_t CUSTOM_HEADER_SIZE = 16;
    constexpr size_t NONCE_LENGTH = crypto_box_NONCEBYTES;
    constexpr size_t MAC_LENGTH = crypto_box_MACBYTES;
    
    // Skip if we don't have enough data for header
    if (bytesTransferred < CUSTOM_HEADER_SIZE)
    {
        NETWORK_LOG_ERROR("[Network] Received packet too small: {} bytes", bytesTransferred);
        return;
    }
    
    const std::vector<uint8_t>& buffer = *receiveBuffer;

    /*
    * SMALL CUSTOM PROTOCOL HEADER
    */

    // Validate magic number
    uint32_t magic = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    if (magic != MAGIC_NUMBER)
    {
        NETWORK_LOG_WARNING("[Network] Received packet with invalid magic number: {}", magic);
        return;
    }
    
    // Validate protocol version
    uint16_t version = (buffer[4] << 8) | buffer[5];
    if (version != PROTOCOL_VERSION)
    {
        NETWORK_LOG_ERROR("[Network] Unsupported protocol version: {}", version);
        return;
    }
    
    // Get packet type
    PacketType packetType = static_cast<PacketType>(buffer[6]);
    
    // Get sequence number
    uint32_t seq = (buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) | buffer[11];

    uint32_t senderIp = utils::ipToUint32(senderEndpoint->address().to_string());
    if (publicIpToPeerConnection.find(senderIp) == publicIpToPeerConnection.end())
    {
        NETWORK_LOG_ERROR("[Network] Received packet from unknown peer: {}", senderEndpoint->address().to_string());
        return;
    }
    
    // Update peer activity time
    auto& peerConnection = publicIpToPeerConnection[senderIp];
    peerConnection.updateActivity();

    if (packetType == PacketType::DISCONNECT)
    {
        SYSTEM_LOG_INFO("[Network] Received disconnect notification from peer");
        NETWORK_LOG_INFO("[Network] Received disconnect notification from peer");
        handleDisconnect(*senderEndpoint);
        return;
    }

    // Consume packet if network not running
    if (!running)
    {
        NETWORK_LOG_ERROR("[Network] Received packet, but network not running");
        return;
    }

    // Store peer endpoint if not already connected
    if (!peerConnection.isConnected())
    {
        NETWORK_LOG_INFO("[Network] First valid packet received from peer, establishing connection");
        peerConnection.setConnected(true);
        
        // Notify peer connected event
        notifyConnectionEvent(NetworkEvent::PEER_CONNECTED, peerConnection.getPeerEndpoint().address().to_string());
    }

    // Process packet based on type
    switch (packetType)
    {
        case PacketType::HOLE_PUNCH:
            NETWORK_LOG_INFO("[Network] Received hole-punch packet from peer");
            // Activity time was already updated above
            break;
            
        case PacketType::HEARTBEAT:
            NETWORK_LOG_INFO("[Network] Received heartbeat packet from peer");
            // Activity time was already updated above
            break;
            
        case PacketType::MESSAGE:
        {
            if (bytesTransferred < CUSTOM_HEADER_SIZE + NONCE_LENGTH + MAC_LENGTH)
            {
                NETWORK_LOG_ERROR("[Network] Received WinTun packet too small: {} bytes", bytesTransferred);
                return;
            }
            uint32_t winTunPacketSize = (buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15];
            size_t wholePacketSize = CUSTOM_HEADER_SIZE + NONCE_LENGTH + MAC_LENGTH + winTunPacketSize;
            // Validate message length
            if (wholePacketSize > bytesTransferred)
            {
                NETWORK_LOG_ERROR("[Network] Message length exceeds packet size");
                return;
            }
            
            // Create ACK packet
            auto ack = std::make_shared<std::vector<uint8_t>>(16);

            // Attach custom header
            attachCustomHeader(ack, PacketType::ACK, std::make_optional(seq));
            
            // Send ACK
            socket->async_send_to(
                boost::asio::buffer(*ack), *senderEndpoint,
                [this, ack](const boost::system::error_code& error, std::size_t sent)
                {
                    if (error && error != boost::asio::error::operation_aborted)
                    {
                        NETWORK_LOG_ERROR("[Network] Error sending ACK: {} (code: {})", error.message(), error.value());
                    }
                });

            // Packet pointer position helpers for decryption
            uint8_t* basePos = receiveBuffer->data();
            uint8_t* noncePos = basePos + CUSTOM_HEADER_SIZE;
            uint8_t* macPos = noncePos + NONCE_LENGTH;
            uint8_t* encrPos = macPos + MAC_LENGTH;

            // Decrypt
            size_t encrSize = bytesTransferred - CUSTOM_HEADER_SIZE - NONCE_LENGTH;
            if (crypto_box_open_easy_afternm(
                macPos, // dest
                macPos, // source,
                encrSize,
                noncePos, // nonce
                peerConnection.getSharedKey().data()) != 0)
            {
                NETWORK_LOG_ERROR("[Network] Failed to decrypt message from peer: {}", senderEndpoint->address().to_string());
                return;
            }
            
            // Process message, send to wintun interface
            std::uint8_t* wintTunPacketPos = macPos;
            deliverPacketToTun({wintTunPacketPos, wintTunPacketPos + winTunPacketSize});
            break;
        }
        case PacketType::ACK:
        {
            // Remove from pending acks
            std::lock_guard<std::mutex> lock(pendingAcksMutex);
            pendingAcks.erase(seq);
            break;
        }
        default:
            NETWORK_LOG_ERROR("[Network] Unknown packet type: {}", static_cast<int>(packetType));
            break;
    }
}

void UDPNetwork::deliverPacketToTun(const std::vector<uint8_t> packet)
{
    // Extract source and destination IPs for filtering
    uint32_t srcIp = (packet[12] << 24) | (packet[13] << 16) | (packet[14] << 8) | packet[15];
    uint32_t dstIp = (packet[16] << 24) | (packet[17] << 16) | (packet[18] << 8) | packet[19];

    // Only deliver packets that are meant for us OR are broadcast/multicast packets
    uint32_t BROADCAST_IP = utils::ipToUint32(networkConfigManager->getSetupConfig().IP_SPACE + std::to_string(255));

    bool isForUs = (dstIp == selfVirtualIp);
    bool isBroadcast = (dstIp == BROADCAST_IP) || (dstIp == NetworkConstants::BROADCAST_IP2);
    bool isMulticast = (dstIp >> 28) == 14; // 224.0.0.0/4 (first octet 224-239)

    if (!isForUs && !isBroadcast && !isMulticast)
    {
        // Drop packet not meant for us
        return;
    }

    // Send the packet to the TUN interface
    onMessageCallback(std::move(packet));
}

bool UDPNetwork::isConnected() const
{
    return running;
}

void UDPNetwork::notifyConnectionEvent(NetworkEvent event, const std::string& endpoint)
{
    SYSTEM_LOG_INFO("[Network] Queuing network event: {}", static_cast<int>(event));
    if (endpoint.empty())
    {
        stateManager->queueEvent(NetworkEventData(event));
    }
    else
    {
        stateManager->queueEvent(NetworkEventData(event, endpoint));
    }
}

void UDPNetwork::handleDisconnect(
    boost::asio::ip::udp::endpoint peerEndpoint,
    bool isCausedByError)
{
    uint32_t ipToRemove = utils::ipToUint32(peerEndpoint.address().to_string());
    if (ipToRemove == selfVirtualIp)
    {
        NETWORK_LOG_ERROR("[Network] How did we get here? Cannot remove self from peer list");
        return;
    }

    if (isCausedByError)
    {
        NETWORK_LOG_ERROR("[Network] Peer disconnected due to error: {}", peerEndpoint.address().to_string());
        sendDisconnectNotification(peerEndpoint);
    }

    virtualIpToPublicIp.erase(ipToRemove);
    publicIpToPeerConnection.erase(ipToRemove);
    notifyConnectionEvent(NetworkEvent::PEER_DISCONNECTED, peerEndpoint.address().to_string());
}

void UDPNetwork::sendDisconnectNotification(const boost::asio::ip::udp::endpoint& peerEndpoint)
{
    try
    {
        // Inneficient, but it is what it is :)
        bool connected;
        for (const auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
        {
            if (connectionInfo.getPeerEndpoint() == peerEndpoint)
            {
                connected = connectionInfo.isConnected();
                break;
            }
        }

        if (!connected || !socket)
        {
            return; // No connection to notify
        }

        SYSTEM_LOG_INFO("[Network] Sending disconnect notification to peer");
        NETWORK_LOG_INFO("[Network] Sending disconnect notification to peer");
        
        // Create disconnect packet
        auto packet = std::make_shared<std::vector<uint8_t>>(16);
        
        // Attach custom header
        attachCustomHeader(packet, PacketType::DISCONNECT);
        
        // Send packet - try multiple times to increase chance of delivery
        for (int i = 0; i < 3; i++)
        {
            socket->async_send_to(
                boost::asio::buffer(*packet), peerEndpoint,
                [packet](const boost::system::error_code& error, std::size_t bytesSent)
                {
                    // Ignore errors since we're disconnecting
                });
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    catch (const std::exception& e)
    {
        SYSTEM_LOG_ERROR("[Network] Error sending disconnect notification: {}", e.what());
        NETWORK_LOG_ERROR("[Network] Error sending disconnect notification: {}", e.what());
    }
}

void UDPNetwork::stopConnection()
{
    for (auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
    {
        if (connectionInfo.isConnected())
        {
            NETWORK_LOG_INFO("[Network] Sending disconnect notification to peer: {}",
                connectionInfo.getPeerEndpoint().address().to_string());
            sendDisconnectNotification(connectionInfo.getPeerEndpoint());
            connectionInfo.setConnected(false);
        }
    }

    virtualIpToPublicIp.clear();
    publicIpToPeerConnection.clear();

    running = false;

    stopKeepAliveTimer();
    
    stateManager->setState(SystemState::IDLE);
    
    SYSTEM_LOG_INFO("[Network] Stopped connection to peer");
    NETWORK_LOG_INFO("[Network] Stopped connection to peer");
}

void UDPNetwork::shutdown()
{
    // Stop any active connection
    if (!publicIpToPeerConnection.empty() && !virtualIpToPublicIp.empty())
    {
        stopConnection();
    }
    
    // Then shut down the network stack
    running = false;
    stateManager->setState(SystemState::SHUTTING_DOWN);

    stopKeepAliveTimer();

    if (socket)
    {
        boost::system::error_code ec;
        socket->cancel(ec);
        socket->close(ec);
    }
    
    // Stop io_context 
    ioContext.stop();

    if (ioThread.joinable())
        ioThread.join();
    
    SYSTEM_LOG_INFO("[Network] Network subsystem shut down");
}

void UDPNetwork::startKeepAliveTimer()
{
    if (!running) return;

    keepAliveTimer.expires_after(std::chrono::seconds(4));
    keepAliveTimer.async_wait([this](const boost::system::error_code& error)
    {
        handleKeepAlive(error);
    });
}

void UDPNetwork::stopKeepAliveTimer()
{
    try
    {
        NETWORK_LOG_INFO("[Network] Stopping keep-alive timer");
        keepAliveTimer.cancel();
    }
    catch (const boost::system::system_error& e)
    {
        NETWORK_LOG_ERROR("[Network] Error cancelling keep-alive timer: {}", e.what());
    }
}

void UDPNetwork::handleKeepAlive(const boost::system::error_code& error)
{
    if (error == boost::asio::error::operation_aborted)
    {
        NETWORK_LOG_INFO("[Network] Keep-alive timer cancelled");
        return;
    }

    if (!running)
    {
        NETWORK_LOG_INFO("[Network] Network not running, cancelling keep-alive");
        return;
    }

    // TODO: REFACTOR FOR *1, FOR MULTIPLE PEERS
    NETWORK_LOG_INFO("[Network] Running keep-alive functionality");
    for (const auto& [publicIp, connectionInfo] : publicIpToPeerConnection)
    {
        sendHolePunchPacket(connectionInfo.getPeerEndpoint());
    }

    checkAllConnections();

    startKeepAliveTimer(); // Restart timer
}

// ! EXPECTS AN EMPTY PACKET / SPACE FOR THE HEADER
uint32_t UDPNetwork::attachCustomHeader(
    const std::shared_ptr<std::vector<uint8_t>>& packet,
    PacketType packetType,
    std::optional<uint32_t> seqOpt)
{
    /*
    * SMALL CUSTOM PROTOCOL HEADER
    */

    // Set magic number
    (*packet)[0] = (MAGIC_NUMBER >> 24) & 0xFF;
    (*packet)[1] = (MAGIC_NUMBER >> 16) & 0xFF;
    (*packet)[2] = (MAGIC_NUMBER >> 8) & 0xFF;
    (*packet)[3] = MAGIC_NUMBER & 0xFF;
    
    // Set protocol version
    (*packet)[4] = (PROTOCOL_VERSION >> 8) & 0xFF;
    (*packet)[5] = PROTOCOL_VERSION & 0xFF;
    
    // Set packet type
    (*packet)[6] = static_cast<uint8_t>(packetType);
    (*packet)[7] = 0;
    
    // Set sequence number
    uint32_t seq = seqOpt.value_or(nextSeqNumber++);
    (*packet)[8] = (seq >> 24) & 0xFF;
    (*packet)[9] = (seq >> 16) & 0xFF;
    (*packet)[10] = (seq >> 8) & 0xFF;
    (*packet)[11] = seq & 0xFF;

    return seq;
}

boost::asio::io_context& UDPNetwork::getIOContext()
{
    return ioContext;
}