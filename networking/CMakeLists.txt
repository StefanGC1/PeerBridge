cmake_minimum_required(VERSION 3.10)
project(PeerBridgeNet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add test options
option(BUILD_TESTS "Build the tests" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

find_package(Boost REQUIRED COMPONENTS system filesystem thread stacktrace_windbg)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GRPC REQUIRED grpc++ grpc)
pkg_check_modules(PROTOBUF REQUIRED protobuf)
pkg_check_modules(QUILL REQUIRED quill)

# Add GTest for testing
if(BUILD_TESTS)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTEST REQUIRED gtest)
    pkg_check_modules(GMOCK REQUIRED gmock)
    enable_testing()
endif()

# Find protoc and grpc_cpp_plugin
find_program(PROTOC_EXECUTABLE NAMES protoc HINTS ENV Protobuf_ROOT PATHS ENV PATH ${PROTOBUF_PREFIX}/bin)
find_program(GRPC_CPP_PLUGIN_EXECUTABLE NAMES grpc_cpp_plugin HINTS ENV GRPC_ROOT PATHS ENV PATH ${GRPC_PREFIX}/bin)

if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc compiler not found. Please ensure it is in your PATH or set Protobuf_ROOT.")
endif()
message(STATUS "Found protoc: ${PROTOC_EXECUTABLE}")

if(NOT GRPC_CPP_PLUGIN_EXECUTABLE)
    message(FATAL_ERROR "grpc_cpp_plugin not found. Please ensure it is in your PATH or set GRPC_ROOT.")
endif()
message(STATUS "Found grpc_cpp_plugin: ${GRPC_CPP_PLUGIN_EXECUTABLE}")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSODIUM REQUIRED libsodium)

set(WINTUN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/wintun/include")
message(STATUS "wintun: ${WINTUN_INCLUDE_DIR}")
set(WINTUN_DLL_DIR "${CMAKE_SOURCE_DIR}/wintun/bin")

# Create proto directory
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/proto)

# Proto files configuration
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
get_filename_component(PROTO_DIR_ABS ${PROTO_DIR} ABSOLUTE) 

set(PROTO_FILES
    ${PROTO_DIR}/PeerBridge.proto
)

# Output directory for generated C++ files from .proto
set(GENERATED_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-sources/protobuf)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

set(GENERATED_PROTO_SRCS)

# Loop through each .proto file to define custom commands for C++ generation
foreach(PROTO_FILE_PATH ${PROTO_FILES})
    get_filename_component(PROTO_FILE_NAME_WE ${PROTO_FILE_PATH} NAME_WE) # e.g., "PeerBridgeNet"
    get_filename_component(PROTO_FILE_ABS_PATH ${PROTO_FILE_PATH} ABSOLUTE)

    set(PB_CC_FILE "${GENERATED_PROTO_DIR}/${PROTO_FILE_NAME_WE}.pb.cc")
    set(PB_H_FILE "${GENERATED_PROTO_DIR}/${PROTO_FILE_NAME_WE}.pb.h")
    set(GRPC_PB_CC_FILE "${GENERATED_PROTO_DIR}/${PROTO_FILE_NAME_WE}.grpc.pb.cc")
    set(GRPC_PB_H_FILE "${GENERATED_PROTO_DIR}/${PROTO_FILE_NAME_WE}.grpc.pb.h")

    list(APPEND GENERATED_PROTO_SRCS ${PB_CC_FILE} ${GRPC_PB_CC_FILE})

    add_custom_command(
        OUTPUT ${PB_CC_FILE} ${PB_H_FILE} ${GRPC_PB_CC_FILE} ${GRPC_PB_H_FILE}
        COMMAND ${PROTOC_EXECUTABLE}
            "--proto_path=${PROTO_DIR_ABS}"
            "--cpp_out=${GENERATED_PROTO_DIR}"
            "--grpc_out=${GENERATED_PROTO_DIR}"
            "--plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}"
            "${PROTO_FILE_ABS_PATH}"
        DEPENDS "${PROTO_FILE_ABS_PATH}" ${PROTOC_EXECUTABLE} ${GRPC_CPP_PLUGIN_EXECUTABLE}
        COMMENT "Generating C++ sources from ${PROTO_FILE_NAME_WE}.proto"
        VERBATIM
    )
