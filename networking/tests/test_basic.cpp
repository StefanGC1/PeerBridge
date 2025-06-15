#include <gtest/gtest.h>

// Basic test to verify that the testing framework is working
TEST(BasicTest, OneEqualsOne) {
    EXPECT_EQ(1, 1);
}

// Test runner main function
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 