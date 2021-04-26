#include "log.h"
#include "registers.h"
#include "wram.h"

#define TAG "wram"

Wram::Wram() : BufferMemComponent(
    MemComponentType::ram,
    kWramSize)
{
}

IndirectWram::IndirectWram(const std::shared_ptr<Wram>& wram)
    : MemComponent(MemComponentType::indirectRam)
{
}

uint8_t IndirectWram::readU8(uint32_t addr)
{
    auto ret = m_Wram->readU8(m_Address);

    switch (addr) {
    case kRegisterWMDATA: {
        auto value = m_Wram->readU8(m_Address);
        m_Address++;
        return value;
    }

    default:
        LOGW(TAG, "Ignore ReadU8 at %06X", addr);
        return 0;
    }
}

void IndirectWram::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr) {
    case kRegisterWMDATA: {
        m_Wram->writeU8(m_Address, value);
        m_Address++;
        break;
    }

    case kRegisterWMADDL:
        m_Address &= 0xFFFF00;
        m_Address |= value;
        break;
 
    case kRegisterWMADDM:
        m_Address &= 0xFF00FF;
        m_Address |= value << 8;
        break;

    case kRegisterWMADDH:
        m_Address &= 0xFFFF;
        m_Address |= value << 16;
        break;

    default:
        LOGW(TAG, "Ignore WriteU8 %02X at %06X", value, addr);
        break;
    }
}

void IndirectWram::dumpToFile(FILE* f)
{
    fwrite(&m_Address, sizeof(m_Address), 1, f);
}

void IndirectWram::loadFromFile(FILE* f)
{
    fread(&m_Address, sizeof(m_Address), 1, f);
}
