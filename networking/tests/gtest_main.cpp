#include "Logger.hpp"
#include "trace.hpp"
#include <gtest/gtest.h>

// Test gTest :)
TEST(BasicTest, OneEqualsOne)
{
    EXPECT_EQ(1, 1);
}

int main(int argc, char **argv)
{
    install_custom_terminate_handler();
    initTestLogging();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 