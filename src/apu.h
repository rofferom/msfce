#pragma once

#include <stdlib.h>
#include <stdint.h>

class Apu {
public:
    Apu() = default;
    ~Apu() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

private:
    enum class State {
        init,
        started,
        ending,
    };

    struct Port {
        uint8_t m_Apu;
        uint8_t m_Cpu;
    };

private:
    State m_State = State::init;

    Port m_Port0 { 0xAA, 0 };
    Port m_Port1 { 0xBB, 0 };
    Port m_Port2 {};
    Port m_Port3 {};
};
