#include <stdio.h>
#include <assert.h>
#include "frontend.h"
#include "log.h"
#include "registers.h"
#include "timings.h"
#include "utils.h"
#include "ppu.h"

#define TAG "ppu"

namespace {

constexpr int kPpuScanWidth = 340;
constexpr int kPpuScanHeight = 262;

constexpr int kPpuTileInfoSize = 2;
constexpr int kPpuTilemapWidth = 32;
constexpr int kPpuTilemapHeight = 32;
constexpr int kPpuTilemapSize = kPpuTilemapWidth * kPpuTilemapHeight * kPpuTileInfoSize;

constexpr int kPpuBaseTileWidth = 8;
constexpr int kPpuBaseTileHeight = 8;

constexpr int kPpuObjCount = 128;
constexpr int kPpuObjBpp = 4;
constexpr int kPpuObjTileSize = 8 * kPpuObjBpp; // 8x8 4bpp
constexpr int kPpuObjPaletteOffset = 128;

constexpr uint16_t convert1kWorkStep(uint16_t v) {
    return v << 11;
}

constexpr uint16_t convert4kWorkStep(uint16_t v) {
    return v << 13;
}

constexpr uint16_t convert8kWorkStep(uint16_t v) {
    return v << 14;
}

/*
 * Unit of returned value is a tile size (8x8 or 16x16).
 * This function converts the value got in BGxSC registers.
 */
void getTilemapDimension(uint16_t tilemapSize, int* width, int* height)
{
    switch (tilemapSize) {
    case 0:
        *width = 32;
        *height = 32;
        break;

    case 1:
        *width = 64;
        *height = 32;
        break;

    case 2:
        *width = 32;
        *height = 64;
        break;

    case 3:
        *width = 64;
        *height = 64;
        break;

    default:
        *width = 0;
        *height = 0;
        assert(false);
        break;
    }
}

/*
 * Unit of returned value is tile.
 * tile_size comes from BGMODE register.
*/
void getTileDimension(int tileSize, int* width, int* height)
{
    switch (tileSize) {
    case 0:
        *width = 1;
        *height = 1;
        break;

    case 1:
        *width = 2;
        *height = 2;
        break;

    default:
        assert(false);
        *width = 0;
        *height = 0;
        break;
    }
}

/**
 * Returns the number of maximum active background in the current mode
 */
size_t getBackgroundCountFromMode(int mode)
{
    constexpr int kInvalidBgCount = -1;

    static const int bgCount[] = {
        4,
        3,
        kInvalidBgCount,
        2,
    };

    assert(static_cast<size_t>(mode) < SIZEOF_ARRAY(bgCount));
    assert(bgCount[mode] != kInvalidBgCount);
    return bgCount[mode];
}

/*
 * The bpp of a background depends on the current mode
*/
int getTileBppFromMode(int mode, size_t bgIdx)
{
    if (mode == 0) {
        static const int colorCount[] = {
            2,
            2,
            2,
            2,
        };

        static_assert(SIZEOF_ARRAY(colorCount) == 4);
        assert(bgIdx < SIZEOF_ARRAY(colorCount));
        return colorCount[bgIdx];
    } else if (mode == 1) {
        static const int colorCount[] = {
            4,
            4,
            2,
        };

        static_assert(SIZEOF_ARRAY(colorCount) == 3);
        assert(bgIdx < SIZEOF_ARRAY(colorCount));
        return colorCount[bgIdx];
    } else if (mode == 3) {
        static const int colorCount[] = {
              8,
              4,
          };

          static_assert(SIZEOF_ARRAY(colorCount) == 2);
          assert(bgIdx < SIZEOF_ARRAY(colorCount));
          return colorCount[bgIdx];

    } else {
        assert(false);
        return 0;
    }
}

/*
 * Get the correct subtilemap from a coordinate.
 * Depending of BGxSC configuration, we have to use one or more
 * tilemaps.
 *
 * Here are a set of mapping functions to get the tilemap to
* use depending of the absolute tile coordinates.
*/

uint32_t tilemapMapper32x32(uint16_t tilemapBase, int x, int y)
{
    return tilemapBase;
}

uint32_t tilemapMapper32x64(uint16_t tilemapBase, int x, int y)
{
    int idx;

    if (y < 32) {
        idx = 0;
    } else {
        idx = 1;
    }

    return tilemapBase + idx * kPpuTilemapSize;
}

uint32_t tilemapMapper64x32(uint16_t tilemapBase, int x, int y)
{
    int idx;

    if (x < 32) {
        idx = 0;
    } else {
        idx = 1;
    }

    return tilemapBase + idx * kPpuTilemapSize;
}

uint32_t tilemapMapper64x64(uint16_t tilemapBase, int x, int y)
{
    int idx;

    if (x < 32 && y < 32) {
        idx = 0;
    } else if (x >= 32 && y < 32) {
        idx = 1;
    } else if (x < 32 && y >= 32) {
        idx = 2;
    } else if (x >= 32 && y >= 32) {
        idx = 3;
    } else {
        assert(false);
        idx = 0;
    }

    return tilemapBase + idx * kPpuTilemapSize;
}

// Set of functions to read the color of a pixel within a tile
uint32_t tileReadColor2bpp(const uint8_t* tileData, size_t tile_size, int row, int column)
{
    constexpr int kTileBpp = 2;
    uint32_t color;

    // 0-1 bits are stored in a 8 words chunk
    int rowOffset = row * kTileBpp;
    const uint8_t* rowData = tileData + rowOffset;

    color = (rowData[0] >> column) & 1;
    color |= ((rowData[1] >> column) & 1) << 1;

    return color;
}

uint32_t tileReadColor4bpp(const uint8_t* tileData, size_t tileSize, int row, int column)
{
    uint32_t color;

    // 0-1 bits are stored in a 8 words chunk, in the first plane
    const uint8_t* rowData = tileData + row * 2;
    color = (rowData[0] >> column) & 1;
    color |= ((rowData[1] >> column) & 1) << 1;

    // 2-3 bits are stored in the second plane
    rowData += tileSize / 2;
    color |= ((rowData[0] >> column) & 1) << 2;
    color |= ((rowData[1] >> column) & 1) << 3;

    return color;
}

typedef uint32_t (*ColorReadCb)(const uint8_t* tileData, size_t tileSize, int row, int column);

ColorReadCb getColorReadCb(int tileBpp)
{
    static const ColorReadCb map[] = {
        nullptr,
        nullptr,
        tileReadColor2bpp, // 2
        nullptr,
        tileReadColor4bpp, // 4
    };

    static_assert(SIZEOF_ARRAY(map) == 5);
    assert(static_cast<size_t>(tileBpp) <= SIZEOF_ARRAY(map));
    assert(map[tileBpp]);

    return map[tileBpp];
}

constexpr uint8_t scaleColor(uint32_t c)
{
    // Convert the packed RGB15 color to RGB32
    return c * 255 / 0b11111;
}

constexpr SnesColor rawColorToRgb(uint32_t raw_color)
{
    // Convert the packed RGB15 color to RGB32
    uint8_t red = scaleColor(raw_color & 0b11111);
    uint8_t green = scaleColor((raw_color >> 5) & 0b11111);
    uint8_t blue = scaleColor((raw_color >> 10) & 0b11111);

    return {red, green, blue};
}

constexpr void applyBrightness(SnesColor* color, uint8_t brightness)
{
    color->r = color->r * (brightness + 1) / 16;
    color->g = color->g * (brightness + 1) / 16;
    color->b = color->b * (brightness + 1) / 16;
}

// Return the size (in 8x8 tiles) of a sprite from its attributes
void getSpriteSize(uint8_t obselSize, uint8_t objSize, int* width, int* height)
{
    if (obselSize == 0) {
        if (objSize == 0) {
            *width = 1;
            *height = 1;
        } else {
            *width = 2;
            *height = 2;
        }
    } else if (obselSize == 3) {
        if (objSize == 0) {
            *width = 2;
            *height = 2;
        } else {
            *width = 4;
            *height = 4;
        }
    } else {
        *width = 0;
        *height = 0;
        assert(false);
    }
}

} // anonymous namespace

// Background priority charts
const Ppu::LayerPriority Ppu::s_LayerPriorityMode0[] = {
    {Layer::background, 0, 1},
    {Layer::background, 1, 1},
    {Layer::background, 0, 0},
    {Layer::background, 1, 0},
    {Layer::background, 2, 1},
    {Layer::background, 3, 1},
    {Layer::background, 2, 1},
    {Layer::background, 3, 0},

    {Layer::none, 0, 0},
};

const Ppu::LayerPriority Ppu::s_LayerPriorityMode1_BG3_On[] = {
    {Layer::background, 2,   1},
    {Layer::sprite,    -1,   3},
    {Layer::background, 0,   1},
    {Layer::background, 1,   1},
    {Layer::sprite,    -1,   2},
    {Layer::background, 0,   0},
    {Layer::sprite,    -1,   1},
    {Layer::background, 1,   0},
    {Layer::sprite,    -1,   0},
    {Layer::background, 2,   0},

    {Layer::none, 0, 0},
};

const Ppu::LayerPriority Ppu::s_LayerPriorityMode1_BG3_Off[] = {
    {Layer::sprite,    -1,   3},
    {Layer::background, 0,   1},
    {Layer::background, 1,   1},
    {Layer::sprite,    -1,   2},
    {Layer::background, 0,   0},
    {Layer::background, 1,   0},
    {Layer::sprite,    -1,   1},
    {Layer::background, 2,   1},
    {Layer::sprite,    -1,   0},
    {Layer::background, 2,   0},
    {Layer::none, 0, 0},
};

const Ppu::LayerPriority Ppu::s_LayerPriorityMode3[] = {
    {Layer::sprite,    -1,   3},
    {Layer::background, 0,   1},
    {Layer::sprite,    -1,   2},
    {Layer::background, 1,   1},
    {Layer::sprite,    -1,   1},
    {Layer::background, 0,   0},
    {Layer::sprite,    -1,   0},
    {Layer::background, 1,   0},
    {Layer::none, 0, 0},
};

