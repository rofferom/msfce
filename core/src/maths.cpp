#include <assert.h>
#include "msfce/core/log.h"
#include "registers.h"
#include "maths.h"

#define TAG "maths"

namespace msfce::core {

Maths::Maths() : MemComponent(MemComponentType::maths)
{
}

uint8_t Maths::readU8(uint32_t addr)
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
        LOGW(TAG, "Unsupported readU8 %04X", addr);
        assert(false);
        break;
    }

    return 0;
}

void Maths::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr & 0xFFFF) {
    case kRegisterWRMPYA:
        m_Multiplicand &= 0xFF00;
        m_Multiplicand |= value;
        break;

    case kRegisterWRMPYB:
        m_Multiplier = value;
        m_RemainderProduct = m_Multiplicand * m_Multiplier;
        m_Quotient = m_Multiplier;

        LOGD(
            TAG,
            "Multiply: 0x%04X * 0x%02X => 0x%04X",
            m_Multiplicand,
            m_Multiplier,
            m_RemainderProduct);
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

        LOGD(
            TAG,
            "Divide: 0x%04X/0x%02X => Q:0x%04X, R:0x%04X",
            m_Dividend,
            m_Divisor,
            m_Quotient,
            m_RemainderProduct);
        break;

    default:
        LOGW(TAG, "Unsupported writeU8 %04X (value=%02X)", addr, value);
        assert(false);
        break;
    }
}

void Maths::dumpToFile(FILE* f)
{
    fwrite(&m_Multiplicand, sizeof(m_Multiplicand), 1, f);
    fwrite(&m_Multiplier, sizeof(m_Multiplier), 1, f);
    fwrite(&m_Dividend, sizeof(m_Dividend), 1, f);
    fwrite(&m_Divisor, sizeof(m_Divisor), 1, f);
    fwrite(&m_Quotient, sizeof(m_Quotient), 1, f);
    fwrite(&m_RemainderProduct, sizeof(m_RemainderProduct), 1, f);
}

void Maths::loadFromFile(FILE* f)
{
    fread(&m_Multiplicand, sizeof(m_Multiplicand), 1, f);
    fread(&m_Multiplier, sizeof(m_Multiplier), 1, f);
    fread(&m_Dividend, sizeof(m_Dividend), 1, f);
    fread(&m_Divisor, sizeof(m_Divisor), 1, f);
    fread(&m_Quotient, sizeof(m_Quotient), 1, f);
    fread(&m_RemainderProduct, sizeof(m_RemainderProduct), 1, f);
}

} // namespace msfce::core
