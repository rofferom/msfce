#pragma once

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "utils.h"

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

class BufferMemComponent : public MemComponent {
public:
    BufferMemComponent(MemComponentType type, size_t size);
    BufferMemComponent(MemComponentType type, std::vector<uint8_t>&& data);

    uint8_t readU8(uint32_t address) override;
    void writeU8(uint32_t address, uint8_t value) override;

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    std::vector<uint8_t> m_Data;
};
