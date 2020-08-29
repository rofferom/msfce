#include <assert.h>
#include "log.h"
#include "registers.h"
#include "maths.h"

#define TAG "maths"

uint8_t Maths::readU8(size_t addr)
{
    switch (addr & 0xFFFF) {
    case kRegisterRDDIVL:
        return m_Quotient & 0xFF;

    case kRegisterRDDIVH:
        return m_Quotient >> 8;

    case kRegisterRDMPYL:
        return m_RemainderProduct & 0xFF;

    case kRegisterRDMPYH:
        return m_RemainderProduct >> 8;

    default:
        LOGW(TAG, "Unsupported readU8 %04X", static_cast<uint32_t>(addr));
        assert(false);
        break;
    }

    return 0;
}

uint16_t Maths::readU16(size_t addr)
{
    switch (addr & 0xFFFF) {
    case kRegisterRDDIVL:
        return m_Quotient;

    case kRegisterRDMPYL:
        return m_RemainderProduct;

    default:
        LOGW(TAG, "Unsupported readU16 %04X", static_cast<uint32_t>(addr));
        assert(false);
        break;
    }

    return 0;
}

void Maths::writeU8(size_t addr, uint8_t value)
{
    switch (addr & 0xFFFF) {
    case kRegisterWRMPYA:
        m_Multiplicand &= 0xFF00;
        m_Multiplicand |= value;
        break;

    case kRegisterWRMPYB:
        m_Multiplier = value;
        m_RemainderProduct = m_Multiplicand * m_Multiplier;
        m_Dividend = m_Multiplier;

        LOGD(TAG, "Multiply: 0x%04X * 0x%02X => 0x%04X",
             m_Multiplicand, m_Multiplier, m_RemainderProduct);
        break;

    case kRegisterWRDIVL:
        m_Dividend &= 0xFF00;
        m_Dividend |= value;
        break;

    case kRegisterWRDIVH:
        m_Dividend &= 0xFF;
        m_Dividend |= value << 8;
        break;

    case kRegisterWRDIVB:
        m_Divisor = value;

        if (m_Divisor) {
            m_Quotient = m_Dividend / m_Divisor;
            m_RemainderProduct = m_Dividend % m_Divisor;
        } else {
            m_Quotient = 0xFFFF;
            m_RemainderProduct = m_Dividend;
        }

        LOGD(TAG, "Divide: 0x%04X/0x%02X => Q:0x%04X, R:0x%04X",
             m_Dividend, m_Divisor, m_Quotient, m_RemainderProduct);
        break;

    default:
        LOGW(TAG, "Unsupported writeU8 %04X (value=%02X)", static_cast<uint32_t>(addr), value);
        assert(false);
        break;
    }
}

void Maths::writeU16(size_t addr, uint16_t value)
{
    size_t offset = addr & 0xFFFF;

    switch (offset) {
    case kRegisterWRDIVL:
        writeU8(addr, value & 0xFF);
        writeU8(addr + 1, value >> 8);
        break;

    default:
        LOGW(TAG, "Unsupported writeU16 %04X (value=%04X)", static_cast<uint32_t>(addr), value);
        assert(false);
        break;
    }
}
