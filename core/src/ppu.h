#pragma once

#include <stdlib.h>
#include <stdint.h>

#include <functional>
#include <memory>

#include "msfce/core/renderer.h"
#include "memcomponent.h"
#include "registers.h"
#include "schedulertask.h"

namespace msfce::core {

class Ppu
    : public MemComponent
    , public SchedulerTask {
public:
    enum : uint32_t {
        Event_VBlankStart = (1 << 0),
        Event_HBlankStart = (1 << 1),
        Event_HBlankEnd = (1 << 2),
        Event_ScanStarted = (1 << 3),
        Event_ScanEnded = (1 << 4),
        Event_HV_IRQ = (1 << 5),
    };

    enum class DrawConfig {
        Draw,
        Skip,
    };

    enum class HVIRQConfig {
        Disable = 0,
        H = 1,
        V = 2,
        HV = 3,
    };

    using ScanStartedCb = std::function<void()>;
    using ScanEndedCb = std::function<void()>;
    using RenderCb = std::function<void(const Color& c)>;

public:
    Ppu(ScanStartedCb scanStartedCb,
        ScanEndedCb scanEndedCb,
        RenderCb renderCb);
    ~Ppu() = default;

    void dump() const;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    int run() override;

    uint32_t getEvents() const;
    void setDrawConfig(DrawConfig config);

    void setHVIRQConfig(HVIRQConfig config, uint16_t H, uint16_t V);

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
        const uint8_t* tileDataPlane2; // 2 next bits
        const uint8_t* tileDataPlane3; // 2 next bits

        // Mosaic
        struct {
            int startX;
            int startY;
            int size = 1;
        } mosaic;
    };

    struct RenderObjInfo {
        ObjProperty* m_Obj[kObjCount];
        int m_ObjCount;
    };

    // Window
    struct WindowConfig {
        enum class Config {
            disabled,
            outside,
            inside,
        };

        // Position
        int m_Left;
        int m_Right;

        // Configuration
        Config m_BackgroundConfig[kBackgroundCount];
        Config m_ObjConfig;
        Config m_MathConfig;
    };

    enum class WindowLogic : uint32_t {
        OR = 0,
        AND = 1,
        XOR = 2,
        XNOR = 3,
    };

    struct ScreenConfig {
        bool m_BgEnabled[kBackgroundCount];
        bool m_ObjEnabled;

        bool m_Window_BgDisable[kBackgroundCount];
        bool m_Window_ObjDisable;
    };

    enum class ColorMathConfig : uint32_t {
        Never,
        NotMathWin,
        MathWin,
        Always,
    };

    struct BgColorProp {
        Layer m_Layer;

        // Background attributes
        int m_BgIdx;

        // Sprite attributes
        int m_Palette;
    };

private:
    void setHVIRQ(int x, int y);

    TilemapMapper getTilemapMapper(uint16_t tilemapSize) const;

    void updateTileData(const Background* bg, RendererBgInfo* renderBg);
    void updateSubtileData(const Background* bg, RendererBgInfo* renderBg);

    bool getScreenCurrentPixel(
        int x,
        int y,
        const ScreenConfig& screenConfig,
        uint32_t* color,
        BgColorProp* colorProp);

    bool getBackgroundCurrentPixel(
        int x,
        const ScreenConfig& screenConfig,
        const RendererBgInfo* renderBg,
        int priority,
        uint32_t* color);

    bool getSpriteCurrentPixel(
        int x,
        int y,
        const ScreenConfig& screenConfig,
        int priority,
        uint32_t* color,
        int* palette = nullptr);

    void moveToNextPixel(RendererBgInfo* renderBg);
    void incrementVramAddress();

    void loadObjs();
    void printObjsCoordinates();

    uint32_t getColorFromCgram(int bgIdx, int bpp, int palette, int colorIdx);
    uint32_t getObjColorFromCgram(int palette, int color);
    uint32_t getMainBackdropColor();

    bool getPixelFromBg(
        int bgIdx,
        const Background* bg,
        int screen_x,
        int screen_y,
        Color* c,
        int* priority);
    bool getPixelFromObj(int screen_x, int screen_y, Color* c, int* priority);

    void initScreenRender();
    void initLineRender(int y);
    void renderDot(int x, int y);
    void renderStep();