const Ppu::LayerPriority Ppu::s_LayerPriorityMode7[] = {
    {Layer::sprite,    -1,   3},
    {Layer::sprite,    -1,   2},
    {Layer::sprite,    -1,   1},
    {Layer::background, 0,   0},
    {Layer::sprite,    -1,   0},
    {Layer::none, 0, 0},
};

Ppu::Ppu(RenderCb renderCb)
    : MemComponent(MemComponentType::ppu),
      SchedulerTask(),
      m_RenderCb(renderCb)
{
}

void Ppu::dump() const
{
    FILE* f;

    // VRAM
    f = fopen("dump_vram.bin", "wb");
    assert(f);
    fwrite(m_Vram, 1, sizeof(m_Vram), f);
    fclose(f);

    // CGRAM
    f = fopen("dump_cgram.bin", "wb");
    assert(f);
    fwrite(m_Cgram, 1, sizeof(m_Cgram), f);
    fclose(f);

    // OAM
    f = fopen("dump_oam.bin", "wb");
    assert(f);
    fwrite(m_Oam, 1, sizeof(m_Oam), f);
    fclose(f);
}

uint8_t Ppu::readU8(uint32_t addr)
{
    switch (addr) {
    case kRegMPYL:
        return m_MPY & 0xFF;

    case kRegMPYM:
        return (m_MPY >> 8) & 0xFF;

    case kRegMPYH:
        return (m_MPY >> 16) & 0xFF;

    default:
        LOGW(TAG, "Ignore ReadU8 at %06X", addr);
        assert(false);
        return 0;
    }
}

void Ppu::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr) {
    case kRegINIDISP: {
        bool forcedBlanking = value & (1 << 7);
        if (m_ForcedBlanking != forcedBlanking) {
            m_ForcedBlanking = forcedBlanking;
            LOGD(TAG, "ForcedBlanking is now %s", m_ForcedBlanking ? "enabled" : "disabled");
        }

        uint8_t brightness = value & 0b1111;
        if (m_Brightness != brightness) {
            m_Brightness = brightness;
            LOGD(TAG, "Brightness is now %d", m_Brightness);
        }
        break;
    }

    // VRAM registers
    case kRegVMAIN:
        LOGD(TAG, "VMAIN <= 0x%02X", value);
        m_VramIncrementHigh = value >> 7;

        // Some mode are not supported yet
        assert(!((value >> 2) & 0b11));

        m_VramIncrementStep = value & 0b11;
        break;

    case kRegVMADDL:
        m_VramAddress = (m_VramAddress & 0xFF00) | value;
        break;

    case kRegVMADDH:
        m_VramAddress = (m_VramAddress & 0xFF) | (value << 8);
        break;

    case kRegVMDATAL: {
        uint16_t address = m_VramAddress * 2;
        m_Vram[address] = value;

        if (!m_VramIncrementHigh) {
            incrementVramAddress();
        }
        break;
    }

    case kRegVMDATAH: {
        uint16_t address = m_VramAddress * 2 + 1;
        m_Vram[address] = value;

        if (m_VramIncrementHigh) {
            incrementVramAddress();
        }
        break;
    }

    // CGRAM registers
    case kRegCGADD:
        m_CgdataAddress = value;
        m_CgramLsbSet = false;
        m_CgramLsb = 0;
        break;

    case kRegCGDATA:
        if (m_CgramLsbSet) {
            m_Cgram[m_CgdataAddress] = ((value & 0x7F) << 8) | m_CgramLsb;

            m_CgdataAddress++;
            m_CgramLsbSet = false;
        } else {
            m_CgramLsb = value;
            m_CgramLsbSet = true;
        }
        break;

    // OAM registers
    case kRegOBJSEL:
        m_ObjSize = (value >> 5) & 0b11;

        m_ObjGapSize = convert4kWorkStep((value >> 3) & 0b11);
        m_ObjBase = convert8kWorkStep(value & 0b111);
        break;

    case kRegOAMADDL:
        m_OamAddress = (m_OamAddress & 0x100) | value;
        m_OamHighestPriorityObj = value >> 1;
        m_OamFlip = 0;
        break;

    case kRegOAMADDH:
        m_OamAddress = ((value & 1) << 8) | (m_OamAddress & 0xFF);
        m_OamForcedPriority = value >> 7;
        m_OamFlip = 0;
        break;

    case kRegOAMDATA: {
        if (!m_OamFlip) {
            m_OamWriteRegister = (m_OamWriteRegister & 0xFF00) | value;
        }

        if (m_OamAddress & 0x100) {
            int address = ((m_OamAddress & 0x10F) << 1) + (m_OamFlip & 1);
            m_Oam[address] = value;
        } else if (m_OamFlip) {
            m_OamWriteRegister = (value << 8) | (m_OamWriteRegister & 0xFF);

            int address = m_OamAddress << 1;
            m_Oam[address] = m_OamWriteRegister & 0xFF;
            m_Oam[address + 1] = m_OamWriteRegister >> 8;
        }

        m_OamFlip ^= 1;
        if (!m_OamFlip) {
            m_OamAddress++;
            m_OamAddress &= 0x1FF;
        }
        break;
    }

    // Background
    case kRegBGMODE: {
        int bgmode = value & 0b111;

        if (m_Bgmode != bgmode) {
            LOGI(TAG, "New BG mode: %d", bgmode);
            m_Bgmode = bgmode;
        }

        m_Bg3Priority = (value >> 3) & 1;

        for (size_t i = 0; i < SIZEOF_ARRAY(m_Backgrounds); i++) {
            m_Backgrounds[i].m_TileSize = (value >> (4 + i)) & 1;
        }

        break;
    }

    case kRegBG1SC:
    case kRegBG2SC:
    case kRegBG3SC:
    case kRegBG4SC: {
        int bgIdx = addr - kRegBG1SC;
        Background& bg = m_Backgrounds[bgIdx];

        bg.m_TilemapBase = convert1kWorkStep(value >> 2);
        bg.m_TilemapSize = value & 0b11;
        break;
    }

    case kRegBG12NBA: {
        m_Backgrounds[0].m_TileBase = convert4kWorkStep(value & 0b1111);
        m_Backgrounds[1].m_TileBase = convert4kWorkStep(value >> 4);
        break;
    }

    case kRegBG34NBA: {
        m_Backgrounds[2].m_TileBase = convert4kWorkStep(value & 0b1111);
        m_Backgrounds[3].m_TileBase = convert4kWorkStep(value >> 4);
        break;
    }

    case kRegBG1HOFS:
        m_Backgrounds[0].m_HOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[0].m_HOffset >> 8) & 7);
        m_OldBgByte = value;

        m_M7HOFS = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegBG1VOFS:
        m_Backgrounds[0].m_VOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;

        m_M7VOFS = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegBG2HOFS:
        m_Backgrounds[1].m_HOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[1].m_HOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG2VOFS:
        m_Backgrounds[1].m_VOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegBG3HOFS:
        m_Backgrounds[2].m_HOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[2].m_HOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG3VOFS:
        m_Backgrounds[2].m_VOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegBG4HOFS:
        m_Backgrounds[3].m_HOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[3].m_HOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG4VOFS:
        m_Backgrounds[3].m_VOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegTM:
        for (int i = 0; i < kBackgroundCount; i++) {
            m_MainScreenConfig.m_BgEnabled[i] = value & (1 << i);
        }

        m_MainScreenConfig.m_ObjEnabled = value & (1 << 4);
        break;

    case kRegTS:
        for (int i = 0; i < kBackgroundCount; i++) {
            m_SubScreenConfig.m_BgEnabled[i] = value & (1 << i);
        }

        m_SubScreenConfig.m_ObjEnabled = value & (1 << 4);
        break;

    case kRegWH0:
        m_Window1Config.m_Left = value;
        break;

    case kRegWH1:
        m_Window1Config.m_Right = value;
        break;

    case kRegWH2:
        m_Window2Config.m_Left = value;
        break;

    case kRegWH3:
        m_Window2Config.m_Right = value;
        break;

    case kRegW12SEL: {
        // BG1
        m_Window1Config.m_BackgroundConfig[0] = getWindowConfig(value & 0b11);
        m_Window2Config.m_BackgroundConfig[0] = getWindowConfig((value >> 2) & 0b11);

        // BG2
        m_Window1Config.m_BackgroundConfig[1] = getWindowConfig((value >> 4) & 0b11);
        m_Window2Config.m_BackgroundConfig[1] = getWindowConfig((value >> 6) & 0b11);
        break;
    }

    case kRegW34SEL:
        // BG3
        m_Window1Config.m_BackgroundConfig[2] = getWindowConfig(value & 0b11);
        m_Window2Config.m_BackgroundConfig[2] = getWindowConfig((value >> 2) & 0b11);

        // BG4
        m_Window1Config.m_BackgroundConfig[3] = getWindowConfig((value >> 4) & 0b11);
        m_Window2Config.m_BackgroundConfig[3] = getWindowConfig((value >> 6) & 0b11);
        break;

    case kRegWOBJSEL:
        // OBJ
        m_Window1Config.m_ObjConfig = getWindowConfig(value & 0b11);
        m_Window2Config.m_ObjConfig = getWindowConfig((value >> 2) & 0b11);

        // Math
        m_Window1Config.m_MathConfig = getWindowConfig((value >> 4) & 0b11);
        m_Window2Config.m_MathConfig = getWindowConfig((value >> 6) & 0b11);
        break;

    case kRegWBGLOG:
        for (int i = 0; i < kBackgroundCount; i++) {
            m_WindowLogicBackground[i] = static_cast<WindowLogic>((value >> (2 * i)) & 0b11);
        }

        break;

    case kRegWOBJLOG:
        m_WindowLogicObj = static_cast<WindowLogic>(value & 0b1);
        m_WindowLogicMath = static_cast<WindowLogic>((value >> 1) & 1);
        break;

    case kRegTMW:
        for (int i = 0; i < kBackgroundCount; i++) {
            m_MainScreenConfig.m_Window_BgDisable[i] = (value >> i) & 1;
        }

        m_MainScreenConfig.m_Window_ObjDisable = (value >> 4) & 1;

        break;

    case kRegTSW:
        for (int i = 0; i < kBackgroundCount; i++) {
            m_SubScreenConfig.m_Window_BgDisable[i] = (value >> i) & 1;
        }

        m_SubScreenConfig.m_Window_ObjDisable = (value >> 4) & 1;

        break;


    case kRegCGWSEL: {
        switch (value >> 6) {
        case 0:
            m_ForceMainScreenBlack = ColorMathConfig::Never;
            break;

        case 1:
            m_ForceMainScreenBlack = ColorMathConfig::NotMathWin;
            break;

        case 2:
            m_ForceMainScreenBlack = ColorMathConfig::MathWin;
            break;

        case 3:
            m_ForceMainScreenBlack = ColorMathConfig::Always;
            break;
        }

        switch ((value >> 4) & 0b11) {
        case 0:
            m_ColorMathEnabled = ColorMathConfig::Always;
            break;

        case 1:
            m_ColorMathEnabled = ColorMathConfig::MathWin;
            break;

        case 2:
            m_ColorMathEnabled = ColorMathConfig::NotMathWin;
            break;

        case 3:
            m_ColorMathEnabled = ColorMathConfig::Never;
            break;
        }

        m_SubscreenEnabled = value & (1 << 1);
        break;
    }

    case kRegCGADSUB:
        m_ColorMathOperation = (value >> 6) & 0b11;
        m_ColorMathBackdrop = (value >> 5) & 0b11;

        for (int i = 0; i < kBackgroundCount; i++) {
            m_ColorMathBackground[i] = (value >> i) & 1;
        }

        break;

    case kRegCOLDATA: {
        const uint8_t intensity = value & 0b11111;

        if (value & (1 << 7)) {
            // Blue
            m_SubscreenBackdrop &= 0b1111111111;
            m_SubscreenBackdrop |= intensity << 10;

        }

        if (value & (1 << 6)) {
            // Green
            m_SubscreenBackdrop &= 0b111110000011111;
            m_SubscreenBackdrop |= intensity << 5;
        }

        if (value & (1 << 5)) {
            // Red
            m_SubscreenBackdrop &= 0b111111111100000;
            m_SubscreenBackdrop |= intensity;
        }

        break;
    }

    case kRegMOSAIC:
        m_Mosaic.m_Size = ((value >> 4) & 0b1111) + 1;
        if (m_Mosaic.m_Size == 1) {
            for (int i = 0; i < kBackgroundCount; i++) {
                m_Mosaic.m_Backgrounds[i] = false;
            }
        } else {
            for (int i = 0; i < kBackgroundCount; i++) {
                m_Mosaic.m_Backgrounds[i] = (value >> i) & 1;
            }
        }

        break;

    case kRegM7A:
        m_M7A = (value << 8) | m_M7Old;
        m_M7Old = value;

        m_MPY = m_M7A * (m_M7B >> 8);
        break;

    case kRegM7B:
        m_M7B = (value << 8) | m_M7Old;
        m_M7Old = value;

        m_MPY = m_M7A * (m_M7B >> 8);
        break;

    case kRegM7C:
        m_M7C = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegM7D:
        m_M7D = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegM7X:
        m_M7X = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegM7Y:
        m_M7Y = (value << 8) | m_M7Old;
        m_M7Old = value;
        break;

    case kRegM7SEL:
        m_M7ScreenOver = (value >> 6) & 0b11;
        m_M7HFlip = value & 1;
        m_M7VFlip = (value >> 1) & 1;
        break;

    // To be implemented
    case kRegSETINI:
        break;

    case kRegRDCGRAM:
    default:
        LOGW(TAG, "Ignore WriteU8 %02X at %06X", value, addr);
        assert(false);
        break;
    }
}

