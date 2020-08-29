#include <assert.h>
#include "log.h"
#include "registers.h"
#include "apu.h"

#define TAG "apu"

uint8_t Apu::readU8(size_t addr)
{
    switch (addr) {
    case kRegApuPort0: {
        uint8_t value = m_Port0.m_Apu;

        // Ack for last kick command
        if (m_State == State::ending) {
            m_Port0.m_Apu = 0xAA;
            m_Port1.m_Apu = 0xBB;
            m_State = State::waiting;
        }

        return value;
    }

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
        return readU8(addr) | (readU8(addr + 1) << 8);

    case kRegApuPort1:
        return m_Port1.m_Apu | (m_Port2.m_Apu << 8);

    case kRegApuPort2:
        return m_Port2.m_Cpu | (m_Port3.m_Cpu << 8);;

    case kRegApuPort3:
    default:
        break;
    }

    assert(false);

    return 0;
}

void Apu::writeU8(size_t addr, uint8_t value)
{
    switch (addr) {
    case kRegApuPort0:
        if (m_State == State::waiting && value == 0xCC) {
            // Assume games already start with a transfer
            LOGD(TAG, "Transfer started");
            m_Port0.m_Apu = value;
            assert(m_Port1.m_Apu);
            m_State = State::waitingBlockBegin;
        } else if (m_State == State::waitingBlockBegin) {
            if (value == 0) {
                LOGD(TAG, "First value received");
                m_Port0.m_Apu = value;
                m_State = State::transfering;
            }
        } else if (m_State == State::transfering) {
            uint8_t delta = value - m_Port0.m_Apu;
            m_Port0.m_Apu = value;

            if (delta > 1) {
                LOGD(TAG, "Transfer block completed");

                if (m_Port1.m_Apu) {
                    LOGD(TAG, "Transfer new block");
                    m_State = State::waitingBlockBegin;
                } else {
                    LOGD(TAG, "Entry command received");
                    m_State = State::ending;
                }
            }
        }
        break;

    case kRegApuPort1:
        if (m_State != State::waiting) {
            m_Port1.m_Apu = value;
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
    switch (addr) {
    case kRegApuPort0:
    case kRegApuPort1:
    case kRegApuPort2:
    case kRegApuPort3:
        writeU8(addr + 1, value >> 8);
        writeU8(addr, value & 0xFF);
        break;

    default:
        assert(false);
        break;
    }
}
