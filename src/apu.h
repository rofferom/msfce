#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "memcomponent.h"

class Apu : public MemComponent {
public:
    Apu();
    ~Apu() = default;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

private:
    enum class State {
        waiting,
        waitingBlockBegin,
        transfering,
        ending,
    };

    struct Port {
        uint8_t m_Apu;
        uint8_t m_Cpu;
    };

private:
    State m_State = State::waiting;

    Port m_Port0 { 0xAA, 0 };
    Port m_Port1 { 0xBB, 0 };
    Port m_Port2 {};
    Port m_Port3 {};
};
