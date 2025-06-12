#pragma once

#include <string>
#include <guiddef.h>
#include <cstdint>
#include <iphlpapi.h>

class NetworkConfigManager
{
public:
    enum class RouteConfigApproach : uint8_t { GENERIC_ROUTE, FALLBACK_ROUTE_ALL, FAILED };

    struct SetupConfig
    {
        /* THIS VALUES WILL ONLY UPDATE WHEN CONNECTION IS NOT YET ESTABLISHED */

        const std::string IP_SPACE;
        const GUID ADAPTER_GUID;

        static SetupConfig loadConfig();
        // Following function is unused for now
        // TODO: Implement later
        // static bool saveConfig();
    };

    struct ConnectionConfig
    {
        std::string selfVirtualIp;
        std::vector<std::string> peerVirtualIps;
    };

    // TODO: Once config file implemented,
    // Do static FILE_PATH, staic SET_FILE_PATH or something similar

    NetworkConfigManager();

    bool configureInterface(const ConnectionConfig&);
    bool setupRouting(const ConnectionConfig&);
    void setupFirewall();

    void resetInterfaceConfiguration(const std::vector<std::string>&);
    bool removeRouting(const std::vector<std::string>&);
    void removeFirewall();

    void setNarrowAlias(const std::string&);

    SetupConfig getSetupConfig();

private:
    RouteConfigApproach routeApproach = RouteConfigApproach::GENERIC_ROUTE;
    std::string narrowAlias;
    SetupConfig setupConfig;

    bool executeNetshCommand(const std::string& command);

};