    static WindowConfig::Config getWindowConfig(uint32_t value);
    static bool isInsideWindow(
        int x,
        const WindowConfig& config,
        WindowConfig::Config layerConfig,
        bool* enabled);
    bool applyWindowLogic(
        int x,
        WindowConfig::Config window1Config,
        WindowConfig::Config window2Config,
        WindowLogic logic);

    void initScreenRenderMode7();
    void initLineRenderMode7(int y);

    bool renderDotMode7(
        int x,
        int y,
        const ScreenConfig& screenConfig,
        uint32_t* color,
        BgColorProp* colorProp);

    bool renderGetColorMode7(
        int x,
        int y,
        const ScreenConfig& screenConfig,
        uint32_t* color);

private:
    ScanStartedCb m_ScanStartedCb;
    ScanEndedCb m_ScanEndedCb;
    RenderCb m_RenderCb;
    DrawConfig m_DrawConfig = DrawConfig::Draw;
    uint32_t m_Events = 0;

    bool m_ForcedBlanking = false;
    uint8_t m_Brightness = 0;

    uint8_t m_Ppu1OpenBus = 0;
    uint8_t m_Ppu2OpenBus = 0;

    struct {
        HVIRQConfig m_Config = HVIRQConfig::Disable;
        uint16_t m_H = 0;
        uint16_t m_V = 0;
    } m_HVIRQ;

    // Position reader
    uint16_t m_HPos = 0;
    uint8_t m_HPosReadFlip = 0;

    uint16_t m_VPos = 0;
    uint8_t m_VPosReadFlip = 0;

    // VRAM
    bool m_VramIncrementHigh = false;
    uint8_t m_VramAddressTranslate = 0;
    uint8_t m_VramIncrementStep = 0;
    uint8_t m_Vram[64 * 1024];
    uint16_t m_VramAddress = 0;
    uint16_t m_VramPrefetch = 0;

    // CGRAM (palette)
    uint16_t m_Cgram[256];
    uint8_t m_CgdataAddress = 0;

    bool m_CgramLsbSet = false;
    uint8_t m_CgramLsb = 0;

    uint32_t m_SubscreenBackdrop = 0;

    // OAM (sprites)
    uint8_t m_Oam[2 * 256 + 32];
    uint16_t m_OamAddress = 0;
    uint16_t m_OamAddressReload = 0;
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

    // Mosaic
    struct {
        uint8_t m_Size = 0;
        bool m_Backgrounds[kBackgroundCount];
    } m_Mosaic;

    // Window
    WindowConfig m_Window1Config;
    WindowConfig m_Window2Config;

    WindowLogic m_WindowLogicBackground[kBackgroundCount];
    WindowLogic m_WindowLogicObj;
    WindowLogic m_WindowLogicMath;

    ScreenConfig m_MainScreenConfig;
    ScreenConfig m_SubScreenConfig;

    // Color math
    ColorMathConfig m_ForceMainScreenBlack;
    ColorMathConfig m_ColorMathEnabled;
    bool m_SubscreenEnabled = false;

    uint8_t m_ColorMathOperation = 0;
    bool m_ColorMathBackground[kBackgroundCount];
    bool m_ColorMathObj = false;
    bool m_ColorMathBackdrop = false;

    // Mode 7
    int m_M7ScreenOver = 0;
    bool m_M7HFlip = false;
    bool m_M7VFlip = false;

    uint8_t m_M7Old = 0;
    int16_t m_M7HOFS = 0;
    int16_t m_M7VOFS = 0;
    int16_t m_M7A = 0;
    int16_t m_M7B = 0;
    int16_t m_M7C = 0;
    int16_t m_M7D = 0;
    int16_t m_M7X = 0;
    int16_t m_M7Y = 0;
    int32_t m_MPY = 0;

    // Layers priority charts
    static const Ppu::LayerPriority s_LayerPriorityMode0[];
    static const Ppu::LayerPriority s_LayerPriorityMode1_BG3_On[];
    static const Ppu::LayerPriority s_LayerPriorityMode1_BG3_Off[];
    static const Ppu::LayerPriority s_LayerPriorityMode3[];
    static const Ppu::LayerPriority s_LayerPriorityMode7[];

    // Rendering
    int m_RenderX = 0;
    int m_RenderY = 0;

    RendererBgInfo m_RenderBgInfo[kBackgroundCount];
    RenderObjInfo m_RenderObjInfo[kPpuDisplayHeight];
    const Ppu::LayerPriority* m_RenderLayerPriority = nullptr;
};

} // namespace msfce::core
