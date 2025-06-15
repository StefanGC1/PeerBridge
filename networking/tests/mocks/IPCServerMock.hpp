#pragma once

#include <gmock/gmock.h>
#include "interfaces/IIPCServer.hpp"

class IPCServerMock : public IIPCServer
{
public:
    MOCK_METHOD(void, RunServer, (const std::string&), (override));
    MOCK_METHOD(void, ShutdownServer, (), (override));
    MOCK_METHOD(void, setGetStunInfoCallback, (GetStunInfoCallback), (override));
    MOCK_METHOD(void, setShutdownCallback, (ShutdownCallback), (override));
}; 