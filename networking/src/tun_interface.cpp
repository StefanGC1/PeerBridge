#include "tun_interface.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <iostream>
#include <string>

typedef WINTUN_ADAPTER_HANDLE (*WintunCreateAdapterFunc)(const WCHAR*, const WCHAR*, GUID*);
typedef DWORD (*WintunGetLastErrorFunc)();

bool TunInterface::initialize(const std::string& deviceName) {
    // Load wintun.dll
    HMODULE wintunModule = LoadLibrary("wintun.dll");
    if (!wintunModule) {
        std::cerr << "Failed to load wintun.dll" << std::endl;
        return false;
    }

    // Retrieve the WintunCreateAdapter function
    WintunCreateAdapterFunc WintunCreateAdapter = 
        (WintunCreateAdapterFunc)GetProcAddress(wintunModule, "WintunCreateAdapter");
    if (!WintunCreateAdapter) {
        std::cerr << "Failed to load WintunCreateAdapter function" << std::endl;
        FreeLibrary(wintunModule);
        return false;
    }

    // Convert deviceName to wide string
    std::wstring wideDeviceName(deviceName.begin(), deviceName.end());
    WINTUN_ADAPTER_HANDLE adapter = WintunCreateAdapter(wideDeviceName.c_str(), L"Wintun", NULL);
    if (!adapter) {
        // Check for last error
        DWORD error = GetLastError();
        std::cerr << "Failed to create WinTun adapter. Error code: " << error << std::endl;

        FreeLibrary(wintunModule);
        return false;
    }

    this->adapter = adapter;
    std::cout << "WinTun adapter created successfully." << std::endl;

    int logic = 0;
    std::cin >> logic;

    FreeLibrary(wintunModule);
    free(adapter);

    return true;
}

#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

bool TunInterface::initialize(const std::string& deviceName) {
    int tun_fd = open("/dev/net/tun", O_RDWR);
    if (tun_fd < 0) {
        return false;
    }

    struct ifreq ifr = {};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, deviceName.c_str(), IFNAMSIZ);

    if (ioctl(tun_fd, TUNSETIFF, &ifr) < 0) {
        close(tun_fd);
        return false;
    }

    this->fd = tun_fd;
    return true;
}
#endif