uint32_t Ppu::getEvents() const
{
    return m_Events;
}

void Ppu::setDrawConfig(DrawConfig config)
{
    m_DrawConfig = config;
}

void Ppu::setHVIRQConfig(HVIRQConfig config, uint16_t H, uint16_t V)
{
    m_HVIRQ.m_Config = config;
    m_HVIRQ.m_H = H;
    m_HVIRQ.m_V = V;
}

void Ppu::incrementVramAddress()
{
    switch (m_VramIncrementStep) {
    case 0:
        m_VramAddress++;
        break;

    case 1:
        m_VramAddress += 32;
        break;

    case 2:
    case 3:
        m_VramAddress += 128;
        break;

    default:
        LOGC(TAG, "Unsupported increment step %d", m_VramIncrementStep);
        assert(false);
        break;
    }
}

//  tilemapSize is the tilemap size read in BGxSC registers
Ppu::TilemapMapper Ppu::getTilemapMapper(uint16_t tilemapSize) const
{
    static const TilemapMapper map[] = {
        tilemapMapper32x32,
        tilemapMapper64x32,
        tilemapMapper32x64,
        tilemapMapper64x64,
    };

    static_assert(SIZEOF_ARRAY(map) == 4);
    assert(tilemapSize <= SIZEOF_ARRAY(map));

    return map[tilemapSize];
}

void Ppu::updateTileData(const Background* bg, RendererBgInfo* renderBg)
{
    // Get tileinfo from tilemap
    // First we need to make a projection inside the submap
    // (The working tilemap is always a 32x32 one)
    uint32_t tilemapBase = renderBg->tilemapMapper(bg->m_TilemapBase, renderBg->tilemapX, renderBg->tilemapY);

    int subtilemapX = renderBg->tilemapX % kPpuTilemapWidth;
    int subtilemapY = renderBg->tilemapY % kPpuTilemapHeight;

    int tileinfoAddr = tilemapBase + (subtilemapY * kPpuTilemapWidth + subtilemapX) * kPpuTileInfoSize;
    uint16_t tileInfo = (m_Vram[tileinfoAddr + 1] << 8) | m_Vram[tileinfoAddr];

    // Parse tileinfo
    renderBg->verticalFlip = tileInfo >> 15;
    renderBg->horizontalFlip = (tileInfo >> 14) & 1;
    renderBg->priority = (tileInfo >> 13) & 1;
    renderBg->palette = (tileInfo >> 10) & 0b111;
    renderBg->tileIndex = tileInfo & 0b1111111111;

    // Get the real pixel coordinates
    // Use the flip status to point to the correct subtile
    // Then compute the subtile coordinate to be able to compute
    // the subtile location
    int realPixelX;
    if (!renderBg->horizontalFlip) {
        realPixelX = renderBg->tileWidthPixel - renderBg->tilePixelX - 1;
    } else {
        realPixelX = renderBg->tilePixelX;
    }

    int realPixelY;
    if (renderBg->verticalFlip) {
        realPixelY = renderBg->tileHeightPixel - renderBg->tilePixelY - 1;
    } else {
        realPixelY = renderBg->tilePixelY;
    }

    renderBg->subtileX = realPixelX / kPpuBaseTileWidth;
    renderBg->subtileY = realPixelY / kPpuBaseTileHeight;

    renderBg->subtilePixelX = realPixelX % kPpuBaseTileWidth;
    renderBg->subtilePixelY = realPixelY % kPpuBaseTileHeight;
}

void Ppu::updateSubtileData(const Background* bg, RendererBgInfo* renderBg)
{
    // The second row of tiles is located 0x10 tiles after
    int tileBaseAddr = bg->m_TileBase + renderBg->tileSize * renderBg->tileIndex;
    int tileAddr = tileBaseAddr + renderBg->subtileX * renderBg->tileSize + renderBg->subtileY * 0x10 * renderBg->tileSize;
    tileAddr &= 0xFFFF;

    // Point to the current tile lane
    const uint8_t* tileData = m_Vram + tileAddr;

    if (renderBg->tileBpp == 2) {
        // 0-1 bits are stored in a 8 words chunk, in the first plane
        renderBg->tileDataPlane0 = tileData + renderBg->subtilePixelY * 2;
        renderBg->tileDataPlane1 = nullptr;
    } else if (renderBg->tileBpp == 4) {
        // 0-1 bits are stored in a 8 words chunk, in the first plane
        renderBg->tileDataPlane0 = tileData + renderBg->subtilePixelY * 2;
        // 2-3 bits are stored in the second plane
        renderBg->tileDataPlane1 = renderBg->tileDataPlane0 + renderBg->tileSize / 2;
    } else if (renderBg->tileBpp == 8) {
        // 0-1 bits are stored in a 8 words chunk, in the first plane
        renderBg->tileDataPlane0 = tileData + renderBg->subtilePixelY * 2;
        // 2-3 bits are stored in the second plane
        renderBg->tileDataPlane1 = renderBg->tileDataPlane0 + 0x10;
        // 4-5 bits are stored in the third plane
        renderBg->tileDataPlane2 = renderBg->tileDataPlane1 + 0x10;
        // 6-7 bits are stored in the forth plane
        renderBg->tileDataPlane3 = renderBg->tileDataPlane2 + 0x10;
    } else {
        LOGE(TAG, "Unsupported %d bpp", renderBg->tileBpp);
        assert(false);
    }
}

bool Ppu::getScreenCurrentPixel(
    int x,
    int y,
    const ScreenConfig& screenConfig,
    uint32_t* color,
    Ppu::LayerPriority* priority)
{
    bool colorValid = false;

    for (size_t prioIdx = 0; !colorValid && m_RenderLayerPriority[prioIdx].m_Layer != Layer::none; prioIdx++) {
        const auto& layer = m_RenderLayerPriority[prioIdx];

        if (layer.m_Layer == Layer::background) {
            RendererBgInfo* renderBg = &m_RenderBgInfo[layer.m_BgIdx];
            colorValid = getBackgroundCurrentPixel(x, screenConfig, renderBg, layer.m_Priority, color);
        } else if (layer.m_Layer == Layer::sprite) {
            colorValid = getSpriteCurrentPixel(x, y, screenConfig, layer.m_Priority, color);
        }

        if (colorValid && priority) {
            *priority = layer;
        }
    }

    return colorValid;
}

