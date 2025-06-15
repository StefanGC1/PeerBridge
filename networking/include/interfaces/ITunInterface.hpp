#pragma once

#include <functional>
#include <vector>
#include <string>
#include <cstdint>


class ITunInterface
{
public:
    virtual ~ITunInterface() = default;

    using PacketCallback = std::function<void(const std::vector<uint8_t>&)>;

    virtual bool initialize(const std::string&) = 0;

    virtual bool startPacketProcessing() = 0;
    virtual void stopPacketProcessing() = 0;
    virtual bool sendPacket(std::vector<uint8_t>) = 0;
    virtual void setPacketCallback(PacketCallback callback) = 0;

    virtual bool isRunning() const = 0;
    virtual void close() = 0;

    virtual std::string getNarrowAlias() const = 0;
};