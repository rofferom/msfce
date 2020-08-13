#include <assert.h>
#include "apu.h"
#include "log.h"
#include "utils.h"
#include "membus.h"

#define TAG "membus"

constexpr size_t kBankSize = 0x10000;

// PPU registers
constexpr size_t kRegPpuStart = 0x2100;
constexpr size_t kRegPpuEnd = 0x213F;

constexpr size_t kRegNmitimen = 0x4200;

// APU registers
constexpr size_t kRegApuStart = 0x2140;
constexpr size_t kRegApuEnd = 0x2143;

// WRAM registers
constexpr size_t kWramBankStart = 0x7E;
constexpr size_t kWramBankEnd = 0x7F;

constexpr size_t kRegisterWMDATA = 0x2180;
constexpr size_t kRegisterWMADDL = 0x2181;
constexpr size_t kRegisterWMADDM = 0x2182;
constexpr size_t kRegisterWMADDH = 0x2183;

// Joypad registers
constexpr size_t kRegisterJoyWr = 0x4016;
constexpr size_t kRegisterJoyWrio = 0x4201;

// Maths registers
constexpr size_t kRegisterWRMPYA = 0x4202;
constexpr size_t kRegisterWRMPYB = 0x4203;

constexpr size_t kRegisterWRDIVL = 0x4204;
constexpr size_t kRegisterWRDIVH = 0x4205;
constexpr size_t kRegisterWRDIVB = 0x4206;

constexpr size_t kRegisterRDDIVL = 0x4214;
constexpr size_t kRegisterRDDIVH = 0x4215;

constexpr size_t kRegisterRDMPYL = 0x4216;
constexpr size_t kRegisterRDMPYH = 0x4217;

// DMA/HDMA registers
constexpr size_t kRegisterMDMAEN = 0x420B;
constexpr size_t kRegisterHDMAEN = 0x420C;

constexpr size_t kRegDmaStart = 0x4300;
constexpr size_t kRegDmaEnd = 0x437F;


// Misc Registers
constexpr size_t kRegisterHTIMEL = 0x4207;
constexpr size_t kRegisterHTIMEH = 0x4208;
constexpr size_t kRegisterVTIMEL = 0x4209;
constexpr size_t kRegisterVTIMEH = 0x420A;
constexpr size_t kRegisterMemsel = 0x420D;

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

    // PPU
    if (bank == 0 && (kRegPpuStart <= offset && offset <= kRegPpuEnd)) {
        LOGW(TAG , "PPU write (0x%04X) ignored", offset);
        return nullptr;
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
        m_Apu->writeU16(addr, value);
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
