#pragma once

#include "interfaces/ISystemStateManager.hpp"
#include <mutex>
#include <atomic>
#include <queue>
#include <string>

inline std::string toString(SystemState state)
{
    switch (state) 
    {
        case SystemState::IDLE: return "IDLE";
        case SystemState::CONNECTING: return "CONNECTING";
        case SystemState::CONNECTED: return "CONNECTED";
        case SystemState::SHUTTING_DOWN: return "SHUTTING_DOWN";
        default: return "UNKNOWN";
    }
}

// Manages the overall system state
class SystemStateManager : public ISystemStateManager
{
public:
    SystemStateManager();

    // System state management
    void setState(SystemState state) override;
    SystemState getState() const override;
    bool isInState(SystemState state) const override;

    // Event queue
    void queueEvent(const NetworkEventData& event) override;
    std::optional<NetworkEventData> getNextEvent() override;
    bool hasEvents() const override;
    
private:
    std::atomic<SystemState> currentState;

    std::queue<NetworkEventData> eventQueue;
    mutable std::mutex eventMutex;

    bool isValidTransition(SystemState from, SystemState to) const;
};
