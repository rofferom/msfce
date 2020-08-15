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
    struct Background {
        uint16_t m_TilemapBase = 0;
        uint16_t m_TilemapSize = 0;
        uint16_t m_TileBase = 0;
        uint16_t m_TileSize = 0;

        uint16_t m_HOffset = 0;
        uint16_t m_VOffset = 0;
    };

    struct BgPriority {
        int m_BgIdx;
        int m_TilePriority;
    };

    struct ObjProperty {
        int16_t m_X = 0;
        int16_t m_Y = 0;
        int m_Size = 0;
        bool m_VerticalFlip = 0;
        bool m_HorizontalFlip = 0;
        int m_Priority = 0;
        int m_Palette = 0;
        int m_TileIndex = 0;
    };

private:
    void incrementVramAddress();

    void loadObjs();
    void printObjsCoordinates();

    uint32_t getColorFromCgram(int bgIdx, int bpp, int palette, int colorIdx);
    uint32_t getObjColorFromCgram(int palette, int color);
    bool getPixelFromBg(int bgIdx, const Background* bg, int screen_x, int screen_y, Color* c, int* priority);
    bool getPixelFromObj(int screen_x, int screen_y, Color* c, int* priority);

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

    // OAM (sprites)
    uint8_t m_Oam[2 * 256 + 32];
    uint16_t m_OamAddress = 0;
    int m_OamPriority = 0;

    int m_OamFlip = 0;
    uint16_t m_OamWriteRegister = 0;

    uint16_t m_ObjSize = 0;
    uint16_t m_ObjGapSize = 0;
    uint16_t m_ObjBase = 0;

    static const int kObjCount = 128;
    ObjProperty m_Objs[kObjCount];

    // Backgrounds
    static const int kBackgroundCount = 4;
    Background m_Backgrounds[kBackgroundCount];
    uint8_t m_OldBgByte = 0;

    int m_Bgmode = 0;
    bool m_Bg3Priority = 0;

    // Background priority charts
    static const Ppu::BgPriority s_BgPriorityMode1_BG3_On[];
};
