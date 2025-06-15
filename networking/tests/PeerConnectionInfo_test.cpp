#include <gtest/gtest.h>
#include "NetworkingModule.hpp"
#include <thread>
#include <chrono>

class PeerConnectionInfoTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        info = PeerConnectionInfo();
    }

    PeerConnectionInfo info;
};

TEST_F(PeerConnectionInfoTest, TestInitialStateNotConnectedAndNotTimedOut)
{
    EXPECT_FALSE(info.isConnected());
    EXPECT_FALSE(info.hasTimedOut(0));
}

TEST_F(PeerConnectionInfoTest, TestTimeoutAfter1Second)
{
    info.setConnected(true);
    EXPECT_FALSE(info.hasTimedOut(1));
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_TRUE(info.hasTimedOut(1));
}

TEST_F(PeerConnectionInfoTest, TestNoTimeoutAfter1SecondWithUpdateActivity)
{
    info.setConnected(true);
    EXPECT_FALSE(info.hasTimedOut(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    info.updateActivity();
    EXPECT_FALSE(info.hasTimedOut(1));
}

TEST_F(PeerConnectionInfoTest, TestConnectedFlagChangesState)
{
    EXPECT_FALSE(info.isConnected());
    info.setConnected(true);
    EXPECT_TRUE(info.isConnected());
    info.setConnected(false);
    EXPECT_FALSE(info.isConnected());
} 