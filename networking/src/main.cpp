#include <iostream>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>

#include "tun_interface.hpp"

#ifdef __cplusplus
extern "C"{
#endif

#include <sodium.h>

#ifdef __cplusplus
}
#endif

int main() {
    // Test Boost.Asio (basic IO context)
    try {
        boost::asio::io_context io_context;  // Use io_context instead of io_service
        std::cout << "Boost.Asio is working!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Boost.Asio error: " << e.what() << std::endl;
        return 1;
    }

    // Test Boost.Filesystem (current working directory)
    try {
        boost::filesystem::path currentPath = boost::filesystem::current_path();
        std::cout << "Current path: " << currentPath.string() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Boost.Filesystem error: " << e.what() << std::endl;
        return 1;
    }

    // Test Libsodium
    if (sodium_init() < 0) {
        std::cerr << "Libsodium initialization failed!" << std::endl;
        return 1;
    }
    std::cout << "Libsodium initialized successfully!" << std::endl;

    // Generate a random key
    unsigned char key[crypto_secretbox_KEYBYTES];
    randombytes_buf(key, sizeof key);
    std::cout << "Generated random Libsodium key successfully!" << std::endl;

    std::cout << "Boost.Asio, Boost.Filesystem, and Libsodium are all working!" << std::endl;

    TunInterface tunInterface;
    if (tunInterface.initialize("PeerBridge")) {
        std::cout << "WinTun adapter initialized successfully." << std::endl;
    } else {
        std::cerr << "Failed to initialize WinTun adapter." << std::endl;
    }

    return 0;
}
