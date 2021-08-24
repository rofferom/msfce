#pragma once

#include <memory>

#include "memcomponent.h"

struct SnesController {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;

    bool start = false;
    bool select = false;

    bool l = false;
    bool r = false;

    bool y = false;
    bool x = false;
    bool b = false;
    bool a = false;
};

class ControllerPorts : public MemComponent {
public:
    ControllerPorts();
    ~ControllerPorts() = default;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    void setController1(const SnesController& controller);

    void readController();

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    static uint16_t packController(const SnesController& controller);

private:
    // Local controller state (set by Frontend)
    SnesController m_Controller1_State;

    // Manual read
    bool m_Controller1_Strobe = false;
    uint16_t m_Controller1_ReadReg = 0xFFFF;

    // Automatic read
    uint16_t m_Joypad1Register = 0;
};

