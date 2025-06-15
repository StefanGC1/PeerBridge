#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <boost/asio.hpp>
#include <thread>
#include <memory>

#include "Utils.hpp"
#include "P2PSystem.hpp"
#include "mocks/IPCServerMock.hpp"
#include "mocks/UDPNetworkMock.hpp"
#include "mocks/TunInterfaceMock.hpp"
#include "mocks/SystemStateManagerMock.hpp"
#include "mocks/NetworkConfigManagerMock.hpp"
#include "mocks/StunClientMock.hpp" 

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::StrictMock;
using ::testing::NiceMock;

class P2PSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        p2pSystem = std::make_unique<P2PSystem>();
        p2pSystem->shouldRunMonitorThread = false;

        stateManagerMock = std::make_shared<NiceMock<SystemStateManagerMock>>();
        networkConfigManagerMock = std::make_shared<NiceMock<NetworkConfigManagerMock>>();
        stunClientMock = std::make_unique<NiceMock<StunClientMock>>();
        tunInterfaceMock = std::make_unique<NiceMock<TunInterfaceMock>>();
        udpNetworkMock = std::make_unique<NiceMock<UDPNetworkMock>>();
        ipcServerMock = std::make_unique<NiceMock<IPCServerMock>>();
        
        ioContext = std::make_unique<boost::asio::io_context>();
        workGuard = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
            ioContext->get_executor());

        ON_CALL(*udpNetworkMock, getIOContext())
            .WillByDefault(::testing::ReturnRef(*ioContext));
    }

    void TearDown() override
    {
        p2pSystem.reset();

        if (ioContext) {
            ioContext->stop();
        }

        workGuard.reset();

        if (ioContextThread.joinable()) {
            ioContextThread.join();
        }
        stateManagerMock.reset();
        networkConfigManagerMock.reset();
    }

    void startIOContext()
    {
        ioContextThread = std::thread([this]() {
            ioContext->run();
        });
    }

    void injectMocks()
    {
        p2pSystem->stateManagerLRef() = stateManagerMock;
        p2pSystem->networkConfigLRef() = networkConfigManagerMock;
        p2pSystem->stunClientLRef() = std::move(stunClientMock);
        p2pSystem->tunInterfaceLRef() = std::move(tunInterfaceMock);
        p2pSystem->networkModuleLRef() = std::move(udpNetworkMock);
        p2pSystem->ipcServerLRef() = std::move(ipcServerMock);
    }

    std::unique_ptr<P2PSystem> p2pSystem;
    
    std::shared_ptr<NiceMock<SystemStateManagerMock>> stateManagerMock;
    std::shared_ptr<NiceMock<NetworkConfigManagerMock>> networkConfigManagerMock;
    std::unique_ptr<NiceMock<StunClientMock>> stunClientMock;
    std::unique_ptr<NiceMock<TunInterfaceMock>> tunInterfaceMock;
    std::unique_ptr<NiceMock<UDPNetworkMock>> udpNetworkMock;
    std::unique_ptr<NiceMock<IPCServerMock>> ipcServerMock;
    
    std::unique_ptr<boost::asio::io_context> ioContext;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuard;
    std::thread ioContextThread;
};

TEST_F(P2PSystemTest, TestInitializeSuccess)
{
    PublicAddress testPublicAddress{"192.168.1.100", 12345};
    std::string testNarrowAlias = "PeerBridge-TEST";
    
    // Start the IO context thread
    startIOContext();
    
    {
        ::testing::InSequence seq;
        EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE)).Times(1);
        EXPECT_CALL(*stateManagerMock, setState(SystemState::SHUTTING_DOWN)).Times(::testing::AnyNumber());
    }
    
    EXPECT_CALL(*stunClientMock, setStunServer("stun.l.google.com", "19302"))
        .Times(1);
    
    EXPECT_CALL(*stunClientMock, discoverPublicAddress())
        .WillOnce(Return(std::make_optional(testPublicAddress)));
    
    EXPECT_CALL(*ipcServerMock, setGetStunInfoCallback(_))
        .Times(1);
    
    EXPECT_CALL(*ipcServerMock, setShutdownCallback(_))
        .Times(1);
    
    EXPECT_CALL(*tunInterfaceMock, initialize("PeerBridge"))
        .WillOnce(Return(true));
    
    EXPECT_CALL(*tunInterfaceMock, setPacketCallback(_))
        .Times(1);
    
    EXPECT_CALL(*tunInterfaceMock, getNarrowAlias())
        .WillOnce(Return(testNarrowAlias));
    
    EXPECT_CALL(*networkConfigManagerMock, setNarrowAlias(testNarrowAlias))
        .Times(1);
    
    EXPECT_CALL(*udpNetworkMock, setMessageCallback(_))
        .Times(1);
    
    EXPECT_CALL(*udpNetworkMock, startListening(0))
        .WillOnce(Return(true));
    
    injectMocks();
    
    bool result = p2pSystem->initialize();
    
    EXPECT_TRUE(result);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(P2PSystemTest, TestInitializeFailsOnStunDiscovery)
{
    {
        ::testing::InSequence seq;
        EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE)).Times(1);
        EXPECT_CALL(*stateManagerMock, setState(SystemState::SHUTTING_DOWN)).Times(::testing::AnyNumber());
    }
    
    EXPECT_CALL(*stunClientMock, setStunServer("stun.l.google.com", "19302"))
        .Times(1);
    
    EXPECT_CALL(*stunClientMock, discoverPublicAddress())
        .WillOnce(Return(std::nullopt));
    
    injectMocks();
    
    bool result = p2pSystem->initialize();
    
    EXPECT_FALSE(result);
}

