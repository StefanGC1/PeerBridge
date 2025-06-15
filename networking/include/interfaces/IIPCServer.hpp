#pragma once

#include <string>
#include <functional>
#include <sodium.h>

class IIPCServer
{
public:
    struct StunInfo
    {
        std::string publicIp;
        int publicPort;
        std::array<uint8_t, crypto_box_PUBLICKEYBYTES>& publicKey;
    };
    using GetStunInfoCallback = std::function<StunInfo()>;
    using ShutdownCallback = std::function<void(bool)>;

    virtual ~IIPCServer() = default;

    virtual void RunServer(const std::string&) = 0;
    virtual void ShutdownServer() = 0;

    virtual void setGetStunInfoCallback(GetStunInfoCallback) = 0;
    virtual void setShutdownCallback(ShutdownCallback) = 0;
};
