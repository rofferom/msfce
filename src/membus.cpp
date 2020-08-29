#include <assert.h>
#include "apu.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "maths.h"
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
    case kRegTIMEUP:
    case kRegVTIMEL:
    case kRegVTIMEH:
    case kRegVMAIN:
    case kRegVMADDL:
    case kRegVMADDH:
    case kRegVMDATAL:
    case kRegVMDATAH:
    case kRegRDVRAML:
    case kRegRDVRAMH:
    case kRegOAMADDL:
    case kRegOAMADDH:
    case kRegOAMDATA:
        return true;

    default:
        return false;
    }
}

bool isMathsAddress(size_t addr, uint8_t bank, uint16_t offset)
{
    if (bank > 0x3F) {
        return false;
    }

    switch (offset) {
    case kRegisterWRMPYA:
    case kRegisterWRMPYB:
    case kRegisterWRDIVL:
    case kRegisterWRDIVH:
    case kRegisterWRDIVB:
    case kRegisterRDDIVL:
    case kRegisterRDDIVH:
    case kRegisterRDMPYL:
    case kRegisterRDMPYH:
        return true;

    default:
        return false;
    }
}

bool isDmaAddress(size_t addr, uint8_t bank, uint16_t offset)
{
    if (bank != 0x00) {
        return false;
    }

    if (kRegDmaStart <= offset && offset <= kRegDmaEnd) {
        return true;
    }

    switch (offset) {
    case kRegisterMDMAEN:
    case kRegisterHDMAEN:
        return true;

    default:
        return false;
    }
}

bool isJoypadAddress(size_t addr, uint8_t bank, uint16_t offset)
{
    if (bank != 0x00) {
        return false;
    }

    if (kRegDmaStart <= offset && offset <= kRegDmaEnd) {
        return true;
    }

    switch (offset) {
    case kRegisterJoy1L:
    case kRegisterJoy1H:
    case kRegisterJoy2L:
    case kRegisterJoy2H:
    case kRegisterJoyA:
    case kRegisterJoyB:
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
        //LOGI(TAG, "Read from 0x%06X", (uint32_t) ((bank - kWramBankStart) * kBankSize + offset));
        return &m_Wram[(bank - kWramBankStart) * kBankSize + offset];
    } else if (bank <= 0x3F && offset <= 0x1FFF) {
        // Bank 0x00 => 0x3F mirror
        //LOGI(TAG, "Read from 0x%06X", offset);
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        return m_Maths->readU8(addr);
    }

    // DMA
    if (isDmaAddress(addr, bank, offset)) {
        return m_Dma->readU8(addr);
    }

    // Joypad
    if (isJoypadAddress(addr, bank, offset)) {
        return m_ControllerPorts->readU8(addr);
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        return m_Maths->readU16(addr);
    }

    // DMA
    if (isDmaAddress(addr, bank, offset)) {
        return m_Dma->readU16(addr);
    }

    // Joypad
    if (isJoypadAddress(addr, bank, offset)) {
        return m_ControllerPorts->readU16(addr);
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        assert(false);
        return 0;
    }

    // DMA
    if (isDmaAddress(addr, bank, offset)) {
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
    case kRegisterJoyWr:
        LOGW(TAG , "%02X write ignored", static_cast<uint32_t>(addr));
        return nullptr;

    case kRegisterMemsel:
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
        LOGC(TAG, "Unhandled write address 0x%06X", static_cast<uint32_t>(addr));
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        m_Maths->writeU8(addr, value);
        return;
    }

    // Dma
    if (isDmaAddress(addr, bank, offset)) {
        m_Dma->writeU8(addr, value);
        return;
    }

    // Joypad
    if (isJoypadAddress(addr, bank, offset)) {
        m_ControllerPorts->writeU8(addr, value);
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        m_Maths->writeU16(addr, value);
        return;
    }

    // Dma
    if (isDmaAddress(addr, bank, offset)) {
        m_Dma->writeU16(addr, value);
        return;
    }

    // Joypad
    if (isJoypadAddress(addr, bank, offset)) {
        m_ControllerPorts->writeU16(addr, value);
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

    // Maths
    if (isMathsAddress(addr, bank, offset)) {
        assert(false);
        return;
    }

    // Dma
    if (isDmaAddress(addr, bank, offset)) {
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

int Membus::plugApu(const std::shared_ptr<Apu>& spu)
{
    m_Apu = spu;

    return 0;
}

int Membus::plugControllerPorts(const std::shared_ptr<ControllerPorts>& controllerPorts)
{
    m_ControllerPorts = controllerPorts;

    return 0;
}

int Membus::plugDma(const std::shared_ptr<Dma>& dma)
{
    m_Dma = dma;

    return 0;
}

int Membus::plugMaths(const std::shared_ptr<Maths>& maths)
{
    m_Maths = maths;

    return 0;
}

int Membus::plugPpu(const std::shared_ptr<Ppu>& ppu)
{
    m_Ppu = ppu;

    return 0;
}

int Membus::plugRom(std::unique_ptr<std::vector<uint8_t>> rom)
{
    m_RomPtr = std::move(rom);
    m_Rom = m_RomPtr->data();

    return 0;
}

void Membus::dump()
{
    FILE* f = fopen("dump_wram.bin", "wb");
    assert(f);
    fwrite(m_Wram, 1, sizeof(m_Wram), f);
    fclose(f);
}
