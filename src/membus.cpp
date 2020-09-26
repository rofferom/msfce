#include <assert.h>
#include "log.h"
#include "registers.h"
#include "membus.h"

#define TAG "membus"

namespace {

constexpr uint32_t kComponentAccessR = (1 << 0);
constexpr uint32_t kComponentAccessW = (1 << 1);
constexpr uint32_t kComponentAccessRW = kComponentAccessR | kComponentAccessW;

} // anonymous namespace

Membus::Membus(AddressingType addrType)
    : m_AddrType(addrType)
{
    if (addrType == AddressingType::lowrom) {
        initLowRom();
    } else {
        assert(false);
    }

    // Fill generic mirroring info
    m_Components[enumToInt(MemComponentType::ram)].addrConverter = [](uint8_t bank, uint16_t offset) -> uint32_t {
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
}

void Membus::initLowRom()
{
    // Fill "real" mapping
    for (const auto& component: s_LowRomMap.components) {
        for (int i = component.bankStart; i <= component.bankEnd; i++) {
            if (m_Banks[i].type == BankType::invalid) {
                m_Banks[i].type = BankType::direct;
            } else {
                assert(m_Banks[i].type == BankType::direct);
            }

            m_Banks[i].ranges.push_back(&component);
        }
    }

    // Fill mirroring info
    for (const auto& mirror: s_LowRomMap.mirrors) {
        assert(mirror.srcBankEnd - mirror.srcBankStart == mirror.targetBankEnd - mirror.targetBankStart);

        for (int i = mirror.srcBankStart; i <= mirror.srcBankEnd; i++) {
            m_Banks[i].type = BankType::mirrored;
            m_Banks[i].targetBank = mirror.targetBankStart + (i - mirror.srcBankStart);
        }
    }

    // Plug LowRom specific address converters
    m_Components[enumToInt(MemComponentType::rom)].addrConverter = [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (bank <= 0x7D) {
            return bank * 0x8000 + (offset - 0x8000);
        } else if (bank >= 0xFE) {
            return (bank - 0xFE + 0x7E) * 0x8000 + (offset - 0x8000);
        } else {
            assert(false);
        }
    };

    m_Components[enumToInt(MemComponentType::sram)].addrConverter = [](uint8_t bank, uint16_t offset) -> uint32_t {
        if (bank >= 0xFE) {
            return (bank - 0xFE + 0xE) * 0x8000 + offset;
        } else {
            return (bank - kSramBankStart) * 0x8000 + offset;
        }
    };
}

Membus::ComponentHandler *Membus::getComponentFromAddr(
    uint32_t addr,
    MemComponentType* type,
    uint8_t *outBankId,
    uint16_t *outOffset,
    uint32_t access)
{
    Bank* bank;
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

    bank = &m_Banks[targetBank];

    // Find target component
    for (auto& i: bank->ranges) {
        if (i->offsetStart <= offset && offset <= i->offsetEnd) {
            range = i;
            break;
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

    return &m_Components[enumToInt(range->type)];
}

int Membus::plugComponent(const std::shared_ptr<MemComponent>& component)
{
    m_Components[enumToInt(component->getType())].ptr = component;

    return 0;
}

uint8_t Membus::readU8(uint32_t addr)
{
    ComponentHandler *component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(addr, &type, &bank, &offset, kComponentAccessR);
    if (type == MemComponentType::membus) {
        return internalReadU8(addr);
    } else if (!component) {
        assert(false);
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

uint16_t Membus::readU16(uint32_t addr)
{
    ComponentHandler *component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(addr, &type, &bank, &offset, kComponentAccessR);
    if (type == MemComponentType::membus) {
        return internalReadU8(addr);
    } else if (!component) {
        assert(false);
        return 0;
    }

    if (component->addrConverter) {
        finalAddr = component->addrConverter(bank, offset);
    } else {
        finalAddr = (bank << 16) | offset;
    }

    assert(component->ptr);
    return (component->ptr->readU8(finalAddr + 1) << 8)
          | component->ptr->readU8(finalAddr);
}

uint32_t Membus::readU24(uint32_t addr)
{
    ComponentHandler *component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(addr, &type, &bank, &offset, kComponentAccessR);
    if (type == MemComponentType::membus) {
        return internalReadU8(addr);
    } else if (!component) {
        assert(false);
        return 0;
    }

    if (component->addrConverter) {
        finalAddr = component->addrConverter(bank, offset);
    } else {
        finalAddr = (bank << 16) | offset;
    }

    assert(component->ptr);
    return (component->ptr->readU8(finalAddr + 2) << 16)
         | (component->ptr->readU8(finalAddr + 1) << 8)
         |  component->ptr->readU8(finalAddr);
}

void Membus::writeU8(uint32_t addr, uint8_t value)
{
    ComponentHandler *component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(addr, &type, &bank, &offset, kComponentAccessW);
    if (type == MemComponentType::membus) {
        internalWriteU8(addr, value);
        return;
    } else if (!component) {
        assert(false);
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

void Membus::writeU16(uint32_t addr, uint16_t value)
{
    ComponentHandler *component;
    MemComponentType type;
    uint8_t bank;
    uint16_t offset;
    uint32_t finalAddr;

    component = getComponentFromAddr(addr, &type, &bank, &offset, kComponentAccessW);
    if (type == MemComponentType::membus) {
        internalWriteU8(addr, value);
        return;
    } else if (!component) {
        assert(false);
        return;
    }

    if (component->addrConverter) {
        finalAddr = component->addrConverter(bank, offset);
    } else {
        finalAddr = (bank << 16) | offset;
    }

    assert(component->ptr);
    component->ptr->writeU8(finalAddr, value & 0xFF);
    component->ptr->writeU8(finalAddr + 1, value >> 8);
}

uint8_t Membus::internalReadU8(uint32_t addr)
{
    return 0;
}

void Membus::internalWriteU8(uint32_t addr, uint8_t value)
{
}

const Membus::MemoryMap Membus::s_LowRomMap = {
    // Components
    {
        // WRAM direct access
        { 0x00, 0x3F, 0x0000, 0x1FFF, MemComponentType::ram, kComponentAccessRW },
        { 0x7E, 0x7F, 0x0000, 0xFFFF, MemComponentType::ram, kComponentAccessRW },

        // WRAM indirect access
        //{ 0x00, 0x3F, 0x2180, 0x2183, MemComponentType::indirectRam, kComponentAccessRW },

        // PPU
        { 0x00, 0x3F, 0x2100, 0x2133, MemComponentType::ppu, kComponentAccessW },
        { 0x00, 0x3F, 0x2134, 0x213F, MemComponentType::ppu, kComponentAccessR },

        // APU
        { 0x00, 0x3F, 0x2140, 0x217F, MemComponentType::apu, kComponentAccessRW },

        // DMA
        { 0x00, 0x3F, 0x4300, 0x437F, MemComponentType::dma, kComponentAccessRW },
        { 0x00, 0x3F, 0x420B, 0x420C, MemComponentType::dma, kComponentAccessRW },

        // Maths
        { 0x00, 0x3F, 0x4202, 0x4206, MemComponentType::maths, kComponentAccessW },
        { 0x00, 0x3F, 0x4214, 0x4217, MemComponentType::maths, kComponentAccessR },

        // IRQ configuration
        { 0x00, 0x3F, 0x4200, 0x4200, MemComponentType::irq, kComponentAccessRW },
        { 0x00, 0x3F, 0x4207, 0x420A, MemComponentType::irq, kComponentAccessRW },
        { 0x00, 0x3F, 0x4210, 0x4212, MemComponentType::irq, kComponentAccessRW },

        // ROM
        { 0x00, 0x7D, 0x8000, 0xFFFF, MemComponentType::rom, kComponentAccessR },
        { 0xFE, 0xFF, 0x8000, 0xFFFF, MemComponentType::rom, kComponentAccessR },

        // SRAM
        { 0x70, 0x7D, 0x0000, 0x7FFF, MemComponentType::sram, kComponentAccessRW },
        { 0xFE, 0xFF, 0x0000, 0x7FFF, MemComponentType::sram, kComponentAccessRW },

        // Membus
        { 0x00, 0x3F, 0x420D, 0x420D, MemComponentType::membus, kComponentAccessRW },

        // Joypad
        { 0x00, 0x3F, 0x4016, 0x4017, MemComponentType::joypads, kComponentAccessRW },
        { 0x00, 0x3F, 0x4201, 0x4201, MemComponentType::joypads, kComponentAccessRW },
        { 0x00, 0x3F, 0x4213, 0x4213, MemComponentType::joypads, kComponentAccessRW },
        { 0x00, 0x3F, 0x4218, 0x421F, MemComponentType::joypads, kComponentAccessRW },
    },
    // Mirrors (src bank => target bank)
    {
        { 0x80, 0xFD, 0x00, 0x7D },
    },
};