TEST_F(P2PSystemTest, InitializeFailsOnTunInterfaceInit)
{
    PublicAddress testPublicAddress{"192.168.1.100", 12345};
    
    {
        ::testing::InSequence seq;
        EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE)).Times(1);
        EXPECT_CALL(*stateManagerMock, setState(SystemState::SHUTTING_DOWN)).Times(::testing::AnyNumber());
    }
    
    EXPECT_CALL(*stunClientMock, setStunServer("stun.l.google.com", "19302"))
        .Times(1);
    
    EXPECT_CALL(*stunClientMock, discoverPublicAddress())
        .WillOnce(Return(std::make_optional(testPublicAddress)));
    
    EXPECT_CALL(*ipcServerMock, setGetStunInfoCallback(_))
        .Times(1);
    
    EXPECT_CALL(*ipcServerMock, setShutdownCallback(_))
        .Times(1);
    
    EXPECT_CALL(*tunInterfaceMock, initialize("PeerBridge"))
        .WillOnce(Return(false));
    
    injectMocks();
    
    bool result = p2pSystem->initialize();
    
    EXPECT_FALSE(result);
}

TEST_F(P2PSystemTest, TestInitializeFailsOnNetworkStartListening)
{
    PublicAddress testPublicAddress{"192.168.1.100", 12345};
    std::string testNarrowAlias = "PeerBridge-TEST";
    
    {
        ::testing::InSequence seq;
        EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE)).Times(1);
        EXPECT_CALL(*stateManagerMock, setState(SystemState::SHUTTING_DOWN)).Times(::testing::AnyNumber());
    }
    
    EXPECT_CALL(*stunClientMock, setStunServer("stun.l.google.com", "19302"))
        .Times(1);
    
    EXPECT_CALL(*stunClientMock, discoverPublicAddress())
        .WillOnce(Return(std::make_optional(testPublicAddress)));
    
    EXPECT_CALL(*ipcServerMock, setGetStunInfoCallback(_))
        .Times(1);
    
    EXPECT_CALL(*ipcServerMock, setShutdownCallback(_))
        .Times(1);
    
    EXPECT_CALL(*tunInterfaceMock, initialize("PeerBridge"))
        .WillOnce(Return(true));
    
    EXPECT_CALL(*tunInterfaceMock, setPacketCallback(_))
        .Times(1);
    
    EXPECT_CALL(*tunInterfaceMock, getNarrowAlias())
        .WillOnce(Return(testNarrowAlias));
    
    EXPECT_CALL(*networkConfigManagerMock, setNarrowAlias(testNarrowAlias))
        .Times(1);
    
    EXPECT_CALL(*udpNetworkMock, setMessageCallback(_))
        .Times(1);
    
    EXPECT_CALL(*udpNetworkMock, startListening(0))
        .WillOnce(Return(false));
    
    injectMocks();
    
    bool result = p2pSystem->initialize();
    
    EXPECT_FALSE(result);
}

TEST_F(P2PSystemTest, TestHandleInitializeConnection)
{
    ON_CALL(*stateManagerMock, getState())
        .WillByDefault(Return(SystemState::IDLE));
    EXPECT_CALL(*stateManagerMock, getState()).Times(::testing::AtLeast(1));

    EXPECT_CALL(*stateManagerMock, setState(SystemState::CONNECTING)).Times(1);

    INetworkConfigManager::SetupConfig cfg{"10.0.0.", {}};
    ON_CALL(*networkConfigManagerMock, getSetupConfig()).WillByDefault(Return(cfg));

    int selfIndex = 0;
    std::map<uint32_t, std::pair<std::pair<std::uint32_t, int>, std::array<uint8_t, crypto_box_PUBLICKEYBYTES>>> peerMap;

    uint32_t peerVirtualIp = utils::ipToUint32("10.0.0.2");
    peerMap[peerVirtualIp] = {{utils::ipToUint32("192.168.1.100"), 12345}, {}};

    EXPECT_CALL(*udpNetworkMock,
        startConnection(utils::ipToUint32("10.0.0.1"), _, peerMap))
    .WillOnce(Return(true));

    NetworkEventData::SelfIndexAndPeerMap idxAndMap{selfIndex, peerMap};
    NetworkEventData event(NetworkEvent::INITIALIZE_CONNECTION, idxAndMap);

    injectMocks();

    p2pSystem->handleNetworkEvent(event);
    ioContext->poll();
}

