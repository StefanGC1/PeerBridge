#pragma once

#include <gmock/gmock.h>
#include "interfaces/ISystemStateManager.hpp"

class SystemStateManagerMock : public ISystemStateManager
{
public:
    MOCK_METHOD(void, setState, (SystemState state), (override));
    MOCK_METHOD(SystemState, getState, (), (const, override));
    MOCK_METHOD(bool, isInState, (SystemState state), (const, override));
    MOCK_METHOD(void, queueEvent, (const NetworkEventData& event), (override));
    MOCK_METHOD(std::optional<NetworkEventData>, getNextEvent, (), (override));
    MOCK_METHOD(bool, hasEvents, (), (const, override));
}; 