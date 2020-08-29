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
    const std::shared_ptr<SnesController>& controller,
    const RegisterMapping& mapping)
{
    auto rawPtr = (reinterpret_cast<uint8_t *>(controller.get()) + mapping.offset);
    return *(reinterpret_cast<bool *>(rawPtr));
}

} // anonymous namespace

ControllerPorts::ControllerPorts(const std::shared_ptr<SnesController>& snesController)
    : m_Controller1(snesController)
{
}

uint8_t ControllerPorts::readU8(size_t addr)
{
    switch (addr) {
    case kRegisterJoy1L: {
        return m_Joypad1Register & 0xFF;
    }

    case kRegisterJoy1H: {
        return m_Joypad1Register >> 8;
    }

    case kRegisterJoy2L:
    case kRegisterJoy2H:
    case kRegisterJoyA:
    case kRegisterJoyB:
        return 0;

    default:
        LOGW(TAG, "Ignore ReadU8 at %06X", static_cast<uint32_t>(addr));
        assert(false);
        return 0;
    }
}

uint16_t ControllerPorts::readU16(size_t addr)
{
    assert(false);
    return 0;
}

void ControllerPorts::writeU8(size_t addr, uint8_t value)
{
    assert(false);
}

void ControllerPorts::writeU16(size_t addr, uint16_t value)
{
    assert(false);
}

void ControllerPorts::readController()
{
    m_Joypad1Register = 0;

    for (size_t i = 0; i < SIZEOF_ARRAY(s_ControllerRegisterMap); i++) {
        const auto& mapping = s_ControllerRegisterMap[i];

        if (controllerGetButton(m_Controller1, mapping)) {
            m_Joypad1Register |= (1 << mapping.bit);
        }
    }
}