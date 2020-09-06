#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

#include "memcomponent.h"

class Membus {
public:
    Membus() = default;
    ~Membus() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);
    uint32_t readU24(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);
    void writeU24(size_t addr, uint32_t value);

    int plugApu(const std::shared_ptr<MemComponent>& spu);
    int plugControllerPorts(const std::shared_ptr<MemComponent>& controllerPorts);
    int plugDma(const std::shared_ptr<MemComponent>& dma);
    int plugMaths(const std::shared_ptr<MemComponent>& maths);
    int plugPpu(const std::shared_ptr<MemComponent>& ppu);
    int plugRom(std::unique_ptr<std::vector<uint8_t>> rom);

    void dump();

private:
    const uint8_t* getReadPointer(size_t addr);
    uint8_t* getWritePointer(size_t addr);

private:
    uint8_t m_Wram[128 * 1024]; // 128 kbytes
    uint8_t m_Sram[448 * 1024]; // 448 kbytes

    std::unique_ptr<std::vector<uint8_t>> m_RomPtr;
    const uint8_t* m_Rom = nullptr;

    std::shared_ptr<MemComponent> m_Apu;
    std::shared_ptr<MemComponent> m_ControllerPorts;
    std::shared_ptr<MemComponent> m_Dma;
    std::shared_ptr<MemComponent> m_Maths;
    std::shared_ptr<MemComponent> m_Ppu;
};
