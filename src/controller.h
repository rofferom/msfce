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
    SnesController m_Controller1;
    uint16_t m_Joypad1Register = 0;
};