bool Ppu::getBackgroundCurrentPixel(
    int x,
    const ScreenConfig& screenConfig,
    const RendererBgInfo* renderBg,
    int priority,
    uint32_t* color)
{
    auto background = renderBg->background;

    if (renderBg->priority != priority) {
        return false;
    }

    // Check if background is enabled
    if (!screenConfig.m_BgEnabled[renderBg->bgIdx]) {
        return false;
    }

    // Check if background is inside window
    bool pixelInWindow = false;

    if (screenConfig.m_Window_BgDisable[renderBg->bgIdx]) {
        pixelInWindow = applyWindowLogic(
            x,
            m_Window1Config.m_BackgroundConfig[renderBg->bgIdx],
            m_Window2Config.m_BackgroundConfig[renderBg->bgIdx],
            m_WindowLogicBackground[renderBg->bgIdx]);
    }

    if (pixelInWindow) {
        return false;
    }

    // Get pixel and draw it
    uint32_t tilePixelColor;

    tilePixelColor = (renderBg->tileDataPlane0[0] >> renderBg->subtilePixelX) & 1;
    tilePixelColor |= ((renderBg->tileDataPlane0[1] >> renderBg->subtilePixelX) & 1) << 1;

    if (renderBg->tileBpp == 4) {
        if (!renderBg->tileDataPlane1) {
            LOGW(TAG, "%s(): renderBg->tileDataPlane1 == nullptr", __func__);
            return false;
        }

        assert(renderBg->tileDataPlane1);
        tilePixelColor |= ((renderBg->tileDataPlane1[0] >> renderBg->subtilePixelX) & 1) << 2;
        tilePixelColor |= ((renderBg->tileDataPlane1[1] >> renderBg->subtilePixelX) & 1) << 3;
    } else if (renderBg->tileBpp == 8) {
        if (!renderBg->tileDataPlane1) {
            LOGW(TAG, "%s(): renderBg->tileDataPlane1 == nullptr", __func__);
            return false;
        }

        if (!renderBg->tileDataPlane2) {
            LOGW(TAG, "%s(): renderBg->tileDataPlane2 == nullptr", __func__);
            return false;
        }

        if (!renderBg->tileDataPlane3) {
            LOGW(TAG, "%s(): renderBg->tileDataPlane3 == nullptr", __func__);
            return false;
        }

        assert(renderBg->tileDataPlane1);
        tilePixelColor |= ((renderBg->tileDataPlane1[0] >> renderBg->subtilePixelX) & 1) << 2;
        tilePixelColor |= ((renderBg->tileDataPlane1[1] >> renderBg->subtilePixelX) & 1) << 3;

        assert(renderBg->tileDataPlane2);
        tilePixelColor |= ((renderBg->tileDataPlane2[0] >> renderBg->subtilePixelX) & 1) << 4;
        tilePixelColor |= ((renderBg->tileDataPlane2[1] >> renderBg->subtilePixelX) & 1) << 5;

        assert(renderBg->tileDataPlane3);
        tilePixelColor |= ((renderBg->tileDataPlane3[0] >> renderBg->subtilePixelX) & 1) << 6;
        tilePixelColor |= ((renderBg->tileDataPlane3[1] >> renderBg->subtilePixelX) & 1) << 7;
    }

    if (tilePixelColor == 0) {
        return false;
    }

    *color = getColorFromCgram(renderBg->bgIdx, renderBg->tileBpp, renderBg->palette, tilePixelColor);

    return true;
}

bool Ppu::getSpriteCurrentPixel(int x, int y, const ScreenConfig& screenConfig, int priority, uint32_t* c)
{
    if (!m_RenderObjInfo[y].m_ObjCount) {
        return false;
    }

    // Check if background is inside window
    bool pixelInWindow = false;

    if (screenConfig.m_Window_ObjDisable) {
        pixelInWindow = applyWindowLogic(
            x,
            m_Window1Config.m_ObjConfig,
            m_Window2Config.m_ObjConfig,
            m_WindowLogicObj);
    }

    if (pixelInWindow) {
        return false;
    }

    // Search sprite
    ObjProperty* prop = nullptr;

    for (int i = 0; i < m_RenderObjInfo[y].m_ObjCount; i++) {
        auto searchProp = m_RenderObjInfo[y].m_Obj[i];
        assert(searchProp);

        if (searchProp->m_Priority != priority) {
            continue;
        } else if (x < searchProp->m_X || searchProp->m_xEnd <= x) {
            continue;
        }

        prop = searchProp;

        // Sprite found, lets read the requested pixel
        // Define some callbacks to get some data depending of the current PPU configuration
        auto colorReadCb = getColorReadCb(kPpuObjBpp);

        // Extract the pixel coordinates in the tile
        int tileX = x - prop->m_X;
        int tileY = y - prop->m_Y;

        // Use the flip status to point to the correct subtile
        // Then compute the subtile coordinate to be able to compute
        // the subtile location
        int subtileX;
        if (prop->m_HorizontalFlip) {
            subtileX = prop->m_WidthPixel - tileX - 1;
        } else {
            subtileX = tileX;
        }

        int subtileY;
        if (prop->m_VerticalFlip) {
            subtileY = prop->m_HeightPixel - tileY - 1;
        } else {
            subtileY = tileY;
        }

        subtileX /= kPpuBaseTileWidth;
        subtileY /= kPpuBaseTileHeight;

        // Compute the final tile location
        // The second row of tiles is located 0x10 tiles after
        int tileBaseAddr = m_ObjBase + prop->m_TileIndex * kPpuObjTileSize;
        if (prop->m_TileIndex >= 0x100) {
            tileBaseAddr += m_ObjGapSize;
        }

        int tileAddr = tileBaseAddr + subtileX * kPpuObjTileSize + subtileY * 0x10 * kPpuObjTileSize;
        tileAddr &= 0xFFFF;

        const uint8_t* tileData = m_Vram + tileAddr;

        // Update coordinates before draw to apply some tile modifiers
        if (prop->m_VerticalFlip) {
            tileY = (kPpuBaseTileHeight - 1) - (tileY % kPpuBaseTileHeight);
        } else {
            tileY %= kPpuBaseTileHeight;
        }

        // Pixel horizontal order is reversed in the tile. Bit 7 is the first pixel
        if (!prop->m_HorizontalFlip) {
            tileX = (kPpuBaseTileWidth - 1) - (tileX % kPpuBaseTileWidth);
        } else {
            tileX %= kPpuBaseTileWidth;
        }

        // Read tile color and get the color from the palette
        int color = colorReadCb(tileData, kPpuObjTileSize, tileY, tileX);
        if (color == 0) {
            continue;
        }

        *c = getObjColorFromCgram(prop->m_Palette, color);

        return true;
    }

    return false;
}

void Ppu::moveToNextPixel(RendererBgInfo* renderBg)
{
    Background* bg = renderBg->background;

    // Go to the next pixel
    if (!renderBg->horizontalFlip) {
        renderBg->tilePixelX = (renderBg->tilePixelX + 1) % renderBg->tileWidthPixel;
        renderBg->subtilePixelX--;

        // There is only things to do when we've reached to end of the
        // current subtile
        if (renderBg->subtilePixelX < 0) {
            renderBg->subtilePixelX = kPpuBaseTileWidth - 1;
            renderBg->subtileX--;

            if (renderBg->subtileX >= 0) {
                updateSubtileData(bg, renderBg);
            } else {
                renderBg->tilemapX = (renderBg->tilemapX + 1) % renderBg->tilemapWidth;

                updateTileData(bg, renderBg);
                updateSubtileData(bg, renderBg);
            }
        }
    } else {
        renderBg->tilePixelX = (renderBg->tilePixelX + 1) % renderBg->tileWidthPixel;
        renderBg->subtilePixelX++;

        // There is only things to do when we've reached to end of the
        // current subtile
        if (renderBg->subtilePixelX == kPpuBaseTileWidth) {
            renderBg->subtilePixelX = 0;
            renderBg->subtileX++;

            if (renderBg->subtileX < renderBg->tileWidth) {
                updateSubtileData(bg, renderBg);
            } else {
                renderBg->tilemapX = (renderBg->tilemapX + 1) % renderBg->tilemapWidth;

                updateTileData(bg, renderBg);
                updateSubtileData(bg, renderBg);
            }
        }
    }
}

void Ppu::setHVIRQ(int x, int y)
{
    if (m_HVIRQ.m_Config == HVIRQConfig::Disable) {
        return;
    }

    switch (m_HVIRQ.m_Config) {
    case HVIRQConfig::H:
        if (x == m_HVIRQ.m_H) {
            m_Events |= Event_HV_IRQ;
        }

        break;

    case HVIRQConfig::V:
        if (x == 0 && y == m_HVIRQ.m_V) {
            m_Events |= Event_HV_IRQ;
        }

        break;

    case HVIRQConfig::HV:
        if (x == m_HVIRQ.m_H && y == m_HVIRQ.m_V) {
            m_Events |= Event_HV_IRQ;
        }

        break;

    default:
        break;
    }
}

