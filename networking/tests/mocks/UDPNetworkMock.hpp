#pragma once

#include <gmock/gmock.h>
#include "interfaces/INetworkModule.hpp"

class UDPNetworkMock : public IUDPNetwork
{
public:
    MOCK_METHOD(bool, startListening, (int port), (override));
    MOCK_METHOD(
        bool,
        startConnection,
            (uint32_t,
            (const std::array<uint8_t, crypto_box_SECRETKEYBYTES>&),
            (std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>>)),
    (override));
    MOCK_METHOD(void, stopConnection, (), (override));
    MOCK_METHOD(void, shutdown, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(void, processPacketFromTun, (const std::vector<uint8_t>&), (override));
    MOCK_METHOD(
        bool,
        sendMessage,
            (const std::vector<uint8_t>& data,
            (const boost::asio::ip::udp::endpoint& peerEndpoint),
            (const std::array<uint8_t, crypto_box_BEFORENMBYTES>& sharedKey)),
        (override));
    MOCK_METHOD(void, setMessageCallback, (MessageCallback callback), (override));
    MOCK_METHOD(boost::asio::io_context&, getIOContext, (), (override));
}; 