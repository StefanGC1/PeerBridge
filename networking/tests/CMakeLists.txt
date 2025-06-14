# Test configuration
cmake_minimum_required(VERSION 3.10)

# Test executable
add_executable(PeerBridgeNet_tests
    gtest_main.cpp
    PeerConnectionInfo_test.cpp
    SystemStateManager_test.cpp
    UDPNetwork_test.cpp
    P2PSystem_test.cpp
)

# Include directories for tests
target_include_directories(PeerBridgeNet_tests PRIVATE
    ../include
    ./
    mocks/
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# Link libraries for tests
target_link_libraries(PeerBridgeNet_tests PRIVATE
    PeerBridgeNetLib
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    dbghelp
)

# Add coverage flags if enabled
if(ENABLE_COVERAGE)
    target_compile_options(PeerBridgeNet_tests PRIVATE --coverage -g -O0)
    target_link_libraries(PeerBridgeNet_tests PRIVATE --coverage)
endif()

# Add compiler flags for GTest
target_compile_options(PeerBridgeNet_tests PRIVATE ${GTEST_CFLAGS_OTHER})

# Register tests with CTest
add_test(NAME BasicTests COMMAND PeerBridgeNet_tests) 