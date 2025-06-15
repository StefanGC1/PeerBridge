#pragma once
#include <string>
#include <optional>
#include <boost/asio.hpp>

struct PublicAddress
{
    std::string ip;
    int port;
};

class IStunClient
{
public:
    virtual ~IStunClient() = default;

    virtual std::optional<PublicAddress> discoverPublicAddress() = 0;
    virtual void setStunServer(const std::string& server, const std::string& port) = 0;
    virtual std::unique_ptr<boost::asio::ip::udp::socket> getSocket() = 0;
    virtual boost::asio::io_context& getContext() = 0;
};