int Ppu::run()
{
    m_Events = 0;

    if (m_RenderX == 0) {
        if (m_RenderY == 0) {
            // Start of screen
            initScreenRender();
            m_Events |= Event_ScanStarted;
        } else if (m_RenderY == kPpuDisplayHeight) {
            // V-Blank
            m_Events |= Event_VBlankStart;
        } else if (m_RenderY < kPpuDisplayHeight) {
            // New line
            initLineRender(m_RenderY);
            renderDot(m_RenderX, m_RenderY);

            m_Events |= Event_HBlankEnd;
        }
    } else if (m_RenderX >= kPpuDisplayWidth) {
        // H-Blank
        if (m_RenderX == kPpuDisplayWidth) {
            m_Events |= Event_HBlankStart;
        }

    } else if (m_RenderY >= kPpuDisplayHeight) {
        // V-Blank
    } else {
        renderDot(m_RenderX, m_RenderY);
    }

    setHVIRQ(m_RenderX, m_RenderY);
    m_RenderX++;

    if (m_RenderX == kPpuScanWidth) {
        m_RenderX = 0;
        m_RenderY = (m_RenderY + 1) % kPpuScanHeight;

        if (m_RenderY == 0) {
            m_Events |= Event_ScanEnded;
        }
    }

    return kTimingPpuDot;
}

void Ppu::initScreenRender()
{
    if (m_DrawConfig != DrawConfig::Draw) {
        return;
    }

    if (m_Bgmode == 7) {
        initScreenRenderMode7();
        return;
    }

    // Prepare background rendering
    const size_t bgCount = getBackgroundCountFromMode(m_Bgmode);
    static_assert(SIZEOF_ARRAY(m_Backgrounds) == SIZEOF_ARRAY(m_RenderBgInfo));

    for (size_t bgIdx = 0; bgIdx < bgCount; bgIdx++) {
        RendererBgInfo* renderBg = &m_RenderBgInfo[bgIdx];
        Background* bg = &m_Backgrounds[bgIdx];

        renderBg->bgIdx = bgIdx;
        renderBg->background = bg;

        // Compute some dimensions that will be ready for future use
        getTilemapDimension(bg->m_TilemapSize, &renderBg->tilemapWidth, &renderBg->tilemapHeight);
        renderBg->tilemapWidthPixel = renderBg->tilemapWidth * kPpuBaseTileWidth;
        renderBg->tilemapHeightPixel = renderBg->tilemapHeight * kPpuBaseTileHeight;

        getTileDimension(bg->m_TileSize, &renderBg->tileWidth, &renderBg->tileHeight);
        renderBg->tileWidthPixel = renderBg->tileWidth * kPpuBaseTileWidth;
        renderBg->tileHeightPixel = renderBg->tileHeight * kPpuBaseTileHeight;

        renderBg->tileBpp = getTileBppFromMode(m_Bgmode, bgIdx);

        // Compute the tile size in bytes. Tiles are always 8x8.
        // 16x16 tiles are just a composition of multiple 8x8 tiles.
        renderBg->tileSize = renderBg->tileBpp * 8;

        renderBg->tilemapMapper = getTilemapMapper(bg->m_TilemapSize);

        // Setup mosaic if enabled
        if (m_Mosaic.m_Size > 1 && m_Mosaic.m_Backgrounds[bgIdx]) {
            renderBg->mosaic.startX = 1;
            renderBg->mosaic.startY = 0;
            renderBg->mosaic.size = m_Mosaic.m_Size;
        } else {
            renderBg->mosaic.size = 0;
        }
    }

    // Prepare sprites
    for (size_t i = 0; i < SIZEOF_ARRAY(m_RenderObjInfo); i++) {
        m_RenderObjInfo[i].m_ObjCount = 0;
    }

    loadObjs();
}

void Ppu::initLineRender(int y)
{
    if (m_DrawConfig != DrawConfig::Draw) {
        return;
    }

    if (m_Bgmode == 7) {
        initLineRenderMode7(y);
        return;
    }

    const size_t bgCount = getBackgroundCountFromMode(m_Bgmode);

    // Prepare background rendering for the current line.
    // At the very end, the RendererBgInfo will provide the correct data to
    // display pixel (0, y)
    for (size_t i = 0; i < bgCount; i++) {
        RendererBgInfo* renderBg = &m_RenderBgInfo[i];
        Background* bg = &m_Backgrounds[i];
        int renderY; // Line to render. Can be tweaked when mosaic is enabled

        // Handle mosaic
        if (renderBg->mosaic.size > 1) {
            renderBg->mosaic.startX = 1;

            // Redraw the same line N times (size block)
            const int nextBlockY = renderBg->mosaic.startY + renderBg->mosaic.size;
            if (y == nextBlockY) {
                renderBg->mosaic.startY = y;
            }

            renderY = renderBg->mosaic.startY;
        } else {
            renderY = y;
        }

        // Compute background start coordinates in pixels at first
        int bgX = bg->m_HOffset % renderBg->tilemapWidthPixel;
        int bgY = (bg->m_VOffset + renderY) % renderBg->tilemapHeightPixel;

        // Get the tile coordinates inside the tilemap
        renderBg->tilemapX = bgX / renderBg->tileWidthPixel;
        renderBg->tilemapY = bgY / renderBg->tileHeightPixel;

        // Get the pixel coordinates inside the tile.
        // Doesn't take account of horizontal/vertical flip (info unknown at this point)
        renderBg->tilePixelX = bgX % renderBg->tileWidthPixel;
        renderBg->tilePixelY = bgY % renderBg->tileHeightPixel;

        // Prepare work info
        updateTileData(bg, renderBg);
        updateSubtileData(bg, renderBg);
    }

    if (m_Bgmode == 0) {
        m_RenderLayerPriority = s_LayerPriorityMode0;
    } else if (m_Bgmode == 1) {
        m_RenderLayerPriority = m_Bg3Priority ? s_LayerPriorityMode1_BG3_On : s_LayerPriorityMode1_BG3_Off;
    } else if (m_Bgmode == 3) {
        m_RenderLayerPriority = s_LayerPriorityMode3;
    } else if (m_Bgmode == 7) {
        m_RenderLayerPriority = s_LayerPriorityMode7;
    } else {
        m_RenderLayerPriority = nullptr;
    }
}

void Ppu::renderDot(int x, int y)
{
    if (m_ForcedBlanking) {
        m_RenderCb({0, 0, 0});
        return;
    }

    if (m_DrawConfig != DrawConfig::Draw) {
        return;
    }

    // Mode not supported
    if (!m_RenderLayerPriority) {
        return;
    }

    if (m_Bgmode == 7) {
        renderDotMode7(x, y);
        return;
    }

    // Render MainScreen
    Ppu::LayerPriority priority;
    uint32_t rawColor;
    bool colorValid;

    colorValid = getScreenCurrentPixel(
        x,
        y,
        m_MainScreenConfig,
        &rawColor,
        &priority);

    // Check if color math is enabled and pixel is in the window
    bool insideMathWindow;

    if (m_ColorMathEnabled == ColorMathConfig::Never) {
        insideMathWindow = false;
    } else if (m_ColorMathEnabled == ColorMathConfig::Always) {
        insideMathWindow = true;
    } else {
        insideMathWindow = applyWindowLogic(
            x,
            m_Window1Config.m_MathConfig,
            m_Window2Config.m_MathConfig,
            m_WindowLogicMath);

        if (m_ColorMathEnabled == ColorMathConfig::NotMathWin) {
            insideMathWindow = !insideMathWindow;
        }
    }

    // Check if color math should be applied to this color
    bool doMath;

    if (insideMathWindow) {
        if (!colorValid) {
            doMath = m_ColorMathBackdrop;
        } else if (priority.m_Layer == Ppu::Layer::background) {
            doMath = m_ColorMathBackground[priority.m_BgIdx];
        } else {
            doMath = false;
        }
    } else {
        doMath = false;
    }

    // Apply math if enabled
    if (doMath) {
        // Force backdrop if color is invalid
        if (!colorValid) {
            rawColor = getMainBackdropColor();
        }

        // Render SubScreen
        uint32_t subscreenRawColor;

        if (m_SubscreenEnabled) {
            bool subscreenColorValid;

            subscreenColorValid = getScreenCurrentPixel(
                x,
                y,
                m_SubScreenConfig,
                &subscreenRawColor,
                nullptr);

            if (!subscreenColorValid) {
                subscreenRawColor = m_SubscreenBackdrop;
            }
        } else {
            subscreenRawColor = m_SubscreenBackdrop;
        }

        struct SplittedRawColor {
            uint32_t r;
            uint32_t g;
            uint32_t b;

            static SplittedRawColor fromU32(uint32_t c) {
                return {
                    c & 0b11111,
                    (c >> 5) & 0b11111,
                    (c >> 10) & 0b11111,
                };
            }

            uint32_t toU32() const {
                constexpr int kMask = 0b11111;
                uint32_t c;

                c = r & kMask;
                c |= (g & kMask) << 5;
                c |= (b & kMask) << 10;

                return c;
            }
        };

        SplittedRawColor splittedMainColor = SplittedRawColor::fromU32(rawColor);
        SplittedRawColor splittedSubColor = SplittedRawColor::fromU32(subscreenRawColor);

        // Add/Sub
        if (m_ColorMathOperation & (1 << 1)) {
            auto subCb = [](uint32_t a, uint32_t b) -> uint32_t {
                if (b > a) {
                    return 0;
                } else {
                    return a - b;
                }
            };

            splittedMainColor.r = subCb(splittedMainColor.r, splittedSubColor.r);
            splittedMainColor.g = subCb(splittedMainColor.g, splittedSubColor.g);
            splittedMainColor.b = subCb(splittedMainColor.b, splittedSubColor.b);
        } else {
            splittedMainColor.r = splittedMainColor.r + splittedSubColor.r;
            splittedMainColor.g = splittedMainColor.g + splittedSubColor.g;
            splittedMainColor.b = splittedMainColor.b + splittedSubColor.b;
        }

        // Divide
        if (m_ColorMathOperation & 1) {
            splittedMainColor.r /= 2;
            splittedMainColor.g /= 2;
            splittedMainColor.b /= 2;
        }

        // Repack into raw color + saturate
        rawColor = splittedMainColor.toU32();

        // Use backdrop colors if required
        // 1. MainScreen
        // 2. SubScreen
        if (!rawColor) {
            rawColor = getMainBackdropColor();

            if (!rawColor) {
                rawColor = m_SubscreenBackdrop;
            }
        }
    } else if (!colorValid) {
        rawColor = getMainBackdropColor();
    }

    // Compute final color
    SnesColor color = rawColorToRgb(rawColor);
    applyBrightness(&color, m_Brightness);

    m_RenderCb(color);

    // Move to next pixel
    const size_t bgCount = getBackgroundCountFromMode(m_Bgmode);
    for (size_t i = 0; i < bgCount; i++) {
        RendererBgInfo* renderBg = &m_RenderBgInfo[i];

        // If mosaic is enabled, redraw the same line N times
        if (renderBg->mosaic.size > 1) {
            const int nextBlockX = renderBg->mosaic.startX + renderBg->mosaic.size;
            if (x == nextBlockX) {
                renderBg->mosaic.startX = x;

                // Move to the next block start
                for (int j = 0; j < renderBg->mosaic.size; j++) {
                    moveToNextPixel(renderBg);
                }
            }
        } else {
            moveToNextPixel(renderBg);
        }
    }
}

