#pragma once

#include <stdint.h>
#include <memory>
#include <vector>

class Apu;
class ControllerPorts;
class Dma;
class Ppu;
class Maths;

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

    int plugApu(const std::shared_ptr<Apu>& spu);
    int plugControllerPorts(const std::shared_ptr<ControllerPorts>& controllerPorts);
    int plugDma(const std::shared_ptr<Dma>& dma);
    int plugMaths(const std::shared_ptr<Maths>& maths);
    int plugPpu(const std::shared_ptr<Ppu>& ppu);
    int plugRom(std::unique_ptr<std::vector<uint8_t>> rom);

    void dump();

private:
    const uint8_t* getReadPointer(size_t addr);
    uint8_t* getWritePointer(size_t addr);

private:
    uint8_t m_Wram[128 * 1024]; // 128 kbytes

    // Maths registers
    uint8_t m_MathsMutliplicand = 0;
    uint8_t m_MathsMutliplier = 0;

    uint16_t m_MathsDividend = 0;
    uint8_t m_MathsDivisor = 0;

    uint16_t m_MathsQuotient = 0;
    uint16_t m_MathsRemainderProduct = 0;

    std::unique_ptr<std::vector<uint8_t>> m_RomPtr;
    const uint8_t* m_Rom = nullptr;

    std::shared_ptr<Apu> m_Apu;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;
    std::shared_ptr<Dma> m_Dma;
    std::shared_ptr<Maths> m_Maths;
    std::shared_ptr<Ppu> m_Ppu;
};