endforeach()

message(STATUS "Generated Proto sources: ${GENERATED_PROTO_SRCS}")

include_directories(${WINTUN_INCLUDE_DIR})

# Create a library with all the core functionality (excluding main.cpp)
set(LIB_SOURCES
    src/NetworkingModule.cpp
    src/Stun.cpp
    src/P2PSystem.cpp
    src/TUNInterface.cpp
    src/Logger.cpp
    src/NetworkConfigManager.cpp
    src/SystemStateManager.cpp
    src/IPCServer.cpp
)

set(PROTO_SOURCES ${GENERATED_PROTO_SRCS})

# Create the library
add_library(PeerBridgeNetLib STATIC ${LIB_SOURCES} ${PROTO_SOURCES})

# Add coverage flags if enabled
if(ENABLE_COVERAGE)
    target_compile_options(PeerBridgeNetLib PUBLIC --coverage -g -O0 -fprofile-arcs -ftest-coverage)
    target_link_libraries(PeerBridgeNetLib PUBLIC --coverage)
endif()

# Include directories for the library
target_include_directories(PeerBridgeNetLib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    ${Boost_INCLUDE_DIRS}
    ${LIBSODIUM_INCLUDE_DIRS}
    ${PROTOBUF_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
    ${GENERATED_PROTO_DIR}
    ${WINTUN_INCLUDE_DIR}
)

# Link libraries for the library
target_link_libraries(PeerBridgeNetLib PUBLIC
    ${Boost_LIBRARIES}
    Boost::stacktrace_windbg
    dbghelp
    ${LIBSODIUM_LIBRARIES}
    ${GRPC_LIBRARIES}
    ${PROTOBUF_LIBRARIES}
    ws2_32
    mswsock
    crypt32
    iphlpapi
)

target_compile_definitions(PeerBridgeNetLib PUBLIC 
    IXWEBSOCKET_USE_TLS
    SOURCE_ROOT_DIR="${CMAKE_SOURCE_DIR}/src/"
)

# Create executable using the library
add_executable(PeerBridgeNet src/main.cpp)
target_link_libraries(PeerBridgeNet PRIVATE PeerBridgeNetLib)

# Setting elevation as required, this is only possible on windows
# TODO: Add handling for Linux
set(ADMIN_MANIFEST "${CMAKE_SOURCE_DIR}/peerbridge.manifest")
# 24 = RT_MANIFEST resource type
set(ADMIN_RC "${CMAKE_CURRENT_BINARY_DIR}/peerbridge_manifest.rc")
file(WRITE ${ADMIN_RC} "1 24 \"${ADMIN_MANIFEST}\"\n")
target_sources(PeerBridgeNet PRIVATE ${ADMIN_RC})

# All dependencies are now handled by the library

# Copy Wintun driver dll to build dir
add_custom_command(TARGET PeerBridgeNet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DLL_DIR}/wintun.dll"
            $<TARGET_FILE_DIR:PeerBridgeNet>
    )

#### POST-BUILD PACKAGING ####

option(ENABLE_DEP_COPY "Copy DLL dependencies to release folder and set CPACK config" OFF)

if(ENABLE_DEP_COPY)
    # Copy dependencies automatically
    add_custom_target(copy_deps ALL
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_EXE=$<TARGET_FILE:PeerBridgeNet>
            -P "${CMAKE_SOURCE_DIR}/copy_deps.cmake"
        BYPRODUCTS "${CMAKE_BINARY_DIR}/release/PeerBridgeNet.exe"
        VERBATIM
    )
    add_dependencies(copy_deps PeerBridgeNet)

    # Install step: include the release folder contents in the package
    # NOTE: This might not be necessary anymore
    install(DIRECTORY "${CMAKE_BINARY_DIR}/release/" DESTINATION .)

    # Package config (CPack ZIP)
    include(InstallRequiredSystemLibraries)
    set(CPACK_GENERATOR "ZIP")
    set(CPACK_PACKAGE_NAME "PeerBridgeNet")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/package")
    include(CPack)
endif()

# Add tests subdirectory
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()