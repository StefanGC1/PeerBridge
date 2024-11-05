cmake_minimum_required(VERSION 3.15)
project(PeerBridgeNetworking)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the Boost root directory
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/third_party/boost")
# set(Boost_INCLUDE_DIRS "${BOOST_ROOT}/third_party/boost/include/boost")
set(CMAKE_PREFIX_PATH "${BOOST_ROOT}/lib/cmake")  # Specify where BoostConfig.cmake is located

# Find Boost (without specific components)
find_package(Boost REQUIRED)

# Include Boost headers
include_directories(${Boost_INCLUDE_DIRS})

# Link Boost libraries manually
set(Boost_SYSTEM_LIBRARY "${BOOST_ROOT}/lib/libboost_system-mgw14-mt-d-x64-1_86.a") # or .lib on Windows

# Platform-specific TUN/TAP setup
if(WIN32)
    # Windows-specific setup for WinTun
    set(WINTUN_DIR "${CMAKE_SOURCE_DIR}/third_party/wintun")
    include_directories(${WINTUN_DIR}/include)
    # add_library(wintun MODULE IMPORTED)
    # set_property(TARGET wintun PROPERTY IMPORTED_LOCATION "${WINTUN_DIR}/bin/wintun.dll")
elseif(UNIX)
    # Unix-like systems: Use native TUN interface
    find_package(Threads REQUIRED)
endif()

# Include libsodium
set(SODIUM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/libsodium/include")
set(SODIUM_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/libsodium/lib/libsodium.a")  # Adjust path if needed
include_directories(${SODIUM_INCLUDE_DIR})

# Check if libsodium library was found
if(NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "Could not find libsodium library")
endif()

# Define source files
include_directories("${CMAKE_SOURCE_DIR}/include")
set(SOURCES
    src/main.cpp
    src/tun_interface.cpp
    src/networking.cpp
    src/encryption.cpp
)

# Define executable
add_executable(PeerBridgeNetworking ${SOURCES})

# Link libraries
target_link_libraries(PeerBridgeNetworking
    ${Boost_SYSTEM_LIBRARY}  # Manually link Boost system library
    ${SODIUM_LIBRARY}
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(PeerBridgeNetworking ws2_32)  # Add Winsock2 for Windows networking
    add_custom_command(TARGET PeerBridgeNetworking POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DIR}/bin/wintun.dll"
            $<TARGET_FILE_DIR:PeerBridgeNetworking>
    )
elseif(UNIX)
    target_link_libraries(PeerBridgeNetworking Threads::Threads)
endif()
