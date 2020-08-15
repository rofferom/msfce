#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <functional>

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Ppu {
public:
    using DrawPointCb = std::function<void(int x, int y, const Color& c)>;

public:
    Ppu() = default;
    ~Ppu() = default;

    void dump() const;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

    bool isNMIEnabled() const;

    void render(const DrawPointCb& drawPointCb);

private:
    void incrementVramAddress();

private:
    struct Background {
        uint16_t m_TilemapBase = 0;
        uint16_t m_TilemapSize = 0;
        uint16_t m_TileBase = 0;
        uint16_t m_TileSize = 0;

        uint16_t m_HorizontalOffset = 0;
        uint16_t m_VerticalOffset = 0;
    };

    static const int kBackgroundCount = 4;

private:
    bool m_ForcedBlanking = false;
    uint8_t m_Brightness = 0;

    // VBlank interrupt
    bool m_NMIEnabled = false;

    // VRAM
    bool m_VramIncrementHigh = false;
    uint8_t m_VramIncrementStep = 0;
    uint8_t m_Vram[64 * 1024];
    uint16_t m_VramAddress = 0;

    // CGRAM (palette)
    uint16_t m_Cgram[256];
    uint8_t m_CgdataAddress = 0;

    bool m_CgramLsbSet = false;
    uint8_t m_CgramLsb = 0;

    // Backgrounds
    Background m_Backgrounds[kBackgroundCount];
    uint8_t m_OldBgByte = 0;

    int m_Bgmode = 0;
    bool m_Bg3Priority = 0;
};
