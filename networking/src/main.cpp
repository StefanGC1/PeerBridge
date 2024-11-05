#include "tun_interface.hpp"
#include <iostream>

int main() {
    // Initialize and start your application here
    TunInterface tunInterface;
    if (tunInterface.initialize("PeerBridge")) {
        std::cout << "WinTun adapter initialized successfully." << std::endl;
    } else {
        std::cerr << "Failed to initialize WinTun adapter." << std::endl;
    }
    return 0;
}