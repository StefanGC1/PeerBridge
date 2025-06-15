#include <gtest/gtest.h>
#include "SystemStateManager.hpp"
#include "Logger.hpp"

class SystemStateManagerTest : public ::testing::Test
{
protected:
    SystemStateManager stateManager;
};

TEST_F(SystemStateManagerTest, TestInitialStateIdle)
{
    EXPECT_TRUE(stateManager.isInState(SystemState::IDLE));
}

TEST_F(SystemStateManagerTest, TestValidStateTransitions)
{
    stateManager.setState(SystemState::CONNECTING);
    EXPECT_TRUE(stateManager.isInState(SystemState::CONNECTING));

    stateManager.setState(SystemState::CONNECTED);
    EXPECT_TRUE(stateManager.isInState(SystemState::CONNECTED));

    stateManager.setState(SystemState::IDLE);
    EXPECT_TRUE(stateManager.isInState(SystemState::IDLE));

    stateManager.setState(SystemState::SHUTTING_DOWN);
    EXPECT_TRUE(stateManager.isInState(SystemState::SHUTTING_DOWN));
}

TEST_F(SystemStateManagerTest, TestInvalidTransitionIgnoredConnectedToConnecting)
{
    stateManager.setState(SystemState::CONNECTING);
    EXPECT_TRUE(stateManager.isInState(SystemState::CONNECTING));

    stateManager.setState(SystemState::CONNECTED);
    EXPECT_TRUE(stateManager.isInState(SystemState::CONNECTED));

    stateManager.setState(SystemState::CONNECTING);
    EXPECT_TRUE(stateManager.isInState(SystemState::CONNECTED));
}

TEST_F(SystemStateManagerTest, TestEventQueueCorrectOrdering)
{
    NetworkEventData ev1(NetworkEvent::PEER_CONNECTED);
    NetworkEventData ev2(NetworkEvent::SHUTDOWN_REQUESTED);

    stateManager.queueEvent(ev1);
    stateManager.queueEvent(ev2);

    ASSERT_TRUE(stateManager.hasEvents());
    auto deq1 = stateManager.getNextEvent();
    ASSERT_TRUE(deq1);
    EXPECT_EQ(deq1->event, NetworkEvent::PEER_CONNECTED);

    ASSERT_TRUE(stateManager.hasEvents());
    auto deq2 = stateManager.getNextEvent();
    ASSERT_TRUE(deq2);
    EXPECT_EQ(deq2->event, NetworkEvent::SHUTDOWN_REQUESTED);

    EXPECT_FALSE(stateManager.hasEvents());
} 