#include <assert.h>
#include "msfce/core/log.h"
#include "registers.h"
#include "timings.h"
#include "membus.h"

#define TAG "membus"

namespace {

constexpr uint32_t kComponentAccessR = (1 << 0);
constexpr uint32_t kComponentAccessW = (1 << 1);
constexpr uint32_t kComponentAccessRW = kComponentAccessR | kComponentAccessW;

} // anonymous namespace

namespace msfce::core {

Membus::Membus(AddressingType addrType, bool fastRom) : m_FastRom(fastRom)
{
    if (addrType == AddressingType::lowrom) {
        initLowRom();
    } else if (addrType == AddressingType::highrom) {
        initHighRom();
    } else {
        assert(false);
    }

    // Fill generic mirroring info
    m_Components[enumToInt(MemComponentType::ram)].addrConverter =
        [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (kWramBankStart <= bank && bank <= kWramBankEnd) {
            // Direct access
            return (bank - kWramBankStart) * 0x10000 + offset;
        } else if (bank <= 0x3F && offset <= 0x1FFF) {
            return offset;
        } else {
            assert(false);
            return 0;
        }
    };

    auto dropBankConverter = [](uint8_t bank, uint16_t offset) -> uint32_t {
        return offset;
    };

    m_Components[enumToInt(MemComponentType::ppu)].addrConverter =
        dropBankConverter;
    m_Components[enumToInt(MemComponentType::dma)].addrConverter =
        dropBankConverter;
    m_Components[enumToInt(MemComponentType::apu)].addrConverter =
        dropBankConverter;
    m_Components[enumToInt(MemComponentType::irq)].addrConverter =
        dropBankConverter;
    m_Components[enumToInt(MemComponentType::joypads)].addrConverter =
        dropBankConverter;
    m_Components[enumToInt(MemComponentType::indirectRam)].addrConverter =
        dropBankConverter;
}

void Membus::initLowRom()
{
    // Fill "real" mapping
    for (const auto& component : s_LowRomMap.components) {
        for (int i = component.bankStart; i <= component.bankEnd; i++) {
            if (m_Banks[i].type == BankType::invalid) {
                m_Banks[i].type = BankType::direct;
            } else {
                assert(m_Banks[i].type == BankType::direct);
            }

            m_Banks[i].ranges.push_back(&component);

            // Fill SystemArea LUT
            if (i <= 0x3F && component.offsetEnd <= 0x7FFF) {
                for (int offset = component.offsetStart;
                     offset <= component.offsetEnd;
                     offset++) {
                    m_SystemArea[offset] = &component;
                }
            }
        }
    }

    // Fill mirroring info
    for (const auto& mirror : s_LowRomMap.mirrors) {
        assert(
            mirror.srcBankEnd - mirror.srcBankStart ==
            mirror.targetBankEnd - mirror.targetBankStart);

        for (int i = mirror.srcBankStart; i <= mirror.srcBankEnd; i++) {
            m_Banks[i].type = BankType::mirrored;
            m_Banks[i].targetBank =
                mirror.targetBankStart + (i - mirror.srcBankStart);
        }
    }

    // Plug LowRom specific address converters
    m_Components[enumToInt(MemComponentType::rom)].addrConverter =
        [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (bank <= 0x7D && offset >= 0x8000) {
            return bank * 0x8000 + (offset - 0x8000);
        } else if (0x40 <= bank && bank <= 0x6F && offset < 0x8000) {
            return bank * 0x8000 + offset;
        } else if (bank >= 0xFE) {
            return (bank - 0xFE + 0x7E) * 0x8000 + (offset - 0x8000);
        } else {
            assert(false);
            return 0;
        }
    };

    m_Components[enumToInt(MemComponentType::sram)].addrConverter =
        [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (bank >= 0xFE) {
            return (bank - 0xFE + 0xE) * 0x8000 + offset;
        } else {
            return (bank - kSramBankStart) * 0x8000 + offset;
        }
    };
}

void Membus::initHighRom()
{
    // Fill "real" mapping
    for (const auto& component : s_HighRomMap.components) {
        for (int i = component.bankStart; i <= component.bankEnd; i++) {
            if (m_Banks[i].type == BankType::invalid) {
                m_Banks[i].type = BankType::direct;
            } else {
                assert(m_Banks[i].type == BankType::direct);
            }

            m_Banks[i].ranges.push_back(&component);

            // Fill SystemArea LUT
            if (i <= 0x3F && component.offsetEnd <= 0x7FFF) {
                for (int offset = component.offsetStart;
                     offset <= component.offsetEnd;
                     offset++) {
                    m_SystemArea[offset] = &component;
                }
            }
        }
    }

    // Fill mirroring info
    for (const auto& mirror : s_HighRomMap.mirrors) {
        assert(
            mirror.srcBankEnd - mirror.srcBankStart ==
            mirror.targetBankEnd - mirror.targetBankStart);

        for (int i = mirror.srcBankStart; i <= mirror.srcBankEnd; i++) {
            m_Banks[i].type = BankType::mirrored;
            m_Banks[i].targetBank =
                mirror.targetBankStart + (i - mirror.srcBankStart);
        }
    }

    // Plug LowRom specific address converters
    m_Components[enumToInt(MemComponentType::rom)].addrConverter =
        [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (bank <= 0x3F) {
            return (bank << 16) + offset;
        } else if (0x80 <= bank && bank <= 0xBF) {
            return ((bank - 0x80) << 16) + offset;
        } else if (0x40 <= bank && bank <= 0x7D) {
            return ((bank - 0x40) << 16) + offset;
        } else if (bank >= 0xC0) {
            return ((bank - 0xC0) << 16) + offset;
        } else {
            assert(false);
            return 0;
        }
    };

    m_Components[enumToInt(MemComponentType::sram)].addrConverter =
        [](uint8_t bank, uint16_t offset) -> uint32_t {
        return (bank - 0x20) * 0x2000 + offset - 0x6000;
    };
}

Membus::ComponentHandler* Membus::getComponentFromAddr(
    uint32_t addr,
    MemComponentType* type,
    uint8_t* outBankId,
    uint16_t* outOffset,
    int* cycles,
    uint32_t access)
{
    const MemoryRange* range = nullptr;

    uint8_t bankId = addr >> 16;
    uint16_t offset = addr & 0xFFFF;
    uint8_t targetBank;

    // Get target bank (aka: handle mirroring)
    if (m_Banks[bankId].type == BankType::mirrored) {
        targetBank = m_Banks[bankId].targetBank;
    } else {
        targetBank = bankId;
    }

    if (targetBank <= 0x3F && offset <= 0x7FFF) {
        // System area
        range = m_SystemArea[offset];
    } else {
        Bank* bank = &m_Banks[targetBank];

        // Find target component
        for (auto& i : bank->ranges) {
            if (i->offsetStart <= offset && offset <= i->offsetEnd) {
                range = i;
                break;
            }
        }
    }

    if (!range) {
        return nullptr;
    } else if (!(range->access & access)) {
        assert(false);
        return nullptr;
    }

    *type = range->type;
    *outBankId = targetBank;
    *outOffset = offset;

    if (cycles) {
        if (range->type == MemComponentType::rom) {
            *cycles += getRomTiming(bankId);
        } else {
            *cycles += range->cycles;
        }
    }

    return &m_Components[enumToInt(range->type)];
}

int Membus::getRomTiming(uint8_t bank)
{
    return m_FastRom ? kTimingRomFastAccess : kTimingRomSlowAccess;
}

int Membus::plugComponent(const std::shared_ptr<MemComponent>& component)
{
    m_Components[enumToInt(component->getType())].ptr = component;

    return 0;
}

uint8_t Membus::readU8(uint32_t addr, int* cycles)
{
    ComponentHandler* component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(
        addr, &type, &bank, &offset, cycles, kComponentAccessR);
    if (!component) {
        assert(false);
        return 0;
    } else if (type == MemComponentType::membus) {
        return internalReadU8(addr);
    } else if (type == MemComponentType::sram && !component->ptr) {
        // Handle games that don't have SRAM
        return 0;
    }

    if (component->addrConverter) {
        finalAddr = component->addrConverter(bank, offset);
    } else {
        finalAddr = (bank << 16) | offset;
    }

    assert(component->ptr);
    return component->ptr->readU8(finalAddr);
}

uint16_t Membus::readU16(uint32_t addr, int* cycles)
{
    return readU8(addr, cycles) | (readU8(addr + 1, cycles) << 8);
}

uint32_t Membus::readU24(uint32_t addr, int* cycles)
{
    return readU8(addr, cycles) | (readU8(addr + 1, cycles) << 8) |
           (readU8(addr + 2, cycles) << 16);
}

void Membus::writeU8(uint32_t addr, uint8_t value, int* cycles)
{
    ComponentHandler* component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(
        addr, &type, &bank, &offset, cycles, kComponentAccessW);
    if (!component) {
        assert(false);
        return;
    } else if (type == MemComponentType::membus) {
        internalWriteU8(addr, value);
        return;
    } else if (type == MemComponentType::sram && !component->ptr) {
        // Handle games that don't have SRAM
        return;
    }

    if (component->addrConverter) {
        finalAddr = component->addrConverter(bank, offset);
    } else {
        finalAddr = (bank << 16) | offset;
    }

    assert(component->ptr);
    component->ptr->writeU8(finalAddr, value);
}

void Membus::writeU16(uint32_t addr, uint16_t value, int* cycles)
{
    writeU8(addr, value & 0xFF, cycles);
    writeU8(addr + 1, value >> 8, cycles);
}

uint8_t Membus::internalReadU8(uint32_t addr)
{
    switch (addr) {
    case kRegisterMemsel:
        return m_FastRom & 1;

    default:
        LOGW(TAG, "Ignore ReadU8 at %06X", addr);
        assert(false);
        return 0;
    }
}

void Membus::internalWriteU8(uint32_t addr, uint8_t value)
{
}

// clang-format off
const Membus::MemoryMap Membus::s_LowRomMap = {
    // Components
    {
        // WRAM direct access
        { 0x00, 0x3F, 0x0000, 0x1FFF, MemComponentType::ram, kComponentAccessRW, kTimingRamAccess },
        { 0x7E, 0x7F, 0x0000, 0xFFFF, MemComponentType::ram, kComponentAccessRW, kTimingRamAccess },

        // WRAM indirect access
        { 0x00, 0x3F, 0x2180, 0x2180, MemComponentType::indirectRam, kComponentAccessRW, kTimingRamAccess },
        { 0x00, 0x3F, 0x2181, 0x2183, MemComponentType::indirectRam, kComponentAccessW, kTimingRamAccess },

        // PPU
        { 0x00, 0x3F, 0x2100, 0x2133, MemComponentType::ppu, kComponentAccessW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x2134, 0x213F, MemComponentType::ppu, kComponentAccessR, kTimingIoFastAccess },

        // APU
        { 0x00, 0x3F, 0x2140, 0x217F, MemComponentType::apu, kComponentAccessRW, kTimingIoFastAccess },

        // DMA
        { 0x00, 0x3F, 0x4300, 0x437F, MemComponentType::dma, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x420B, 0x420C, MemComponentType::dma, kComponentAccessRW, kTimingIoFastAccess },

        // Maths
        { 0x00, 0x3F, 0x4202, 0x4206, MemComponentType::maths, kComponentAccessW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4214, 0x4217, MemComponentType::maths, kComponentAccessR, kTimingIoFastAccess },

        // IRQ configuration
        { 0x00, 0x3F, 0x4200, 0x4200, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4207, 0x420A, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4210, 0x4212, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },

        // ROM (cycles are computed in a different way)
        { 0x00, 0x7D, 0x8000, 0xFFFF, MemComponentType::rom, kComponentAccessR, 0 },
        { 0x40, 0x6F, 0x0000, 0x7FFF, MemComponentType::rom, kComponentAccessR, 0 },
        { 0xFE, 0xFF, 0x8000, 0xFFFF, MemComponentType::rom, kComponentAccessR, 0 },

        // SRAM
        { 0x70, 0x7D, 0x0000, 0x7FFF, MemComponentType::sram, kComponentAccessRW, kTimingRamAccess },
        { 0xFE, 0xFF, 0x0000, 0x7FFF, MemComponentType::sram, kComponentAccessRW, kTimingRamAccess },

        // Membus
        { 0x00, 0x3F, 0x420D, 0x420D, MemComponentType::membus, kComponentAccessRW, kTimingIoFastAccess },

        // Joypad
        { 0x00, 0x3F, 0x4016, 0x4017, MemComponentType::joypads, kComponentAccessRW, kTimingIoSlowAccess },
        { 0x00, 0x3F, 0x4201, 0x4201, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4213, 0x4213, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4218, 0x421F, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
    },
    // Mirrors (src bank => target bank)
    {
        { 0x80, 0xFD, 0x00, 0x7D },
    },
};

const Membus::MemoryMap Membus::s_HighRomMap = {
    // Components
    {
        // WRAM direct access
        { 0x00, 0x3F, 0x0000, 0x1FFF, MemComponentType::ram, kComponentAccessRW, kTimingRamAccess },
        { 0x7E, 0x7F, 0x0000, 0xFFFF, MemComponentType::ram, kComponentAccessRW, kTimingRamAccess },

        // WRAM indirect access
        { 0x00, 0x3F, 0x2180, 0x2180, MemComponentType::indirectRam, kComponentAccessRW, kTimingRamAccess },
        { 0x00, 0x3F, 0x2181, 0x2183, MemComponentType::indirectRam, kComponentAccessW, kTimingRamAccess },

        // PPU
        { 0x00, 0x3F, 0x2100, 0x2133, MemComponentType::ppu, kComponentAccessW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x2134, 0x213F, MemComponentType::ppu, kComponentAccessR, kTimingIoFastAccess },

        // APU
        { 0x00, 0x3F, 0x2140, 0x217F, MemComponentType::apu, kComponentAccessRW, kTimingIoFastAccess },

        // DMA
        { 0x00, 0x3F, 0x4300, 0x437F, MemComponentType::dma, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x420B, 0x420C, MemComponentType::dma, kComponentAccessRW, kTimingIoFastAccess },

        // Maths
        { 0x00, 0x3F, 0x4202, 0x4206, MemComponentType::maths, kComponentAccessW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4214, 0x4217, MemComponentType::maths, kComponentAccessR, kTimingIoFastAccess },

        // IRQ configuration
        { 0x00, 0x3F, 0x4200, 0x4200, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4207, 0x420A, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4210, 0x4212, MemComponentType::irq, kComponentAccessRW, kTimingIoFastAccess },

        // ROM (cycles are computed in a different way)
        { 0x00, 0x3F, 0x8000, 0xFFFF, MemComponentType::rom, kComponentAccessR, 0 },
        { 0x40, 0x7D, 0x0000, 0xFFFF, MemComponentType::rom, kComponentAccessR, 0 },
        { 0xFE, 0xFF, 0x0000, 0xFFFF, MemComponentType::rom, kComponentAccessR, 0 },

        // SRAM
        { 0x20, 0x3F, 0x6000, 0x7FFF, MemComponentType::sram, kComponentAccessRW, kTimingRamAccess },

        // Membus
        { 0x00, 0x3F, 0x420D, 0x420D, MemComponentType::membus, kComponentAccessRW, kTimingIoFastAccess },

        // Joypad
        { 0x00, 0x3F, 0x4016, 0x4017, MemComponentType::joypads, kComponentAccessRW, kTimingIoSlowAccess },
        { 0x00, 0x3F, 0x4201, 0x4201, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4213, 0x4213, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
        { 0x00, 0x3F, 0x4218, 0x421F, MemComponentType::joypads, kComponentAccessRW, kTimingIoFastAccess },
    },
    // Mirrors (src bank => target bank)
    {
        { 0x80, 0x9F, 0x00, 0x1F },
        { 0xA0, 0xBF, 0x20, 0x3F },
        { 0xC0, 0xFD, 0x40, 0x7D },
    },
};

// clang-format on

} // namespace msfce::core
