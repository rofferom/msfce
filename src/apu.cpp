#include <assert.h>
#include "log.h"
#include "registers.h"
#include "apu.h"

#define TAG "apu"

uint8_t Apu::readU8(size_t addr)
{
    switch (addr) {
    case kRegApuPort0:
        LOGD(TAG, "Read 0x%02X at 0x%06X", m_Port0.m_Apu, static_cast<uint32_t>(addr));
        return m_Port0.m_Apu;

    case kRegApuPort1:
        return m_Port1.m_Apu;

    case kRegApuPort2:
        return m_Port2.m_Cpu;

    case kRegApuPort3:
        return m_Port3.m_Cpu;

    default:
        break;
    }

    assert(false);

    return 0;
}

uint16_t Apu::readU16(size_t addr)
{
    switch (addr) {
    case kRegApuPort0:
        //LOGI(TAG, "Read16 0x%04X", static_cast<uint32_t>(addr));
        return m_Port0.m_Apu | (m_Port1.m_Apu << 8);

    case kRegApuPort1:
        return m_Port1.m_Apu | (m_Port2.m_Apu << 8);

    case kRegApuPort2:
        return m_Port2.m_Apu | (m_Port3.m_Apu << 8);;

    case kRegApuPort3:
    default:
        break;
    }

    assert(false);

    return 0;
}

void Apu::writeU8(size_t addr, uint8_t value)
{
    LOGD(TAG, "Write8 0x%02X at 0x%06X", value, static_cast<uint32_t>(addr));

    switch (addr) {
    case kRegApuPort0:
        if (m_State == State::init && value == 0) {
            m_Port0.m_Apu = 0xAA;
        } if (m_State == State::init && value == 0xCC && m_Port1.m_Cpu) {
            m_Port0.m_Apu = 0xCC;
            m_State = State::started;
        } else if (m_State == State::started) {
            if (value - m_Port0.m_Cpu > 1 && !m_Port1.m_Cpu) {
                LOGI(TAG, "Transfer end");
                m_State = State::init;
            }

            m_Port0.m_Cpu = value;
            m_Port0.m_Apu = value;
        }
        break;

    case kRegApuPort1:
        if (m_State == State::init && value == 0) {
            m_Port1.m_Apu = 0xBB;
        }  else {
            m_Port1.m_Cpu = value;
        }
        break;

    case kRegApuPort2:
        m_Port2.m_Cpu = value;
        break;

    case kRegApuPort3:
        m_Port3.m_Cpu = value;
        break;

    default:
        assert(false);
        break;
    }
}

void Apu::writeU16(size_t addr, uint16_t value)
{
    LOGD(TAG, "Write16 0x%04X at 0x%06X", value, static_cast<uint32_t>(addr));

    switch (addr) {
    case kRegApuPort0:
        if (m_State == State::started) {
            m_Port0.m_Cpu = value & 0xFF;
            m_Port1.m_Cpu = value >> 8;

            m_Port0.m_Apu = m_Port0.m_Cpu;
        }
        break;

    case kRegApuPort1:
        assert(false);
        break;

    case kRegApuPort2:
        m_Port2.m_Cpu = value & 0xFF;
        m_Port3.m_Cpu = value >> 8;
        break;

    case kRegApuPort3:
        assert(false);
        break;

    default:
        assert(false);
        break;
    }
}
