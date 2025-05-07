cmake_minimum_required(VERSION 3.10)
project(BoostTestProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Detect the platform
if (WIN32)
    message(STATUS "Compiling on Windows")

    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WIN_ARCH "win64")
        message(STATUS "Using 64-bit libraries")
    else()
        set(WIN_ARCH "win32")
        message(STATUS "Using 32-bit libraries")
    endif()
    
    set(WINTUN_DIR "${CMAKE_SOURCE_DIR}/external/wintun")
    message(STATUS "Wintun Path: ${WINTUN_DIR}")
    include_directories("${WINTUN_DIR}/include")
elseif (UNIX)
    message(STATUS "Compiling on Linux")
    find_package(Threads REQUIRED)
endif()

# Set the path where Boost is installed
set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/external/boost")
set(BOOST_INCLUDEDIR "${BOOST_ROOT}/include/boost-1_87")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/lib")

# Set Libsodium Paths
set(SODIUM_ROOT "${CMAKE_SOURCE_DIR}/external/libsodium-${WIN_ARCH}")
set(SODIUM_INCLUDEDIR "${SODIUM_ROOT}/include")
set(SODIUM_LIBRARYDIR "${SODIUM_ROOT}/lib")

message(STATUS "Libsodium Include Path: ${SODIUM_INCLUDEDIR}")
message(STATUS "Libsodium Library Path: ${SODIUM_LIBRARYDIR}")

# Project specific source and header files
include_directories("${CMAKE_SOURCE_DIR}/include")
set(SOURCES
    src/main.cpp
    src/tun_interface.cpp
)

# Create executable
add_executable(BoostTestProject ${SOURCES})

# Include directories
target_include_directories(BoostTestProject PRIVATE
    ${BOOST_INCLUDEDIR}
    ${SODIUM_INCLUDEDIR})

# Link Boost libraries directory
target_link_directories(BoostTestProject PRIVATE ${BOOST_LIBRARYDIR})
target_link_directories(BoostTestProject PRIVATE ${SODIUM_LIBRARYDIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BOOST_SUFFIX "-mt-d")   # Use Debug version
else()
    set(BOOST_SUFFIX "-mt")     # Use Release version
endif()

# Link boost libraries
target_link_libraries(BoostTestProject PRIVATE
    "${BOOST_LIBRARYDIR}/libboost_system-mgw14${BOOST_SUFFIX}-x64-1_87.a"
    "${BOOST_LIBRARYDIR}/libboost_filesystem-mgw14${BOOST_SUFFIX}-x64-1_87.a"
    "${SODIUM_LIBRARYDIR}/libsodium.a"
    ws2_32
)

# WINTUN linking
if(WIN32)
    add_custom_command(TARGET BoostTestProject POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DIR}/bin/wintun.dll"
            $<TARGET_FILE_DIR:BoostTestProject>
    )
elseif(UNIX)
    target_link_libraries(BoostTestProject Threads::Threads)
endif()