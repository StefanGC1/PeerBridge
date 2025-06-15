#pragma once

#include "interfaces/IStun.hpp"
#include <string>
#include <optional>
#include <boost/asio.hpp>

class StunClient : public IStunClient
{
public:
    StunClient(const std::string& server = "stun.l.google.com", const std::string& port = "19302");
    
    // Get public IP and port
    std::optional<PublicAddress> discoverPublicAddress() override;
    
    // Set STUN server (possible custom configuration)
    void setStunServer(const std::string& server, const std::string& port = "19302") override;

    std::unique_ptr<boost::asio::ip::udp::socket> getSocket() override;
    boost::asio::io_context& getContext() override;

private:
    std::string stunServer;
    std::string stunPort;
    std::unique_ptr<boost::asio::ip::udp::socket> scoket;
    boost::asio::io_context ioContext;
};
