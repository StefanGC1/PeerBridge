#pragma once

#include <string>
#include <guiddef.h>
#include <cstdint>
#include <vector>

class INetworkConfigManager
{
public:
    enum class RouteConfigApproach : uint8_t { GENERIC_ROUTE, FALLBACK_ROUTE_ALL, FAILED };

    struct SetupConfig
    {
        const std::string IP_SPACE;
        const GUID ADAPTER_GUID;

        // Provide a default implementation that can be overridden by derived classes
        static SetupConfig loadConfig()
        {
            SetupConfig cfg{
                "10.0.0.",  // IP-SPACE
                {}};        // GUID
            return cfg;
        }
    };

    struct ConnectionConfig
    {
        std::string selfVirtualIp;
        std::vector<std::string> peerVirtualIps;
    };

    virtual ~INetworkConfigManager() = default;

    virtual bool configureInterface(const ConnectionConfig&) = 0;
    virtual bool setupRouting(const ConnectionConfig&) = 0;
    virtual void setupFirewall() = 0;

    virtual void resetInterfaceConfiguration(const std::vector<std::string>&) = 0;
    virtual bool removeRouting(const std::vector<std::string>&) = 0;
    virtual void removeFirewall() = 0;

    virtual void setNarrowAlias(const std::string&) = 0;

    virtual SetupConfig getSetupConfig() = 0;
};
