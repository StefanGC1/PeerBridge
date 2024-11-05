#ifndef TUN_INTERFACE_H
#define TUN_INTERFACE_H

#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <wintun.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#endif

class TunInterface {
public:
    TunInterface() = default;
    ~TunInterface() = default;

    // Initialize the TUN interface with a specified device name
    bool initialize(const std::string& deviceName);

    // Close and clean up the TUN interface
    void close();

private:
#ifdef _WIN32
    WINTUN_ADAPTER_HANDLE adapter = nullptr;  // Adapter handle for WinTun on Windows
#else
    int fd = -1;  // File descriptor for TUN on Unix-like systems
#endif
};

#endif // TUN_INTERFACE_H
