#pragma once

#include <memory>

#include "msfce/core/controller.h"
#include "memcomponent.h"

namespace msfce::core {

class ControllerPorts : public MemComponent {
public:
    ControllerPorts();
    ~ControllerPorts() = default;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    void setController1(const Controller& controller);

    void readController();

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    static uint16_t packController(const Controller& controller);

private:
    // Local controller state (set by Frontend)
    Controller m_Controller1_State;

    // Manual read
    bool m_Controller1_Strobe = false;
    uint16_t m_Controller1_ReadReg = 0xFFFF;

    // Automatic read
    uint16_t m_Joypad1Register = 0;
};

} // namespace msfce::core