void Ppu::loadObjs()
{
    int firstObj = m_OamForcedPriority ? m_OamHighestPriorityObj : 0;
    int i = firstObj;

    do {
        ObjProperty& prop = m_Objs[i];

        // Attributes are 4 bytes long
        const uint8_t* objBase = m_Oam + i * 4;

        // Other parts of the attributes are located after the first 512 bytes.
        // Each byte has 2 bits for 4 sprites. Bytes are ordered like this:
        // | 3 2 1 0 | 7 6 5 4 | 11 10 9 8 | ... |
        uint8_t extra = m_Oam[512 + i / 4];
        extra >>= 2 * (i % 4);
        extra &= 0b11;

        prop.m_X = objBase[0];

        // Decode sprite coordinates
        if (extra & 1) {
            // The extra byte is a sign number
            // Craft a negative int16 and decode it
            prop.m_X |= 0xFF00;
        }

        prop.m_Y = objBase[1];

        // Decode sprite size
        prop.m_Size = (extra >> 1) & 1;

        // Decode attributes (third byte)
        prop.m_VerticalFlip = (objBase[3] >> 7) & 1;
        prop.m_HorizontalFlip = (objBase[3] >> 6) & 1;
        prop.m_Priority = (objBase[3] >> 4) & 0b11;
        prop.m_Palette = (objBase[3] >> 1) & 0b111;

        // Decode tile index (upper bit is within attribute byte)
        prop.m_TileIndex = ((objBase[3] & 1) << 8) | objBase[2];

        // Compute other data
        getSpriteSize(m_ObjSize, prop.m_Size, &prop.m_Width, &prop.m_Height);
        prop.m_HeightPixel = prop.m_Height * kPpuBaseTileHeight;
        prop.m_WidthPixel = prop.m_Width * kPpuBaseTileWidth;

        prop.m_xEnd = prop.m_X + prop.m_WidthPixel;

        if (prop.m_X + prop.m_WidthPixel < 0) {
            prop.m_OnScreen = false;
        } else if (prop.m_X > kPpuDisplayWidth) {
            prop.m_OnScreen = false;
        } else if (prop.m_Y > kPpuDisplayHeight) {
            prop.m_OnScreen = false;
        } else {
            prop.m_OnScreen = true;
        }

        if (prop.m_OnScreen) {
            // Save the lines where the sprite could be displayed
            for (int16_t i = 0; i < prop.m_HeightPixel; i++) {
                int16_t y = prop.m_Y + i;
                if (y >= kPpuDisplayHeight) {
                    break;
                }

                assert(static_cast<size_t>(i) < SIZEOF_ARRAY(m_RenderObjInfo));

                m_RenderObjInfo[y].m_Obj[m_RenderObjInfo[y].m_ObjCount] = &prop;
                m_RenderObjInfo[y].m_ObjCount++;
            }
        }

        i = (i + 1) % kObjCount;
    } while (i != firstObj);
}

void Ppu::printObjsCoordinates()
{
    int spriteWidth;
    int spriteHeight;

    for (int i = 0; i < kObjCount; i++) {
        getSpriteSize(m_ObjSize, m_Objs[i].m_Size, &spriteWidth, &spriteHeight);
        LOGI(TAG, "%d - Pos: %dx%d Size: %dx%d Char: %d",
             i, m_Objs[i].m_X, m_Objs[i].m_Y,
             spriteWidth * kPpuBaseTileWidth, spriteHeight * kPpuBaseTileHeight,
             m_Objs[i].m_TileIndex);
    }
}

uint32_t Ppu::getColorFromCgram(int bgIdx, int tileBpp, int palette, int color)
{
    if (m_Bgmode == 0) {
        // In mode 0, all bg have a dedicated 0x20 bytes palette
        return m_Cgram[bgIdx * 0x20 + palette * 4 + color];
    } else if (m_Bgmode == 1) {
        return m_Cgram[palette * (1 << tileBpp) + color];
    } else if (m_Bgmode == 3) {
        if (bgIdx == 0) {
            return m_Cgram[color];
        } else if (bgIdx == 1) {
            return m_Cgram[palette * (1 << tileBpp) + color];
        } else {
            assert(false);
            return 0;
        }
    } else if (m_Bgmode == 7) {
        return m_Cgram[color];
    } else {
        assert(false);
        return 0;
    }
}

uint32_t Ppu::getObjColorFromCgram(int palette, int color)
{
    return m_Cgram[kPpuObjPaletteOffset + palette * (1 << kPpuObjBpp) + color];
}

uint32_t Ppu::getMainBackdropColor()
{
    return m_Cgram[0];
}

bool Ppu::getPixelFromBg(int bgIdx, const Background* bg, int screen_x, int screen_y, SnesColor* c, int* out_priority)
{
    // Compute some dimensions that will be ready for future use
    int tilemapWidth;
    int tilemapHeight;
    getTilemapDimension(bg->m_TilemapSize, &tilemapWidth, &tilemapHeight);
    LOGD(TAG, "Tilemap is %dx%d", tilemapWidth, tilemapHeight);

    int tileWidth;
    int tileHeight;
    getTileDimension(bg->m_TileSize, &tileWidth, &tileHeight);

    int tileBpp = getTileBppFromMode(m_Bgmode, bgIdx);
    LOGD(TAG, "Tiles are (%dx8)x(%dx8) (%dbpp)", tileWidth, tileHeight, tileBpp);

    // Compute the tile size in bytes. Tiles are always 8x8.
    // 16x16 tiles are just a composition of multiple 8x8 tiles.
    int tileSize = tileBpp * 8;

    // Define some callbacks to get some data depending of the current PPU configuration
    auto tilemapMapper = getTilemapMapper(bg->m_TilemapSize);
    auto colorReadCb = getColorReadCb(tileBpp);

    // Compute background coordinates in pixels at first
    int bgX = (bg->m_HOffset + screen_x) % (tilemapWidth * kPpuBaseTileWidth);
    int bgY = (bg->m_VOffset + screen_y) % (tilemapHeight * kPpuBaseTileHeight);

    // Extract
    // 1. The pixel coordinates in the tile
    // 2. The tile coordinates inside the tilemap
    int tileX = bgX % (tileWidth * kPpuBaseTileWidth);
    int tileY = bgY % (tileHeight * kPpuBaseTileHeight);

    bgX /= tileWidth * kPpuBaseTileWidth;
    bgY /= tileHeight * kPpuBaseTileHeight;

    // Make a projection inside the submap
    // The working tilemap is a 32x32 one
    uint32_t tilemapBase = tilemapMapper(bg->m_TilemapBase, bgX, bgY);

    // Get tileinfo from tilemap
    int tilemapX = bgX % kPpuTilemapWidth;
    int tilemapY = bgY % kPpuTilemapHeight;

    int tileinfoAddr = tilemapBase + (tilemapY * kPpuTilemapWidth + tilemapX) * kPpuTileInfoSize;
    uint16_t tileInfo = m_Vram[tileinfoAddr] | (m_Vram[tileinfoAddr + 1] << 8);

    // Parse tileinfo
    int tilePropVerticalFlip = tileInfo >> 15;
    int tilePropHorizontalFlip = (tileInfo >> 14) & 1;
    int tilePropPriority = (tileInfo >> 13) & 1;
    int tilePropPalette = (tileInfo >> 10) & 0b111;

    int tileIndex = tileInfo & 0b1111111111;
    int tileBaseAddr = bg->m_TileBase + tileSize * tileIndex;

    // Use the flip status to point to the correct subtile
    // Then compute the subtile coordinate to be able to compute
    // the subtile location
    int subtileX;
    if (tilePropHorizontalFlip) {
        subtileX = (tileWidth * kPpuBaseTileWidth) - tileX - 1;
    } else {
        subtileX = tileX;
    }

    int subtileY;
    if (tilePropVerticalFlip) {
        subtileY = (tileHeight * kPpuBaseTileWidth) - tileY - 1;
    } else {
        subtileY = tileY;
    }

    subtileX /= kPpuBaseTileWidth;
    subtileY /= kPpuBaseTileHeight;

    // Compute the final tile location
    // The second row of tiles is located 0x10 tiles after
    int tileAddr = tileBaseAddr + subtileX * tileSize + subtileY * 0x10 * tileSize;
    tileAddr &= 0xFFFF;

    const uint8_t* tileData = m_Vram + tileAddr;

    // Update coordinates before draw to apply some tile modifiers
    if (tilePropVerticalFlip) {
        tileY = (kPpuBaseTileHeight - 1) - (tileY % kPpuBaseTileHeight);
    } else {
        tileY %= kPpuBaseTileHeight;
    }

    // Pixel horizontal order is reversed in the tile. Bit 7 is the first pixel
    if (!tilePropHorizontalFlip) {
        tileX = (kPpuBaseTileWidth - 1) - (tileX % kPpuBaseTileWidth);
    } else {
        tileX %= kPpuBaseTileWidth;
    }

    // Read tile color and get the color from the palette
    int color = colorReadCb(tileData, tileSize, tileY, tileX);
    if (color == 0) {
        return false;
    }

    uint32_t rawColor = getColorFromCgram(bgIdx, tileBpp, tilePropPalette, color);
    *c = rawColorToRgb(rawColor);
    *out_priority = tilePropPriority;

    return true;
}

