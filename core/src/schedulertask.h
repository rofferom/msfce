#pragma once

#include <stdio.h>
#include <cstdint>

namespace msfce::core {

class SchedulerTask {
public:
    enum class State {
        idle,
        running,
    };

public:
    SchedulerTask() = default;
    virtual ~SchedulerTask() = default;

    State getState();

    void setIdle();

    void setNextRunCycle(uint64_t cycle);
    uint64_t getNextRunCycle();

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

    virtual int run() = 0;

private:
    State m_State = State::idle;
    uint64_t m_NextRunCycle = 0;
};

} // namespace msfce::core
