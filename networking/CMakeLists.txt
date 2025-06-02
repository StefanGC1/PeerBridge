cmake_minimum_required(VERSION 3.10)
project(PeerBridgeNet)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(PkgConfig REQUIRED)

# Use pkg-config to find gRPC and Protobuf
# This is often more reliable in MSYS2/MinGW environments
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
# Add other gRPC dependencies if linker errors persist for them (e.g., absl_base, cares, etc.)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

# Explicitly find protoc and grpc_cpp_plugin to ensure full paths
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
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(ZLIB REQUIRED zlib)

set(NLOHMANN_INCLUDEDIR "${CMAKE_SOURCE_DIR}/external/nlohmann/include")

set(IXWS_ROOT "${CMAKE_SOURCE_DIR}/external/ixwebsocket")
set(IXWS_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/external/ixwebsocket/include")
set(IXWS_LIBRARY "${CMAKE_SOURCE_DIR}/external/ixwebsocket/lib/libixwebsocket.a")

set(WINTUN_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/wintun/include")
message(STATUS "wintun: ${WINTUN_INCLUDE_DIR}")
set(WINTUN_DLL_DIR "${CMAKE_SOURCE_DIR}/wintun/bin")

# Create proto directory if it doesn't exist
file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/proto)

# Proto files configuration
set(PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto)
get_filename_component(PROTO_DIR_ABS ${PROTO_DIR} ABSOLUTE) # Absolute path for protoc command

set(PROTO_FILES
    ${PROTO_DIR}/PeerBridge.proto
)

# Output directory for generated C++ files from .proto
set(GENERATED_PROTO_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated-sources/protobuf)
file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})

# Prepare lists for generated source files
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

include_directories(${IXWS_INCLUDE_DIRS})
link_directories(${IXWS_ROOT}/lib)

include_directories(${WINTUN_INCLUDE_DIR})

# Project source and header files
include_directories("${CMAKE_SOURCE_DIR}/include")
set(SOURCES
    src/main.cpp
    src/crypto.cpp
    src/networking.cpp
    src/stun.cpp
    src/p2p_system.cpp
    src/tun_interface.cpp
    src/ipc.cpp
)

set(PROTO_SOURCES ${GENERATED_PROTO_SRCS}) # Add the generated .pb.cc and .grpc.pb.cc files

# Create executable
add_executable(PeerBridgeNet ${SOURCES} ${PROTO_SOURCES})

# Setting elevation as required, this is only possible on windows
# TODO: Add handling for Linux
set(ADMIN_MANIFEST "${CMAKE_SOURCE_DIR}/peerbridge.manifest")
# 24 = RT_MANIFEST resource type
set(ADMIN_RC "${CMAKE_CURRENT_BINARY_DIR}/peerbridge_manifest.rc")
file(WRITE ${ADMIN_RC} "1 24 \"${ADMIN_MANIFEST}\"\n")
target_sources(PeerBridgeNet PRIVATE ${ADMIN_RC})

# Include directories
target_include_directories(PeerBridgeNet PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${LIBSODIUM_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
    ${NLOHMANN_INCLUDEDIR}
    ${PROTOBUF_INCLUDE_DIRS}  # From pkg_check_modules
    ${GRPC_INCLUDE_DIRS}      # From pkg_check_modules
    ${GENERATED_PROTO_DIR}    
)

# target_include_directories(PeerBridgeNet PRIVATE ${GENERATED_PROTO_DIR})# Add directory of generated headers to include path

# Link libraries
target_link_libraries(PeerBridgeNet PRIVATE
    ${Boost_LIBRARIES}
    ${LIBSODIUM_LIBRARIES}
    ${IXWS_LIBRARY}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${GRPC_LIBRARIES}         # From pkg_check_modules
    ${PROTOBUF_LIBRARIES}     # From pkg_check_modules
    ws2_32
    mswsock
    crypt32
    iphlpapi
)

# Copy Wintun driver dll to build dir
add_custom_command(TARGET PeerBridgeNet POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WINTUN_DLL_DIR}/wintun.dll"
            $<TARGET_FILE_DIR:PeerBridgeNet>
    )

target_compile_definitions(PeerBridgeNet PRIVATE IXWEBSOCKET_USE_TLS)

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

    # Copy generated proto files to release directory
    # TODO: Remove copying proto generated files as they are not needed for the .exe
    # add_custom_command(TARGET copy_deps POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
    #         "${GENERATED_PROTO_DIR}/" # Source directory for generated files (updated from PROTO_OUTPUT_DIR)
    #         "${CMAKE_BINARY_DIR}/release/proto_gen/" # Destination directory
    #     COMMENT "Copying generated proto files to release/proto_gen"
    # )

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