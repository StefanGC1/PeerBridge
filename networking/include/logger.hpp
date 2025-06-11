#pragma once
#include <iostream>
#include <sstream>
#include <mutex>
#include <string>
#include <quill/Logger.h>
#include <quill/LogMacros.h>

static bool shouldLogNetTraffic = false;

class TrafficLogLimiter
{
public:
    TrafficLogLimiter(double maxLogsPerSec)
    : capacity(maxLogsPerSec),
    tokens(capacity),
    rate(maxLogsPerSec),
    lastTime(std::chrono::steady_clock::now())
    {}

    bool tryLog() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> delta = now - lastTime;
        lastTime = now;
        tokens = std::min(capacity, tokens + (delta.count() * rate));

        if (tokens < 1.0)
            return false;

        tokens -= 1.0;
        return true;
    }

private:
    double capacity;
    double rate;
    double tokens;
    std::chrono::steady_clock::time_point lastTime;
    std::mutex mutex_;
};

void initLogging();
quill::Logger* sysLogger();
quill::Logger* netLogger();

inline TrafficLogLimiter& logLimiter()
{
    static TrafficLogLimiter limiter(6.0);
    return limiter;
}

#define SYSTEM_LOG_INFO(fmt, ...) QUILL_LOG_INFO(sysLogger(), fmt, ##__VA_ARGS__)
#define SYSTEM_LOG_WARNING(fmt, ...) QUILL_LOG_WARNING(sysLogger(), fmt, ##__VA_ARGS__)
#define SYSTEM_LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(sysLogger(), fmt, ##__VA_ARGS__)

// Too lazy to make this look better :/
#define NETWORK_TRAFFIC_LOG(fmt, ...)                               \
    do {                                                            \
        if (shouldLogNetTraffic && logLimiter().tryLog())           \
            QUILL_LOG_INFO(netLogger(), fmt, ##__VA_ARGS__);        \
    } while (0);

#define NETWORK_LOG_INFO(fmt, ...) QUILL_LOG_INFO(netLogger(), fmt, ##__VA_ARGS__)
#define NETWORK_LOG_WARNING(fmt, ...) QUILL_LOG_WARNING(netLogger(), fmt, ##__VA_ARGS__)
#define NETWORK_LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(netLogger(), fmt, ##__VA_ARGS__)

// Don't ask why it's here
inline void setShouldLogTraffic(bool shouldLog)
{
    shouldLogNetTraffic = shouldLog;
    if (shouldLogNetTraffic)
        SYSTEM_LOG_WARNING("[System] P2P Traffic will be logged to file, connection may be slower!");
}