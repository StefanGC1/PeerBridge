#pragma once

#include <gmock/gmock.h>
#include "interfaces/IStun.hpp"

class StunClientMock : public IStunClient
{
public:
    MOCK_METHOD(std::optional<PublicAddress>, discoverPublicAddress, (), (override));
    MOCK_METHOD(void, setStunServer, (const std::string& server, const std::string& port), (override));
    MOCK_METHOD(std::unique_ptr<boost::asio::ip::udp::socket>, getSocket, (), (override));
    MOCK_METHOD(boost::asio::io_context&, getContext, (), (override));
}; 