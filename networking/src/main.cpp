#include "p2p_system.hpp"
#include "logger.hpp"
#include "ipc.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <signal.h>

// Global variables
static std::atomic<bool> g_running = true;
static std::unique_ptr<P2PSystem> g_system;
static std::unique_ptr<IPCServer> g_ipc_server;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    clog << "Signal received: " << signal << ". Shutting down." << std::endl;
    g_running = false;
    if (g_ipc_server) {
        g_ipc_server->ShutdownServer();
    }
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string username = "default_user";
    
    const std::string server_url = "wss://cfea-86-125-92-244.ngrok-free.app";
    int local_port = 0;
    
    g_system = std::make_unique<P2PSystem>();
    g_ipc_server = std::make_unique<IPCServer>(*g_system);
    
    g_system->setStatusCallback([](const std::string& status) {
        clog << "[P2P Status] " << status << std::endl;
    });
    
    g_system->setConnectionCallback([](bool connected, const std::string& peer) {
        if (connected) {
            clog << "[P2P System] Connected to " << peer << std::endl;
        } else {
            clog << "[P2P System] Disconnected from " << peer << std::endl;
        }
    });
    
    g_system->setConnectionRequestCallback([](const std::string& from) {
        clog << "[P2P Request] " << from << " wants to connect." << std::endl;
    });
    
    if (!g_system->initialize(server_url, username, local_port)) {
        std::cerr << "Failed to initialize the P2P system. Exiting." << std::endl;
        return 1;
    }
    clog << "P2P System initialized." << std::endl;

    // TODO sometime later: get available port from frontend
    std::string ipc_server_address = "0.0.0.0:50051";
    std::thread ipc_thread([&ipc_server_address](){
        g_ipc_server->RunServer(ipc_server_address);
        clog << "IPC Server thread finished." << std::endl;
    });

    clog << "PeerBridge C++ module running. Waiting for IPC commands or termination signal." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    clog << "Initiating shutdown..." << std::endl;

    if (g_ipc_server) {
        g_ipc_server->ShutdownServer();
    }
    
    if (ipc_thread.joinable()) {
        ipc_thread.join();
    }
    clog << "IPC thread joined." << std::endl;

    if (g_system) {
        g_system->disconnect();
    }
    clog << "P2P system cleaned up." << std::endl;
    
    clog << "Application exiting. Goodbye!" << std::endl;
    return 0;
}