TEST_F(P2PSystemTest, TestHandlePeerConnected)
{
    ON_CALL(*stateManagerMock, getState())
        .WillByDefault(Return(SystemState::CONNECTING));

    EXPECT_CALL(*stateManagerMock, getState()).Times(::testing::AtLeast(1));

    ON_CALL(*udpNetworkMock, isConnected())
        .WillByDefault(Return(true));

    EXPECT_CALL(*udpNetworkMock, isConnected()).Times(::testing::AtLeast(1));
    EXPECT_CALL(*networkConfigManagerMock, configureInterface(_))
        .WillOnce(Return(true));
    EXPECT_CALL(*tunInterfaceMock, startPacketProcessing())
        .WillOnce(Return(true));

    EXPECT_CALL(*stateManagerMock, setState(SystemState::CONNECTED));

    injectMocks();
    p2pSystem->handleNetworkEvent({NetworkEvent::PEER_CONNECTED});
}

TEST_F(P2PSystemTest, TestHandleAllPeersDisconnected)
{
    ON_CALL(*stateManagerMock, getState())
        .WillByDefault(Return(SystemState::CONNECTED));
    EXPECT_CALL(*stateManagerMock, getState()).Times(::testing::AtLeast(1));

    ON_CALL(*tunInterfaceMock, isRunning()).WillByDefault(Return(true));

    EXPECT_CALL(*udpNetworkMock, stopConnection());
    EXPECT_CALL(*tunInterfaceMock, stopPacketProcessing());
    EXPECT_CALL(*networkConfigManagerMock, resetInterfaceConfiguration(_));

    EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE));

    injectMocks();
    p2pSystem->handleNetworkEvent({NetworkEvent::ALL_PEERS_DISCONNECTED});

    ioContext->poll();
}

TEST_F(P2PSystemTest, TestHandleDisconnectAllRequested)
{
    ON_CALL(*stateManagerMock, getState())
        .WillByDefault(Return(SystemState::CONNECTED));
    EXPECT_CALL(*stateManagerMock, getState()).Times(::testing::AtLeast(1));

    ON_CALL(*tunInterfaceMock, isRunning()).WillByDefault(Return(true));

    EXPECT_CALL(*udpNetworkMock, stopConnection());
    EXPECT_CALL(*tunInterfaceMock, stopPacketProcessing());

    EXPECT_CALL(*stateManagerMock, setState(SystemState::IDLE));

    injectMocks();
    p2pSystem->handleNetworkEvent({NetworkEvent::DISCONNECT_ALL_REQUESTED});
    ioContext->poll();
}

TEST_F(P2PSystemTest, TestHandleShutdownRequested)
{
    ON_CALL(*stateManagerMock, getState())
        .WillByDefault(Return(SystemState::CONNECTED));
    ON_CALL(*tunInterfaceMock, isRunning()).WillByDefault(Return(true));
    EXPECT_CALL(*stateManagerMock, getState()).Times(::testing::AtLeast(1));

    EXPECT_CALL(*stateManagerMock, setState(SystemState::SHUTTING_DOWN));
    EXPECT_CALL(*udpNetworkMock, shutdown());
    EXPECT_CALL(*tunInterfaceMock, stopPacketProcessing());
    EXPECT_CALL(*networkConfigManagerMock, resetInterfaceConfiguration(_));
    EXPECT_CALL(*tunInterfaceMock, close());
    EXPECT_CALL(*ipcServerMock, ShutdownServer());

    injectMocks();
    p2pSystem->setRunning(true);
    p2pSystem->handleNetworkEvent({NetworkEvent::SHUTDOWN_REQUESTED});
    ioContext->poll();
}

TEST_F(P2PSystemTest, TestMonitorLoopProcessesQueuedEvents)
{
    auto concreteStateManager = std::make_shared<SystemStateManager>();
    injectMocks();
    p2pSystem->stateManagerLRef() = concreteStateManager;

    concreteStateManager->queueEvent({NetworkEvent::SHUTDOWN_REQUESTED});
    p2pSystem->setRunning(true);
    p2pSystem->monitorLoop();

    EXPECT_TRUE(concreteStateManager->isInState(SystemState::SHUTTING_DOWN));
}