bool Ppu::getPixelFromObj(int screenX, int screenY, SnesColor* c, int* outPriority)
{
    const ObjProperty* obj = nullptr;
    int spriteWidth;
    int spriteHeight;

    // Find the sprite to display
    for (int i = kObjCount - 1 ; i >= 0; i--) {
        getSpriteSize(m_ObjSize, m_Objs[i].m_Size, &spriteWidth, &spriteHeight);

        // Test if the pixel is within the current sprite
        if (screenX < m_Objs[i].m_X) {
            continue;
        } else if (screenX >= m_Objs[i].m_X + spriteWidth * kPpuBaseTileWidth) {
            continue;
        } else if (screenY < m_Objs[i].m_Y) {
            continue;
        } else if (screenY >= m_Objs[i].m_Y + spriteHeight * kPpuBaseTileHeight) {
            continue;
        }

        obj = &m_Objs[i];
        break;
    }

    if (!obj) {
        return false;
    }

    // Sprint found, lets read the requested pixel
    // Define some callbacks to get some data depending of the current PPU configuration
    auto colorReadCb = getColorReadCb(kPpuObjBpp);

    // Extract the pixel coordinates in the tile
    int tileX = screenX - obj->m_X;
    int tileY = screenY - obj->m_Y;

    // Use the flip status to point to the correct subtile
    // Then compute the subtile coordinate to be able to compute
    // the subtile location
    int subtileX;
    if (obj->m_HorizontalFlip) {
        subtileX = (spriteWidth * kPpuBaseTileWidth) - tileX - 1;
    } else {
        subtileX = tileX;
    }

    int subtileY;
    if (obj->m_VerticalFlip) {
        subtileY = (spriteHeight * kPpuBaseTileHeight) - tileY - 1;
    } else {
        subtileY = tileY;
    }

    subtileX /= kPpuBaseTileWidth;
    subtileY /= kPpuBaseTileHeight;

    // Compute the final tile location
    // The second row of tiles is located 0x10 tiles after
    int tileBaseAddr = m_ObjBase + obj->m_TileIndex * kPpuObjTileSize;
    if (obj->m_TileIndex >= 0x100) {
        tileBaseAddr += m_ObjGapSize;
    }

    int tileAddr = tileBaseAddr + subtileX * kPpuObjTileSize + subtileY * 0x10 * kPpuObjTileSize;
    tileAddr &= 0xFFFF;

    const uint8_t* tileData = m_Vram + tileAddr;

    // Update coordinates before draw to apply some tile modifiers
    if (obj->m_VerticalFlip) {
        tileY = (kPpuBaseTileHeight - 1) - (tileY % kPpuBaseTileHeight);
    } else {
        tileY %= kPpuBaseTileHeight;
    }

    // Pixel horizontal order is reversed in the tile. Bit 7 is the first pixel
    if (!obj->m_HorizontalFlip) {
        tileX = (kPpuBaseTileWidth - 1) - (tileX % kPpuBaseTileWidth);
    } else {
        tileX %= kPpuBaseTileWidth;
    }

    // Read tile color and get the color from the palette
    int color = colorReadCb(tileData, kPpuObjTileSize, tileY, tileX);
    if (color == 0) {
        return false;
    }

    uint32_t rawColor = getObjColorFromCgram(obj->m_Palette, color);
    *c = rawColorToRgb(rawColor);
    *outPriority = obj->m_Priority;

    return true;
}

void Ppu::dumpToFile(FILE* f)
{
    SchedulerTask::dumpToFile(f);

    fwrite(&m_ForcedBlanking, sizeof(m_ForcedBlanking), 1, f);
    fwrite(&m_Brightness, sizeof(m_Brightness), 1, f);
    fwrite(&m_VramIncrementHigh, sizeof(m_VramIncrementHigh), 1, f);
    fwrite(&m_VramIncrementStep, sizeof(m_VramIncrementStep), 1, f);
    fwrite(m_Vram, sizeof(m_Vram), 1, f);
    fwrite(&m_VramAddress, sizeof(m_VramAddress), 1, f);
    fwrite(&m_Cgram, sizeof(m_Cgram), 1, f);
    fwrite(&m_CgdataAddress, sizeof(m_CgdataAddress), 1, f);
    fwrite(&m_CgramLsbSet, sizeof(m_CgramLsbSet), 1, f);
    fwrite(&m_CgramLsb, sizeof(m_CgramLsb), 1, f);
    fwrite(&m_Oam, sizeof(m_Oam), 1, f);
    fwrite(&m_OamAddress, sizeof(m_OamAddress), 1, f);
    fwrite(&m_OamHighestPriorityObj, sizeof(m_OamHighestPriorityObj), 1, f);
    fwrite(&m_OamForcedPriority, sizeof(m_OamForcedPriority), 1, f);
    fwrite(&m_OamFlip, sizeof(m_OamFlip), 1, f);
    fwrite(&m_OamWriteRegister, sizeof(m_OamWriteRegister), 1, f);
    fwrite(&m_ObjSize, sizeof(m_ObjSize), 1, f);
    fwrite(&m_ObjGapSize, sizeof(m_ObjGapSize), 1, f);
    fwrite(&m_ObjBase, sizeof(m_ObjBase), 1, f);
    fwrite(&m_Backgrounds, sizeof(m_Backgrounds), 1, f);
    fwrite(&m_OldBgByte, sizeof(m_OldBgByte), 1, f);
    fwrite(&m_Bgmode, sizeof(m_Bgmode), 1, f);
    fwrite(&m_Bg3Priority, sizeof(m_Bg3Priority), 1, f);
    fwrite(&m_SubscreenBackdrop, sizeof(m_SubscreenBackdrop), 1, f);
    fwrite(&m_Window1Config, sizeof(m_Window1Config), 1, f);
    fwrite(&m_Window2Config, sizeof(m_Window2Config), 1, f);
    fwrite(&m_WindowLogicBackground, sizeof(m_WindowLogicBackground), 1, f);
    fwrite(&m_WindowLogicObj, sizeof(m_WindowLogicObj), 1, f);
    fwrite(&m_WindowLogicMath, sizeof(m_WindowLogicMath), 1, f);
    fwrite(&m_MainScreenConfig, sizeof(m_MainScreenConfig), 1, f);
    fwrite(&m_SubScreenConfig, sizeof(m_SubScreenConfig), 1, f);
    fwrite(&m_ForceMainScreenBlack, sizeof(m_ForceMainScreenBlack), 1, f);
    fwrite(&m_ColorMathEnabled, sizeof(m_ColorMathEnabled), 1, f);
    fwrite(&m_SubscreenEnabled, sizeof(m_SubscreenEnabled), 1, f);
    fwrite(&m_ColorMathOperation, sizeof(m_ColorMathOperation), 1, f);
    fwrite(&m_ColorMathBackground, sizeof(m_ColorMathBackground), 1, f);
    fwrite(&m_ColorMathBackdrop, sizeof(m_ColorMathBackdrop), 1, f);
    fwrite(&m_Mosaic, sizeof(m_Mosaic), 1, f);
    fwrite(&m_M7ScreenOver, sizeof(m_M7ScreenOver), 1, f);
    fwrite(&m_M7HFlip, sizeof(m_M7HFlip), 1, f);
    fwrite(&m_M7VFlip, sizeof(m_M7VFlip), 1, f);
    fwrite(&m_M7Old, sizeof(m_M7Old), 1, f);
    fwrite(&m_M7HOFS, sizeof(m_M7HOFS), 1, f);
    fwrite(&m_M7VOFS, sizeof(m_M7VOFS), 1, f);
    fwrite(&m_M7A, sizeof(m_M7A), 1, f);
    fwrite(&m_M7B, sizeof(m_M7B), 1, f);
    fwrite(&m_M7C, sizeof(m_M7C), 1, f);
    fwrite(&m_M7D, sizeof(m_M7D), 1, f);
    fwrite(&m_M7X, sizeof(m_M7X), 1, f);
    fwrite(&m_M7Y, sizeof(m_M7Y), 1, f);
    fwrite(&m_MPY, sizeof(m_MPY), 1, f);
}

