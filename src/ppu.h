#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory>

#include "frontend.h"
#include "memcomponent.h"
#include "registers.h"
#include "schedulertask.h"

class Ppu : public MemComponent, public SchedulerTask {
public:
    enum : uint32_t {
        Event_VBlankStart = (1 << 0),
        Event_ScanEnded = (1 << 1),
    };

    enum class DrawConfig {
        Draw,
        Skip,
    };

public:
    Ppu(const std::shared_ptr<Frontend>& frontend);
    ~Ppu() = default;

    void dump() const;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    int run() override;

    uint32_t getEvents() const;
    void setDrawConfig(DrawConfig config);

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    typedef uint32_t (*TilemapMapper)(uint16_t tilemapBase, int x, int y);

    static const int kObjCount = 128;
    static const int kBackgroundCount = 4;

private:
    struct Background {
        uint16_t m_TilemapBase = 0;
        uint16_t m_TilemapSize = 0;
        uint16_t m_TileBase = 0;
        uint16_t m_TileSize = 0;

        uint16_t m_HOffset = 0;
        uint16_t m_VOffset = 0;
    };

    enum class Layer {
        none,
        background,
        sprite,
    };

    struct LayerPriority {
        Layer m_Layer;
        int m_BgIdx;
        int m_Priority;
    };

    struct ObjProperty {
        // Loaded from OAM
        int16_t m_X = 0;
        int16_t m_Y = 0;
        int m_Size = 0;
        bool m_VerticalFlip = 0;
        bool m_HorizontalFlip = 0;
        int m_Priority = 0;
        int m_Palette = 0;
        int m_TileIndex = 0;

        // Extra data
        // Size in tiles
        int m_Width;
        int m_Height;

        // Size in pixel
        int m_WidthPixel;
        int m_HeightPixel;

        int m_xEnd;

        bool m_OnScreen;
    };


    struct RendererBgInfo {
        int bgIdx;
        Background* background;

        /**
         * BG global configuration.
         * Unit "tile" is a base 8x8 tile.
         */

        // Unit: tile
        int tilemapWidth;
        int tilemapHeight;

        // Unit: pixel
        int tilemapWidthPixel;
        int tilemapHeightPixel;

        // Unit: tile
        int tileWidth;
        int tileHeight;

        // Unit: pixel
        int tileWidthPixel;
        int tileHeightPixel;

        int tileBpp;
        int tileSize = tileBpp * 8;

        TilemapMapper tilemapMapper;

        /**
         * Current position in the tilemap (unit: tile)
         * The tile can be up to 16x16
         */
        int tilemapX;
        int tilemapY;

        // Tile position (unit: pixel)
        int tilePixelX;
        int tilePixelY;

        // Subtile position (unit: tile)
        int subtileX;
        int subtileY;

        // Subtile properties
        int verticalFlip;
        int horizontalFlip;
        int priority;
        int palette;
        int tileIndex;

        /**
         * Current tile info
         */
        // Position inside the current tile (unit: pixel)
        int subtilePixelX;
        int subtilePixelY;

        // Tile data
        const uint8_t* tileDataPlane0; // 2 first bits
        const uint8_t* tileDataPlane1; // 2 next bits
    };

    struct RenderObjInfo {
        ObjProperty* m_Obj[kObjCount];
        int m_ObjCount;
    };

private:
    TilemapMapper getTilemapMapper(uint16_t tilemapSize) const;

    void updateTileData(const Background* bg, RendererBgInfo* renderBg);
    void updateSubtileData(const Background* bg, RendererBgInfo* renderBg);

    bool getBackgroundCurrentPixel(RendererBgInfo* renderBg, int priority, Color* color);
    bool getSpriteCurrentPixel(int x, int y, int priority, Color* color);
    void moveToNextPixel(RendererBgInfo* renderBg);
    void incrementVramAddress();

    void loadObjs();
    void printObjsCoordinates();

    uint32_t getColorFromCgram(int bgIdx, int bpp, int palette, int colorIdx);
    uint32_t getObjColorFromCgram(int palette, int color);
    uint32_t getMainBackdropColor();

    bool getPixelFromBg(int bgIdx, const Background* bg, int screen_x, int screen_y, Color* c, int* priority);
    bool getPixelFromObj(int screen_x, int screen_y, Color* c, int* priority);

    void initScreenRender();
    void initLineRender(int y);
    void renderDot(int x, int y);
    void renderStep();

private:
    std::shared_ptr<Frontend> m_Frontend;
    DrawConfig m_DrawConfig = DrawConfig::Draw;
    uint32_t m_Events = 0;

    bool m_ForcedBlanking = false;
    uint8_t m_Brightness = 0;

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
    int m_OamHighestPriorityObj = 0;
    int m_OamForcedPriority = 0;

    int m_OamFlip = 0;
    uint16_t m_OamWriteRegister = 0;

    uint16_t m_ObjSize = 0;
    uint16_t m_ObjGapSize = 0;
    uint16_t m_ObjBase = 0;

    ObjProperty m_Objs[kObjCount];

    // Backgrounds
    Background m_Backgrounds[kBackgroundCount];
    uint8_t m_OldBgByte = 0;

    int m_Bgmode = 0;
    bool m_Bg3Priority = 0;

    // Layers priority charts
    static const Ppu::LayerPriority s_LayerPriorityMode0[];
    static const Ppu::LayerPriority s_LayerPriorityMode1_BG3_On[];
    static const Ppu::LayerPriority s_LayerPriorityMode1_BG3_Off[];

    // Rendering
    int m_RenderX = 0;
    int m_RenderY = 0;

    RendererBgInfo m_RenderBgInfo[kBackgroundCount];
    RenderObjInfo m_RenderObjInfo[kPpuDisplayHeight];
    const Ppu::LayerPriority* m_RenderLayerPriority = nullptr;
};
