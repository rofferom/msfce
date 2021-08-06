#include <assert.h>
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
    : MemComponent(MemComponentType::indirectRam),
      m_Wram(wram)
{
}

uint8_t IndirectWram::readU8(uint32_t addr)
{
    switch (addr) {
    case kRegisterWMDATA: {
        auto value = m_Wram->readU8(m_Address);
        m_Address++;
        m_Address &= 0x1ffff;
        return value;
    }

    // Open bus
    case kRegVMAIN:
    case kRegVMADDL:
    case kRegVMDATAL:
    case kRegVMDATAH:
        return m_Wram->readU8(m_Address);

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
        m_Address &= 0x1ffff;
        break;
    }

    case kRegisterWMADDL:
        m_Address &= 0x1FFF00;
        m_Address |= value;
        break;
 
    case kRegisterWMADDM:
        m_Address &= 0x1F00FF;
        m_Address |= value << 8;
        break;

    case kRegisterWMADDH:
        m_Address &= 0xFFFF;
        m_Address |= value << 16;
        m_Address &= 0x1ffff;
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
