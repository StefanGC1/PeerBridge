#pragma once

#include <functional>
#include <vector>
#include <string>
#include <cstdint>
#include <map>
#include <boost/asio/ip/udp.hpp>
#include <sodium/crypto_box.h>

class IUDPNetwork {
public:
    using MessageCallback = std::function<void(const std::vector<uint8_t>)>;

    virtual ~IUDPNetwork() = default;

    virtual bool startListening(int port) = 0;
    virtual bool startConnection(
        uint32_t,
        const std::array<uint8_t, crypto_box_SECRETKEYBYTES>&,
        std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>>) = 0;
    
    virtual void stopConnection() = 0;
    virtual void shutdown() = 0;
    
    virtual bool isConnected() const = 0;
    
    virtual void processPacketFromTun(const std::vector<uint8_t>&) = 0;
    virtual bool sendMessage(
        const std::vector<uint8_t>& data,
        const boost::asio::ip::udp::endpoint& peerEndpoint,
        const std::array<uint8_t, crypto_box_BEFORENMBYTES>& sharedKey) = 0;
    virtual void setMessageCallback(MessageCallback callback) = 0;

    virtual boost::asio::io_context& getIOContext() = 0;
};
