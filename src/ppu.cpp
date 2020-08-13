#include <assert.h>
#include "log.h"
#include "registers.h"
#include "ppu.h"

#define TAG "ppu"

uint8_t Ppu::readU8(size_t addr)
{
    switch (addr) {
    case kRegRDNMI:
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
            LOGI(TAG, "ForcedBlanking is now %s", m_ForcedBlanking ? "enabled" : "disabled");
        }

        uint8_t brightness = value & 0b111;
        if (m_Brightness != brightness) {
            m_Brightness = brightness;
            LOGI(TAG, "Brightness is now %d", m_Brightness);
        }
        break;
    }

    case kRegNmitimen: {

        // H/V IRQ
        if (value & (0b11 << 4)) {
            LOGC(TAG, "Trying to enable unsupported H/V IRQ");
            assert(false);
        }

        // NMI
        bool enableNMI = value & (1 << 7);
        if (m_NMIEnabled != enableNMI) {
            m_NMIEnabled = enableNMI;
            LOGI(TAG, "NMI is now %s", m_NMIEnabled ? "enabled" : "disabled");
        }

        // Joypad
        bool joypadAutoread = value & 1;
        if (joypadAutoread) {
            LOGI(TAG, "Joypad autoread is now %s", joypadAutoread ? "enabled" : "disabled");
        }

        break;
    }

    default:
        LOGW(TAG, "Ignore WriteU8 %04X at %06X", value, static_cast<uint32_t>(addr));
        break;
    }
}

void Ppu::writeU16(size_t addr, uint16_t value)
{
    if (addr == 0x2118) {
        return;
    }

    switch (addr) {
    case kRegNmitimen: {
        LOGC(TAG, "Trying to write an U16 in a U8 register (%04X)", static_cast<uint32_t>(addr));
        assert(false);
        break;
    }

    default:
        LOGW(TAG, "Ignore WriteU16 %04X at %06X", value, static_cast<uint32_t>(addr));
        break;
    }
}

bool Ppu::isNMIEnabled() const
{
    return m_NMIEnabled;
}
