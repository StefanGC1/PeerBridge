#include "p2p_system.hpp"
#include "logger.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <signal.h>

// Global variables
static std::atomic<bool> g_running = true;
static std::unique_ptr<P2PSystem> g_system;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    clog << "Signal received: " << signal << ". Shutting down." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string username = "default_user";
    
    const std::string server_url = "wss://cfea-86-125-92-244.ngrok-free.app";
    int local_port = 0;
    
    g_system = std::make_unique<P2PSystem>();
    
    g_system->setStatusCallback([](const std::string& status) {
        clog << "[P2P Status] " << status << std::endl;
        // I will strangle myself with this fix
        if (status == "Disconnected") {
            g_running = false;
        }
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

    clog << "PeerBridge C++ module running. Waiting for IPC commands or termination signal." << std::endl;
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    clog << "Initiating shutdown..." << std::endl;

    if (g_system) {
        g_system->disconnect(); // This will also stop the IPC server
    }
    
    clog << "Application exiting. Goodbye!" << std::endl;
    return 0;
}