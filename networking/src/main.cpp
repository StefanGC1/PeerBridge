#include "P2PSystem.hpp"
#include "Logger.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <signal.h>
#include <csignal>
#include <boost/stacktrace.hpp>

// Global variables
static std::atomic<bool> g_running = true;
static std::unique_ptr<P2PSystem> p2pSystem;

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    g_running = false;
}

static std::string stackTraceToString()
{
    std::ostringstream oss;
    oss << boost::stacktrace::stacktrace();
    return oss.str();
}

static void onTerminate()
{
    // if there’s an active exception, try to get its what()
    if (auto eptr = std::current_exception())
    {
        try { std::rethrow_exception(eptr); }
        catch (std::exception const& ex)
        {
            SYSTEM_LOG_ERROR("Unhandled exception: {}", ex.what());
        }
        catch (...)
        {
            SYSTEM_LOG_ERROR("Unhandled non-std exception");
        }
    }
    else
    {
        SYSTEM_LOG_ERROR("Terminate called without an exception");
    }
    SYSTEM_LOG_ERROR("Stack trace:\n{}", stackTraceToString());
    std::_Exit(EXIT_FAILURE);  // immediate exit
}

// Called on fatal signals
static void onSignal(int sig)
{
    SYSTEM_LOG_ERROR("Received signal: {}", sig);
    SYSTEM_LOG_ERROR("Stack trace:\n{}", stackTraceToString());
    std::_Exit(EXIT_FAILURE);
}

int main(int argc, char* argv[])
{
    // Init logging
    initLogging();
    // Setup signal handlers
    std::set_terminate(onTerminate);

    signal(SIGINT, signalHandler);
    std::signal(SIGSEGV, onSignal);
    std::signal(SIGABRT, onSignal);
    std::signal(SIGFPE, onSignal);
    std::signal(SIGILL, onSignal);
    std::signal(SIGTERM, onSignal);
    
    // TODO: Disable network traffic logging in prod
    setShouldLogTraffic(true);

    SYSTEM_LOG_INFO("Starting P2P System application...");
    int localPort = 0; // Let system automatically choose a port
    p2pSystem = std::make_unique<P2PSystem>();
    
    // Initialize the application
    if (!p2pSystem->initialize(localPort))
    {
        SYSTEM_LOG_ERROR("Failed to initialize the application. Exiting.");
        return 1;
    }
    
    SYSTEM_LOG_INFO("P2P System initialized successfully. Main thread going to sleep.");
    // Main loop (status updates, etc.)
    while (g_running)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup - use full shutdown at program exit
    p2pSystem->shutdown();
    
    SYSTEM_LOG_INFO("Application exiting. Goodbye!");
    return 0;
}