#define PB_UNIT_TESTING 1
#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include "NetworkingModule.hpp"
#include "Utils.hpp"

class UDPNetworkTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        socket = std::make_unique<boost::asio::ip::udp::socket>(ioContext);
        // open UDP v4 but we don't bind
        socket->open(boost::asio::ip::udp::v4());
        stateManager = std::make_shared<SystemStateManager>();
        networkConfigManager = std::make_shared<NetworkConfigManager>();
        udpNetwork = std::make_unique<UDPNetwork>(std::move(socket), ioContext, stateManager, networkConfigManager);
    }

    boost::asio::io_context ioContext;
    std::unique_ptr<boost::asio::ip::udp::socket> socket;
    std::shared_ptr<SystemStateManager> stateManager;
    std::shared_ptr<NetworkConfigManager> networkConfigManager;
    std::unique_ptr<UDPNetwork> udpNetwork;
};

TEST_F(UDPNetworkTest, TestAttachCustomHeaderProducesMagicAndSeq)
{
    std::vector<uint8_t> payload{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0A, 0xAA};
    std::shared_ptr<std::vector<uint8_t>> sp = std::make_shared<std::vector<uint8_t>>(payload);
    uint32_t seq = udpNetwork->testAttachHeader(sp, UDPNetwork::PacketType::MESSAGE, 42);
    std::vector<uint8_t>& testPayload = *sp;

    EXPECT_EQ(seq, 42u);
    uint32_t magic =
        (static_cast<uint32_t>(testPayload[0]) << 24) |
        (static_cast<uint32_t>(testPayload[1]) << 16) |
        (static_cast<uint32_t>(testPayload[2]) << 8)  |
        static_cast<uint32_t>(testPayload[3]);
    EXPECT_EQ(magic, static_cast<uint32_t>(0x12345678));
    uint8_t pktType = *reinterpret_cast<uint8_t*>(&testPayload[6]);
    EXPECT_EQ(pktType, static_cast<uint8_t>(UDPNetwork::PacketType::MESSAGE));

    EXPECT_EQ(testPayload[12], 0x0A);
    EXPECT_EQ(testPayload[13], 0xAA);
}

TEST_F(UDPNetworkTest, TestStartConnectionSharedKeyInvalid_PeerMapsEmpty)
{
    uint32_t selfIp = utils::ipToUint32("10.0.0.1");
    uint32_t peerVirt = utils::ipToUint32("10.0.0.2");
    uint32_t peerPub = utils::ipToUint32("192.168.1.100");
    int peerPort = 12345;
    std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>> peerMap;
    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> dummyPubKey{};
    peerMap[peerVirt] = {{peerPub, peerPort}, dummyPubKey};

    std::array<uint8_t, crypto_box_SECRETKEYBYTES> dummySec{};

    ASSERT_TRUE(udpNetwork->startConnection(selfIp, dummySec, peerMap));

    const auto& virtMap = udpNetwork->testVirtualToPublic();
    const auto& pubMap = udpNetwork->testPublicToPeer();
    EXPECT_TRUE(virtMap.empty());
    EXPECT_TRUE(pubMap.empty());
}

TEST_F(UDPNetworkTest, TestStartConnectionSharedKeyValid_PeerMapsPopulated)
{
    uint32_t selfIp = utils::ipToUint32("10.0.0.1");
    uint32_t peerVirt = utils::ipToUint32("10.0.0.2");
    uint32_t peerPub = utils::ipToUint32("192.168.1.100");
    int peerPort = 12345;
    std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>> peerMap;
    
    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> dummyPeerPubKey{};
    std::array<uint8_t, crypto_box_SECRETKEYBYTES> dummyPeerSec{};
    crypto_box_keypair(dummyPeerPubKey.data(), dummyPeerSec.data());
    peerMap[peerVirt] = {{peerPub, peerPort}, dummyPeerPubKey};

    std::array<uint8_t, crypto_box_PUBLICKEYBYTES> dummySelfPubKey{};
    std::array<uint8_t, crypto_box_SECRETKEYBYTES> dummySelfSec{};
    crypto_box_keypair(dummySelfPubKey.data(), dummySelfSec.data());

    ASSERT_TRUE(udpNetwork->startConnection(selfIp, dummySelfSec, peerMap));

    const auto& virtMap = udpNetwork->testVirtualToPublic();
    ASSERT_EQ(virtMap.size(), 1u);
    EXPECT_EQ(virtMap.at(peerVirt).first, peerPub);
    EXPECT_EQ(virtMap.at(peerVirt).second, peerPort);
} 