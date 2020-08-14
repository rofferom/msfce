#include <assert.h>
#include "apu.h"
#include "log.h"
#include "ppu.h"
#include "utils.h"
#include "registers.h"
#include "membus.h"

#define TAG "membus"

namespace {

constexpr size_t kBankSize = 0x10000;

bool isPpuAddress(size_t addr, uint8_t bank, uint16_t offset)
{
    if (bank != 0x00) {
        return false;
    }

    if (kRegPpuStart <= offset && offset <= kRegPpuEnd) {
        return true;
    }

    switch (offset) {
    case kRegNmitimen:
    case kRegRDNMI:
        return true;

    default:
        return false;
    }
}

} // anonymous namespace

const uint8_t* Membus::getReadPointer(size_t addr)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // WRAM
    if (kWramBankStart <= bank && bank <= kWramBankEnd) {
        // Direct access
        return &m_Wram[(bank - kWramBankStart) * kBankSize + offset];
    } else if (bank <= 0x3F && offset <= 0x1FFF) {
        // Bank 0x00 => 0x3F mirror
        return &m_Wram[offset];
    }

    // ROM
    if (offset >= 0x8000) {
        if (bank < kWramBankStart) {
            // Direct access. WRAM banks are hiding the ROM
            return &m_Rom[bank * 0x8000 + (offset - 0x8000)];
        } else if (bank < 0xFE) {
            assert(bank >= 0x80);
            // Two cases:
            // 1. 0x80 => 0xFD : Bank 0x00 => 0x7D mirror
            // 2. 0xFE => 0xFF : Direct access. Last part hidden by WRAM in 0x7E => 0x7F
            return &m_Rom[(bank - 0x80) * 0x8000 + (offset - 0x8000)];
        }
    }

    LOGC(TAG, "Unhandled read address 0x%06X", static_cast<uint32_t>(addr));
    assert(false);
    return nullptr;
}

uint8_t Membus::readU8(size_t addr)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        return m_Apu->readU8(addr);
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        return m_Ppu->readU8(addr);
    }

    // Joypad
    if (bank == 0 && (offset == kRegisterJoy1L || offset == kRegisterJoy1H)) {
        return 0;
    } else if (bank == 0 && (offset == kRegisterJoy2L || offset == kRegisterJoy2H)) {
        return 0;
    }

    auto ptr = getReadPointer(addr);
    return *ptr;
}

uint16_t Membus::readU16(size_t addr)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        return m_Apu->readU16(addr);
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        return m_Ppu->readU16(addr);
    }

    // Joypad
    if (bank == 0 && offset == kRegisterJoy1L) {
        return 0;
    } else if (bank == 0 && offset == kRegisterJoy2L) {
        return 0;
    }

    auto ptr = getReadPointer(addr);
    return *ptr | (*(ptr + 1) << 8);
}

uint32_t Membus::readU24(size_t addr)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        assert(false);
        return 0;
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        assert(false);
        return 0;
    }

    // Joypad
    if (bank == 0 && offset == kRegisterJoy1L) {
        assert(false);
        return 0;
    } else if (bank == 0 && offset == kRegisterJoy2L) {
        assert(false);
        return 0;
    }

    auto ptr = getReadPointer(addr);
    return *ptr | (*(ptr + 1) << 8) | (*(ptr + 2) << 16);
}

uint8_t* Membus::getWritePointer(size_t addr)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // WRAM
    if (kWramBankStart <= bank && bank <= kWramBankEnd) {
        // Direct access
        return &m_Wram[(bank - kWramBankStart) * kBankSize + offset];
    } else if (bank <= 0x3F && offset <= 0x1FFF) {
        // Bank 0x00 => 0x3F mirror
        return &m_Wram[offset];
    }

    // DMA/HDMA
    if (bank == 0 && (kRegDmaStart <= offset && offset <= kRegDmaEnd)) {
        LOGW(TAG , "DMA write (0x%04X) ignored", offset);
        return nullptr;
    }

    // Misc registers
    switch (addr) {
    case kRegNmitimen:
    case kRegisterMDMAEN:
    case kRegisterHDMAEN:
        LOGW(TAG , "%02X write ignored", static_cast<uint32_t>(addr));
        return nullptr;

    case kRegisterMemsel:
    case kRegisterJoyWr:
    case kRegisterJoyWrio:
    case kRegisterWRMPYA:
    case kRegisterWRMPYB:
    case kRegisterWRDIVL:
    case kRegisterWRDIVH:
    case kRegisterWRDIVB:
    case kRegisterHTIMEL:
    case kRegisterHTIMEH:
    case kRegisterVTIMEL:
    case kRegisterVTIMEH:
    case kRegisterWMADDL:
    case kRegisterWMADDM:
    case kRegisterWMADDH:
    case kRegisterWMDATA:
        assert(false);
        return nullptr;

    default:
        break;
    }

    LOGC(TAG, "Unhandled write address 0x%06X", static_cast<uint32_t>(addr));
    assert(false);
    return nullptr;
}

void Membus::writeU8(size_t addr, uint8_t value)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        m_Apu->writeU8(addr, value);
        return;
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        m_Ppu->writeU8(addr, value);
        return;
    }

    if (addr == kRegNmitimen) {
        LOGI(TAG, "Writting to kRegNmitimen: 0x%02X", value);
        return;
    } else if (addr == kRegSETINI) {
        LOGI(TAG, "Writting to kRegSETINI: 0x%02X", value);
    } else if (addr == kRegINIDISP) {
        LOGI(TAG, "Writting to kRegINIDISP: 0x%02X", value);
    }

    auto ptr = getWritePointer(addr);
    if (ptr) {
        *ptr = value;
    }
}

void Membus::writeU16(size_t addr, uint16_t value)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        m_Apu->writeU16(addr, value);
        return;
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        m_Ppu->writeU16(addr, value);
        return;
    }


    auto ptr = getWritePointer(addr);
    if (ptr) {
        *ptr = value & 0xFF;;
        *(ptr + 1) = value >> 8;
    }
}

void Membus::writeU24(size_t addr, uint32_t value)
{
    uint8_t bank = (addr >> 16) & 0xFF;
    uint16_t offset = addr & 0xFFFF;

    // APU
    if (bank == 0 && (kRegApuStart <= offset && offset <= kRegApuEnd)) {
        assert(false);
        return;
    }

    // PPU
    if (isPpuAddress(addr, bank, offset)) {
        assert(false);
        return;
    }


    auto ptr = getWritePointer(addr);
    if (ptr) {
        *ptr = value & 0xFF;;
        *(ptr + 1) = (value >> 8) & 0xFF;
        *(ptr + 2) = value >> 16;
    }
}

int Membus::plugRom(std::unique_ptr<std::vector<uint8_t>> rom)
{
    m_RomPtr = std::move(rom);
    m_Rom = m_RomPtr->data();

    return 0;
}

int Membus::plugApu(const std::shared_ptr<Apu>& spu)
{
    m_Apu = spu;

    return 0;
}

int Membus::plugPpu(const std::shared_ptr<Ppu>& ppu)
{
    m_Ppu = ppu;

    return 0;
}
