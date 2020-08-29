#pragma once

#include <memory>

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

class ControllerPorts {
public:
    ControllerPorts(const std::shared_ptr<SnesController>& snesController);
    ~ControllerPorts() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

    void readController();

private:
    std::shared_ptr<SnesController> m_Controller1;
    uint16_t m_Joypad1Register = 0;
};

