#include <stdio.h>
#include <assert.h>
#include "log.h"
#include "registers.h"
#include "utils.h"
#include "ppu.h"

#define TAG "ppu"

namespace {

constexpr uint16_t convert1kWorkStep(uint16_t v) {
    return v << 11;
}

constexpr uint16_t convert4kWorkStep(uint16_t v) {
    return v << 13;
}

} // anonymous namespace

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
}

uint8_t Ppu::readU8(size_t addr)
{
    switch (addr) {
    case kRegRDNMI:
    case kRegTIMEUP:
        // Ack interrupt
        return 0;
    }

    return 0;
}

uint16_t Ppu::readU16(size_t addr)
{
    return 0;
}

void Ppu::writeU8(size_t addr, uint8_t value)
{
    switch (addr) {
    case kRegINIDISP: {
        bool forcedBlanking = value & (1 << 7);
        if (m_ForcedBlanking != forcedBlanking) {
            m_ForcedBlanking = forcedBlanking;
            LOGD(TAG, "ForcedBlanking is now %s", m_ForcedBlanking ? "enabled" : "disabled");
        }

        uint8_t brightness = value & 0b111;
        if (m_Brightness != brightness) {
            m_Brightness = brightness;
            LOGD(TAG, "Brightness is now %d", m_Brightness);
        }
        break;
    }

    case kRegNmitimen: {
        // H/V IRQ
        if (value & (0b11 << 4)) {
            // To be implemented
            break;
        }

        // NMI
        bool enableNMI = value & (1 << 7);
        if (m_NMIEnabled != enableNMI) {
            m_NMIEnabled = enableNMI;
            LOGD(TAG, "NMI is now %s", m_NMIEnabled ? "enabled" : "disabled");
        }

        // Joypad
        bool joypadAutoread = value & 1;
        if (joypadAutoread) {
            LOGD(TAG, "Joypad autoread is now %s", joypadAutoread ? "enabled" : "disabled");
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

        bg.m_TilemapBase = value >> 2;
        bg.m_TilemapSize = convert1kWorkStep(value & 0b11);
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
        m_Backgrounds[0].m_HorizontalOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[0].m_HorizontalOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG1VOFS:
        m_Backgrounds[0].m_VerticalOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegBG2HOFS:
        m_Backgrounds[1].m_HorizontalOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[1].m_HorizontalOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG2VOFS:
        m_Backgrounds[1].m_VerticalOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegBG3HOFS:
        m_Backgrounds[2].m_HorizontalOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[2].m_HorizontalOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG3VOFS:
        m_Backgrounds[2].m_VerticalOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegBG4HOFS:
        m_Backgrounds[3].m_HorizontalOffset = (value << 8) | (m_OldBgByte & ~7) | ((m_Backgrounds[3].m_HorizontalOffset >> 8) & 7);
        m_OldBgByte = value;
        break;

    case kRegBG4VOFS:
        m_Backgrounds[3].m_VerticalOffset = (value << 8) | m_OldBgByte;
        m_OldBgByte = value;
        break;

    case kRegVTIMEL:
    case kRegVTIMEH:
        // To be implemented
        break;

    // To be implemented
    case kRegOBJSEL:
    case kRegOAMADDL:
    case kRegOAMADDH:
    case kRegOAMDATA:
    case kRegSETINI:
    case kRegMOSAIC:
    case kRegWBGLOG:
    case kRegWOBJLOG:
    case kRegTMW:
    case kRegTSW:
    case kRegM7SEL:
    case kRegCGWSEL:
    case kRegCGADSUB:
    case kRegTM:
    case kRegTS:
    case kRegW12SEL:
    case kRegW34SEL:
    case kRegWOBJSEL:
    case kRegCOLDATA:
        break;

    case kRegRDCGRAM:
    default:
        LOGW(TAG, "Ignore WriteU8 %02X at %06X", value, static_cast<uint32_t>(addr));
        assert(false);
        break;
    }
}

void Ppu::writeU16(size_t addr, uint16_t value)
{
    switch (addr) {
    case kRegNmitimen: {
        LOGC(TAG, "Trying to write an U16 in a U8 register (%04X)", static_cast<uint32_t>(addr));
        assert(false);
        break;
    }

    case kRegVTIMEL:
        // H/V IRQ can't be enabled in the current implementation (assert on activation)
        break;

    case kRegVMADDL:
    case kRegVMDATAL:
        writeU8(addr, value & 0xFF);
        writeU8(addr + 1 , value >> 8);
        break;

    // To be implemented
    case kRegOAMADDL:
        break;

    default:
        LOGW(TAG, "Ignore WriteU16 %04X at %06X", value, static_cast<uint32_t>(addr));
        assert(false);
        break;
    }
}

bool Ppu::isNMIEnabled() const
{
    return m_NMIEnabled;
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

void Ppu::render(const DrawPointCb& drawPointCb)
{
}
