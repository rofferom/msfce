#pragma once

#include <stdlib.h>
#include <stdint.h>

class Ppu {
public:
    Ppu() = default;
    ~Ppu() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

    bool isNMIEnabled() const;

private:
    bool m_ForcedBlanking = false;
    uint8_t m_Brightness = 0;

    // VBlank interrupt
    bool m_NMIEnabled = false;
};