void Ppu::loadFromFile(FILE* f)
{
    SchedulerTask::loadFromFile(f);

    fread(&m_ForcedBlanking, sizeof(m_ForcedBlanking), 1, f);
    fread(&m_Brightness, sizeof(m_Brightness), 1, f);
    fread(&m_VramIncrementHigh, sizeof(m_VramIncrementHigh), 1, f);
    fread(&m_VramIncrementStep, sizeof(m_VramIncrementStep), 1, f);
    fread(m_Vram, sizeof(m_Vram), 1, f);
    fread(&m_VramAddress, sizeof(m_VramAddress), 1, f);
    fread(&m_Cgram, sizeof(m_Cgram), 1, f);
    fread(&m_CgdataAddress, sizeof(m_CgdataAddress), 1, f);
    fread(&m_CgramLsbSet, sizeof(m_CgramLsbSet), 1, f);
    fread(&m_CgramLsb, sizeof(m_CgramLsb), 1, f);
    fread(&m_Oam, sizeof(m_Oam), 1, f);
    fread(&m_OamAddress, sizeof(m_OamAddress), 1, f);
    fread(&m_OamHighestPriorityObj, sizeof(m_OamHighestPriorityObj), 1, f);
    fread(&m_OamForcedPriority, sizeof(m_OamForcedPriority), 1, f);
    fread(&m_OamFlip, sizeof(m_OamFlip), 1, f);
    fread(&m_OamWriteRegister, sizeof(m_OamWriteRegister), 1, f);
    fread(&m_ObjSize, sizeof(m_ObjSize), 1, f);
    fread(&m_ObjGapSize, sizeof(m_ObjGapSize), 1, f);
    fread(&m_ObjBase, sizeof(m_ObjBase), 1, f);
    fread(&m_Backgrounds, sizeof(m_Backgrounds), 1, f);
    fread(&m_OldBgByte, sizeof(m_OldBgByte), 1, f);
    fread(&m_Bgmode, sizeof(m_Bgmode), 1, f);
    fread(&m_Bg3Priority, sizeof(m_Bg3Priority), 1, f);
    fread(&m_SubscreenBackdrop, sizeof(m_SubscreenBackdrop), 1, f);
    fread(&m_Window1Config, sizeof(m_Window1Config), 1, f);
    fread(&m_Window2Config, sizeof(m_Window2Config), 1, f);
    fread(&m_WindowLogicBackground, sizeof(m_WindowLogicBackground), 1, f);
    fread(&m_WindowLogicObj, sizeof(m_WindowLogicObj), 1, f);
    fread(&m_WindowLogicMath, sizeof(m_WindowLogicMath), 1, f);
    fread(&m_MainScreenConfig, sizeof(m_MainScreenConfig), 1, f);
    fread(&m_SubScreenConfig, sizeof(m_SubScreenConfig), 1, f);
    fread(&m_ForceMainScreenBlack, sizeof(m_ForceMainScreenBlack), 1, f);
    fread(&m_ColorMathEnabled, sizeof(m_ColorMathEnabled), 1, f);
    fread(&m_SubscreenEnabled, sizeof(m_SubscreenEnabled), 1, f);
    fread(&m_ColorMathOperation, sizeof(m_ColorMathOperation), 1, f);
    fread(&m_ColorMathBackground, sizeof(m_ColorMathBackground), 1, f);
    fread(&m_ColorMathBackdrop, sizeof(m_ColorMathBackdrop), 1, f);
    fread(&m_Mosaic, sizeof(m_Mosaic), 1, f);
    fread(&m_M7ScreenOver, sizeof(m_M7ScreenOver), 1, f);
    fread(&m_M7HFlip, sizeof(m_M7HFlip), 1, f);
    fread(&m_M7VFlip, sizeof(m_M7VFlip), 1, f);
    fread(&m_M7Old, sizeof(m_M7Old), 1, f);
    fread(&m_M7HOFS, sizeof(m_M7HOFS), 1, f);
    fread(&m_M7VOFS, sizeof(m_M7VOFS), 1, f);
    fread(&m_M7A, sizeof(m_M7A), 1, f);
    fread(&m_M7B, sizeof(m_M7B), 1, f);
    fread(&m_M7C, sizeof(m_M7C), 1, f);
    fread(&m_M7D, sizeof(m_M7D), 1, f);
    fread(&m_M7X, sizeof(m_M7X), 1, f);
    fread(&m_M7Y, sizeof(m_M7Y), 1, f);
    fread(&m_MPY, sizeof(m_MPY), 1, f);
}

Ppu::WindowConfig::Config Ppu::getWindowConfig(uint32_t value) {
  if ((value & 0b10) == 0) {
      return WindowConfig::Config::disabled;
  } else if (value & 1) {
      return WindowConfig::Config::inside;
  } else {
      return WindowConfig::Config::outside;
  }
};


bool Ppu::isInsideWindow(int x, const WindowConfig& config, WindowConfig::Config layerConfig, bool* enabled)
{
    bool insideWindow = config.m_Left <= x && x <= config.m_Right;

    if (layerConfig == WindowConfig::Config::outside) {
        *enabled = true;
        insideWindow = !insideWindow;
    } else if (layerConfig == WindowConfig::Config::disabled) {
        *enabled = false;
        insideWindow = true;
    } else {
        *enabled = true;
    }

    return insideWindow;
}

bool Ppu::applyWindowLogic(
    int x,
    WindowConfig::Config window1BgConfig,
    WindowConfig::Config window2BgConfig,
    WindowLogic logic)
{
    bool pixelInWindow;

    bool insideWindow1, window1Enabled;
    insideWindow1 = isInsideWindow(
        x,
        m_Window1Config,
        window1BgConfig,
        &window1Enabled);

    bool insideWindow2, window2Enabled;
    insideWindow2 = isInsideWindow(
        x,
        m_Window2Config,
        window2BgConfig,
        &window2Enabled);

    if (window1Enabled && window2Enabled) {
        switch (logic) {
        case WindowLogic::OR:
            pixelInWindow = !insideWindow1 || !insideWindow2;
            break;

        case WindowLogic::AND:
            pixelInWindow = !insideWindow1 && !insideWindow2;
            break;

        case WindowLogic::XOR:
            pixelInWindow = !insideWindow1 != !insideWindow2;
            break;

        case WindowLogic::XNOR:
            pixelInWindow = !insideWindow1 == !insideWindow2;
            break;

        default:
            LOGE(TAG, "Unknown logic %d", static_cast<int>(logic));
            pixelInWindow = false;
            assert(false);
            break;
        }
    } else if (window1Enabled) {
        pixelInWindow = !insideWindow1;
    } else if (window2Enabled) {
        pixelInWindow = !insideWindow2;
    } else {
        pixelInWindow = false;
    }

    return pixelInWindow;
}

void Ppu::initScreenRenderMode7()
{
    // Prepare sprites
    for (size_t i = 0; i < SIZEOF_ARRAY(m_RenderObjInfo); i++) {
        m_RenderObjInfo[i].m_ObjCount = 0;
    }

    loadObjs();
}

void Ppu::initLineRenderMode7(int y)
{
    m_RenderLayerPriority = s_LayerPriorityMode7;
}

// https://github.com/bsnes-emu/bsnes/blob/master/bsnes/sfc/ppu/mode7.cpp
void Ppu::renderDotMode7(int x, int y)
{
    uint32_t rawColor;
    bool colorValid = false;

    for (size_t prioIdx = 0; !colorValid && m_RenderLayerPriority[prioIdx].m_Layer != Layer::none; prioIdx++) {
        const auto& layer = m_RenderLayerPriority[prioIdx];

        if (layer.m_Layer == Layer::background) {
            RendererBgInfo* renderBg = &m_RenderBgInfo[layer.m_BgIdx];
            rawColor = renderGetColorMode7(x, y);
            colorValid = true;
        } else if (layer.m_Layer == Layer::sprite) {
            colorValid = getSpriteCurrentPixel(x, y, m_MainScreenConfig, layer.m_Priority, &rawColor);
        }
    }

    // Get final color
    if (colorValid) {
        const auto color = rawColorToRgb(rawColor);
        m_RenderCb(color);
    } else {
        m_RenderCb({0, 0, 0});
    }
}

uint32_t Ppu::renderGetColorMode7(int x, int y)
{
    if (m_M7HFlip) {
        x = kPpuDisplayWidth - x;
    }

    if (m_M7VFlip) {
        y = kPpuDisplayHeight - y;
    }

    auto int13ToInt = [](int16_t value) -> int {
        // Extract bit sign
        const int sign = (value >> 12) & 1;
        value &= ~(1 << 12);

        // Add scale factor (0x100)
        int intValue = value << 8;

        if (sign) {
            // Extend sign
            return intValue | 0xFFF00000;
        } else {
            return intValue;
        }
    };

    // Initial type: with 0 scale factor
    // Add 0x100 scale factor
    int scaledX = x << 8;
    int scaledY = y << 8;

    // Initial type: int16 with 0x100 scale factor
    int m7A = m_M7A;
    int m7B = m_M7B;
    int m7C = m_M7C;
    int m7D = m_M7D;

    // Initial type: int13 with 0 scale factor
    // Switch to int16 and add 0x100 scale factor
    int m7HOFS = int13ToInt(m_M7HOFS);
    int m7VOFS = int13ToInt(m_M7VOFS);
    int m7X = int13ToInt(m_M7X);
    int m7Y = int13ToInt(m_M7Y);

    // Apply transformation
    int offsetX = scaledX + m7HOFS - m7X;
    int offsetY = scaledY + m7VOFS - m7Y;

    int vramX = (m7A * offsetX & ~63) + (m7B * offsetY & ~63) + (m7X << 8);
    int vramY = (m7C * offsetX & ~63) + (m7D * offsetY & ~63) + (m7Y << 8);

    // Remove scale factor (multiplication double the scale factor)
    vramX >>= 16;
    vramY >>= 16;

    // Handle wrap
    if (m_M7ScreenOver == 0 || m_M7ScreenOver == 1) {
        if (vramX > 1024) {
            vramX %= 1024;
        } else if (vramX < 0) {
            vramX += 1024;
        }

        if (vramY > 1024) {
            vramY %= 1024;
        } else if (vramY < 0) {
            vramY += 1024;
        }
    } else if (m_M7ScreenOver == 2) {
        if (vramX < 0 || vramX > 1024) {
            return 0;
        }

        if (vramY < 0 || vramY > 1024) {
            return 0;
        }
    } else if (m_M7ScreenOver == 3) {
        if (vramX > 1024) {
            vramX %= 8;
            vramY %= 8;
        } else if (vramX < 0) {
            vramX = (vramX + 1024) % 8;
            vramY = (vramY + 1024) % 8;
        }

        if (vramY > 1024) {
            vramX %= 8;
            vramY %= 8;
        } else if (vramY < 0) {
            vramX = (vramX + 1024) % 8;
            vramY = (vramY + 1024) % 8;
        }
    }

    // Compute tile address
    const int tilemapX = vramX / 8;
    const int tilemapY = vramY / 8;

    const int tileX = vramX % 8;
    const int tileY = vramY % 8;

    const int kPpuMode7TilemapWidth = 128;
    const int mapEntryIdx = kPpuMode7TilemapWidth * tilemapY + tilemapX;
    const int charIdx = m_Vram[mapEntryIdx * 2];

    // Read palette index
    const int tileBaseAddr = charIdx * 0x80 + 1;
    const uint32_t cgramIdx = m_Vram[tileBaseAddr + tileY * 0x10 + tileX * 2];

    return m_Cgram[cgramIdx];
}
