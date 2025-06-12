#pragma once

#include "Logger.hpp"
#include <cstdint>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <map>

namespace NetworkConstants
{
inline constexpr const wchar_t* INTERFACE_NAME = L"PeerBridge";
inline constexpr const wchar_t* TUNNEL_TYPE = L"WINTUN";

inline constexpr char const* NET_MASK = "255.255.255.0";
inline constexpr char const* MULTICAST_SUBNET_RANGE = "224.0.0.0/4";

inline constexpr uint8_t START_IP_INDEX = 1;
inline constexpr uint8_t BASE_IP_INDEX = 0;

inline constexpr uint32_t BROADCAST_IP2 = 0xFFFFFFFF;
}

namespace utils
{
inline uint32_t ipToUint32(const std::string& ipAddress)
{
    uint32_t result = 0;
    
    // Parse IP address
    std::istringstream iss(ipAddress);
    std::string octet;
    int i = 0;
    
    while (std::getline(iss, octet, '.') && i < 4) {
        uint32_t value = std::stoi(octet);
        result = (result << 8) | (value & 0xFF);
        i++;
    }
    
    return result;
}

inline std::string uint32ToIp(uint32_t ipAddress)
{
    std::ostringstream result;
    
    for (int i = 0; i < 4; i++) {
        uint8_t octet = (ipAddress >> (8 * (3 - i))) & 0xFF;
        result << static_cast<int>(octet);
        if (i < 3) {
            result << ".";
        }
    }
    
    return result.str();
}

inline std::pair<std::string, std::string> splitIpPort(const std::string& ipPort)
{
    size_t colonPos = ipPort.find_last_of(':');
    
    if (colonPos == std::string::npos) {
        // No colon found, treat entire string as IP
        return {{}, {}};
    }
    
    std::string ip = ipPort.substr(0, colonPos);
    std::string port = ipPort.substr(colonPos + 1);
    
    return {ip, port};
}
// Map vIP -> (peer IP, peer port)
// Skip self or unavailable peers
inline std::map<uint32_t, std::pair<std::uint32_t, int>> parsePeerInfo(
    const std::vector<std::string>& peerInfo,
    const std::string& baseIPSpace,
    const int selfIndex)
{
    std::map<uint32_t, std::pair<std::uint32_t, int>> peerMap;
    uint32_t vIPIndex = NetworkConstants::START_IP_INDEX;

    for (size_t i = 0; i < peerInfo.size(); i++)
    {
        if (peerInfo[i] == "self")
        {
            SYSTEM_LOG_INFO("[Utils]: Skipping self index: {}", i);
            if (i != selfIndex)
            {
                SYSTEM_LOG_ERROR("[Utils]: Self index is not the same as the index of the self peer!");
                return {};
            }
            vIPIndex++;
            continue;
        }
        if (peerInfo[i] == "unavailable")
        {
            continue;
        }

        auto [ip, port] = splitIpPort(peerInfo[i]);
        if (ip.empty() || port.empty())
        {
            SYSTEM_LOG_ERROR("[Utils]: Invalid peer info: {}", peerInfo[i]);
            return {};
        }
        uint32_t virtualIp = utils::ipToUint32(baseIPSpace + std::to_string(vIPIndex));
        uint32_t ipInt = utils::ipToUint32(ip);
        peerMap[virtualIp] = {ipInt, std::stoi(port)};
        vIPIndex++;
    }

    return peerMap;
}
}