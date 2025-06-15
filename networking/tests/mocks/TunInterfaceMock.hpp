#pragma once

#include <gmock/gmock.h>
#include "interfaces/ITunInterface.hpp"

class TunInterfaceMock : public ITunInterface
{
public:
    MOCK_METHOD(bool, initialize, (const std::string&), (override));
    MOCK_METHOD(bool, startPacketProcessing, (), (override));
    MOCK_METHOD(void, stopPacketProcessing, (), (override));
    MOCK_METHOD(bool, sendPacket, (std::vector<uint8_t>), (override));
    MOCK_METHOD(void, setPacketCallback, (PacketCallback callback), (override));
    MOCK_METHOD(bool, isRunning, (), (const, override));
    MOCK_METHOD(void, close, (), (override));
    MOCK_METHOD(std::string, getNarrowAlias, (), (const, override));
}; 