#include <assert.h>
#include "log.h"
#include "registers.h"
#include "utils.h"
#include "controller.h"

#define TAG "controller"

namespace {

struct RegisterMapping {
    off64_t offset;
    int bit;
};

static const RegisterMapping s_ControllerRegisterMap[] = {
    { offsetof(SnesController, r),      4 },
    { offsetof(SnesController, l),      5 },
    { offsetof(SnesController, x),      6 },
    { offsetof(SnesController, a),      7 },
    { offsetof(SnesController, right),  8 },
    { offsetof(SnesController, left),   9 },
    { offsetof(SnesController, down),   10 },
    { offsetof(SnesController, up),     11 },
    { offsetof(SnesController, start),  12 },
    { offsetof(SnesController, select), 13 },
    { offsetof(SnesController, y),      14 },
    { offsetof(SnesController, b),      15 },
};

static bool controllerGetButton(
    const SnesController* controller,
    const RegisterMapping& mapping)
{
    auto rawPtr = reinterpret_cast<const uint8_t *>(controller) + mapping.offset;
    return *(reinterpret_cast<const bool *>(rawPtr));
}

} // anonymous namespace

ControllerPorts::ControllerPorts()
    : MemComponent(MemComponentType::joypads)
{
}

uint8_t ControllerPorts::readU8(uint32_t addr)
{
    switch (addr) {
    case kRegisterJoy1L: {
        return m_Joypad1Register & 0xFF;
    }

    case kRegisterJoy1H: {
        return m_Joypad1Register >> 8;
    }
    case kRegisterJoyA: {
        uint8_t value = (m_Controller1_ReadReg >> 15) & 1;
        m_Controller1_ReadReg = (m_Controller1_ReadReg << 1) | 1;
        return value;
    }

    case kRegisterJoy2L:
    case kRegisterJoy2H:
    case kRegisterJoyB:
    case kRegisterJoy3L:
    case kRegisterJoy3H:
    case kRegisterJoy4L:
    case kRegisterJoy4H:
        return 0;

    default:
        LOGW(TAG, "Ignore ReadU8 at %06X", static_cast<uint32_t>(addr));
        assert(false);
        return 0;
    }
}

void ControllerPorts::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr) {
    case kRegisterJoyWr:
        // Avoid to fill read register if sequence is incorrect
        if (m_Controller1_Strobe == value) {
            return;
        }

        m_Controller1_Strobe = value;

        // Read controller
        if (m_Controller1_Strobe == 1) {
            m_Controller1_ReadReg = packController(m_Controller1_State);
        }
        break;

    case kRegisterJoyWrio:
        break;

    default:
        LOGW(TAG, "Ignore WriteU8 %02X at %06X", value, addr);
        assert(false);
        break;
    }

}

void ControllerPorts::setController1(const SnesController& controller)
{
    m_Controller1_State = controller;
}

void ControllerPorts::readController()
{
    m_Joypad1Register = packController(m_Controller1_State);
}

uint16_t ControllerPorts::packController(const SnesController& controller)
{
    uint16_t value = 0; // Already contains the standard controler ID (0b0000)

    for (size_t i = 0; i < SIZEOF_ARRAY(s_ControllerRegisterMap); i++) {
        const auto& mapping = s_ControllerRegisterMap[i];

        if (controllerGetButton(&controller, mapping)) {
            value |= (1 << mapping.bit);
        }
    }

    return value;
}

void ControllerPorts::dumpToFile(FILE* f)
{
    fwrite(&m_Controller1_State, sizeof(m_Controller1_State), 1, f);
    fwrite(&m_Joypad1Register, sizeof(m_Joypad1Register), 1, f);
    fwrite(&m_Controller1_Strobe, sizeof(m_Controller1_Strobe), 1, f);
    fwrite(&m_Controller1_ReadReg, sizeof(m_Controller1_ReadReg), 1, f);
}

void ControllerPorts::loadFromFile(FILE* f)
{
    fread(&m_Controller1_State, sizeof(m_Controller1_State), 1, f);
    fread(&m_Joypad1Register, sizeof(m_Joypad1Register), 1, f);
    fread(&m_Controller1_Strobe, sizeof(m_Controller1_Strobe), 1, f);
    fread(&m_Controller1_ReadReg, sizeof(m_Controller1_ReadReg), 1, f);
}
