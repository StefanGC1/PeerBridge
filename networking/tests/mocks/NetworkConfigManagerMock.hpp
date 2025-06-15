#pragma once

#include <gmock/gmock.h>
#include "interfaces/INetworkConfigManager.hpp"

class NetworkConfigManagerMock : public INetworkConfigManager
{
public:
    MOCK_METHOD(bool, configureInterface, (const ConnectionConfig&), (override));
    MOCK_METHOD(bool, setupRouting, (const ConnectionConfig&), (override));
    MOCK_METHOD(void, setupFirewall, (), (override));
    MOCK_METHOD(void, resetInterfaceConfiguration, (const std::vector<std::string>&), (override));
    MOCK_METHOD(bool, removeRouting, (const std::vector<std::string>&), (override));
    MOCK_METHOD(void, removeFirewall, (), (override));
    MOCK_METHOD(void, setNarrowAlias, (const std::string&), (override));
    MOCK_METHOD(SetupConfig, getSetupConfig, (), (override));
}; 