#pragma once

#include <stdint.h>

enum class MemComponentType {
    rom,
    ram,
    indirectRam,
    sram,
    apu,
    maths,
    ppu,
    dma,
    irq,
    joypads,
    membus,

    count
};

constexpr auto kComponentTypeCount = enumToInt(MemComponentType::count);

class MemComponent {
public:
    MemComponent(MemComponentType type);
    virtual ~MemComponent() = default;

    MemComponentType getType() const;

    virtual uint8_t readU8(uint32_t address) = 0;
    virtual void writeU8(uint32_t address, uint8_t value) = 0;

private:
    MemComponentType m_Type;
};
