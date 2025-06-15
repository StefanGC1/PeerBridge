#pragma once

#include "interfaces/INetworkConfigManager.hpp"
#include <string>
#include <guiddef.h>
#include <cstdint>
#include <iphlpapi.h>

class NetworkConfigManager : public INetworkConfigManager
{
public:
    NetworkConfigManager();

    bool configureInterface(const ConnectionConfig&) override;
    bool setupRouting(const ConnectionConfig&) override;
    void setupFirewall() override;

    void resetInterfaceConfiguration(const std::vector<std::string>&) override;
    bool removeRouting(const std::vector<std::string>&) override;
    void removeFirewall() override;

    void setNarrowAlias(const std::string&) override;

    SetupConfig getSetupConfig() override;

private:
    RouteConfigApproach routeApproach = RouteConfigApproach::GENERIC_ROUTE;
    std::string narrowAlias;
    SetupConfig setupConfig;

    bool executeNetshCommand(const std::string& command);
};
