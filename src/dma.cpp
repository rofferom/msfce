#include <assert.h>
#include "log.h"
#include "membus.h"
#include "registers.h"
#include "scheduler.h"
#include "timings.h"
#include "dma.h"

#define TAG "dma"

Dma::Dma(const std::shared_ptr<Membus>& membus)
    : MemComponent(MemComponentType::dma),
      SchedulerTask(),
      m_Membus(membus)
{
}

void Dma::setScheduler(const std::shared_ptr<Scheduler>& scheduler)
{
    m_Scheduler = scheduler;
}

uint8_t Dma::readU8(uint32_t addr)
{
    if (addr == kRegisterMDMAEN || addr == kRegisterHDMAEN) {
        return 0;
    } else {
        addr -= kRegDmaStart;
        return m_ChannelRegisters[addr];
    }
}

void Dma::writeU8(uint32_t addr, uint8_t value)
{
    if (addr == kRegisterMDMAEN) {
        LOGD(TAG, "Start DMA: %02X", value);
        m_ActiveDmaChannels = value;
        m_Scheduler->resumeTask(this, 1);
        return;
    } else if (addr == kRegisterHDMAEN) {
        // Start HDMA
        LOGD(TAG, "Start HDMA: %02X", value);
        m_ActiveHdmaChannels = value;
        return;
    } else {
        addr -= kRegDmaStart;
        m_ChannelRegisters[addr] = value;

        int channelId = addr >> 4;
        int configId = addr & 0xF;
        auto dmaChannel = &m_DmaChannels[channelId];

        if (configId == kRegDmaA1TL) {
            dmaChannel->m_ABusAddress &= 0xFFFF00;
            dmaChannel->m_ABusAddress |= value;
        } else if (configId == kRegDmaA1TH) {
            dmaChannel->m_ABusAddress &= 0xFF00FF;
            dmaChannel->m_ABusAddress |= value << 8;
        } else if (configId == kRegDmaA1B) {
            dmaChannel->m_ABusAddress &= 0xFFFF;
            dmaChannel->m_ABusAddress |= value << 16;
        }

        return;
    }

    LOGW(TAG, "Unsupported writeU8 %04X", static_cast<uint32_t>(addr));
}

int Dma::run()
{
    if (!m_ActiveDmaChannels) {
        return 0;
    }

    if (m_DmaRunningCtx.id != kUnknownChannelId) {
        if (m_DmaRunningCtx.m_Registers->m_DMAByteCounter) {
            int cycles = 0;

            dmaChannelContinue(&cycles);

            return cycles;
        } else {
            m_ActiveDmaChannels &= ~(1 << m_DmaRunningCtx.id);

            m_DmaRunningCtx.id = kUnknownChannelId;
            m_DmaRunningCtx.m_Registers = nullptr;
        }
    }

    // Look for the next channel to run
    for (int i = 0; i < kChannelCount; i++) {
        if (m_ActiveDmaChannels & (1 << i)) {
            int cycles = 0;

            LOGD(TAG, "Start DMA channel %d", i);

            m_DmaRunningCtx.id = i;
            m_DmaRunningCtx.m_Registers = &m_DmaChannels[i];

            dmaChannelStart(i, &m_DmaChannels[i], &cycles);

            return cycles;
        }
    }

    return 0;
}

void Dma::dmaChannelStart(int id, DmaChannel* channel, int *cycles)
{
    // Configure channel: read channel configuration
    const uint8_t* channelCfg = &m_ChannelRegisters[id * kChannelCfgLen];

    // kRegDmaP
    if (channelCfg[kRegDmaP] & (1 << 7)) {
        channel->m_Params.m_Direction = Direction::bToA;
    } else {
        channel->m_Params.m_Direction = Direction::aToB;
    }
    if (channelCfg[kRegDmaP] & (1 << 6)) {
        channel->m_Params.m_AddressingMode = AddressingMode::indirect;
    } else {
        channel->m_Params.m_AddressingMode = AddressingMode::direct;
    }

    switch ((channelCfg[kRegDmaP] >> 3) & 0b11) {
    case 0:
        channel->m_ABusStep = ABusStep::increment;
        break;

    case 2:
        channel->m_ABusStep = ABusStep::decrement;
        break;

    case 1:
    case 3:
        channel->m_ABusStep = ABusStep::fixed;
        break;

    default:
        break;
    }

    channel->m_Params.m_Mode = channelCfg[kRegDmaP] & 0b111;

    // Addresses
    channel->m_BBusAddress = channelCfg[kRegDmaBBAD];

    channel->m_DMAByteCounter =
        channelCfg[kRegDmaDASL] |
       (channelCfg[kRegDmaDASH] << 8);


    // Setup m_RunningCtx
    m_DmaRunningCtx.m_bBaseBusAddress = 0x2100 | channel->m_BBusAddress;

    LOGD(TAG, "\tDirection: %d", static_cast<int>(channel->m_Params.m_Direction));
    LOGD(TAG, "\tABusStep: %d", static_cast<int>(channel->m_ABusStep));
    LOGD(TAG, "\tMode: %d", static_cast<int>(channel->m_Params.m_Mode));
    LOGD(TAG, "\tB-Bus address: 0x%04X", m_DmaRunningCtx.m_bBaseBusAddress);
    LOGD(TAG, "\tA-Bus address: 0x%06X", channel->m_ABusAddress);
    LOGD(TAG, "\tBytes: 0x%04X", channel->m_DMAByteCounter);

    if (!channel->m_DMAByteCounter) {
        return;
    }

    switch (channel->m_Params.m_Direction) {
    case Direction::aToB:
        m_DmaRunningCtx.m_SrcAddress = channel->m_ABusAddress;
        m_DmaRunningCtx.m_aBusAddress = &m_DmaRunningCtx.m_SrcAddress;
        m_DmaRunningCtx.m_DestAddress = m_DmaRunningCtx.m_bBaseBusAddress;
        m_DmaRunningCtx.m_bBusAddress = &m_DmaRunningCtx.m_DestAddress;
        break;

    case Direction::bToA:
        m_DmaRunningCtx.m_SrcAddress = m_DmaRunningCtx.m_bBaseBusAddress;
        m_DmaRunningCtx.m_bBusAddress = &m_DmaRunningCtx.m_SrcAddress;
        m_DmaRunningCtx.m_DestAddress = channel->m_ABusAddress;
        m_DmaRunningCtx.m_aBusAddress = &m_DmaRunningCtx.m_DestAddress;
        break;

    default:
        m_DmaRunningCtx.m_aBusAddress = nullptr;
        m_DmaRunningCtx.m_bBusAddress = nullptr;
        assert(false);
        break;
    }

    m_DmaRunningCtx.id = id;
    m_DmaRunningCtx.m_Registers = channel;

    *cycles += kTimingDmaStart;
}

void Dma::dmaChannelContinue(int *cycles)
{
    const int channelId = m_DmaRunningCtx.id;
    auto channel = m_DmaRunningCtx.m_Registers;

    switch (channel->m_Params.m_Mode) {
    case 0:
        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
        channel->m_DMAByteCounter--;

        *cycles += kTimingDmaAccess;
        break;

    case 1:
        if (channel->m_DMAByteCounter >= 2) {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            (*m_DmaRunningCtx.m_bBusAddress)++;
            *cycles += kTimingDmaAccess;

            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            *m_DmaRunningCtx.m_bBusAddress = m_DmaRunningCtx.m_bBaseBusAddress;
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter -=2;
        } else {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter = 0;
        }
        break;

    case 2:
    case 6:
        if (channel->m_DMAByteCounter >= 2) {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            *cycles += kTimingDmaAccess;

            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter -=2;
        } else {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channelId, channel, m_DmaRunningCtx.m_aBusAddress);
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter = 0;
        }
        break;

    default:
        LOGC(TAG, "Unimplemented mode %d", channel->m_Params.m_Mode);
        assert(false);
        break;
    }

    uint8_t* channelCfg = &m_ChannelRegisters[channelId * kChannelCfgLen];
    channelCfg[kRegDmaDASL] = channel->m_DMAByteCounter & 0xFF;
    channelCfg[kRegDmaDASH] = (channel->m_DMAByteCounter & 0xFF00) >> 8;
}

void Dma::incrementABusAddress(int id, DmaChannel* channel, uint32_t* aBusAddress)
{
    switch (channel->m_ABusStep) {
    case ABusStep::increment:
        (*aBusAddress) = (*aBusAddress & 0xFF0000) | ((*aBusAddress + 1) & 0xFFFF);
        channel->m_ABusAddress = (channel->m_ABusAddress & 0xFF0000) | ((channel->m_ABusAddress + 1) & 0xFFFF);
        break;

    case ABusStep::decrement:
        (*aBusAddress) = (*aBusAddress & 0xFF0000) | ((*aBusAddress - 1) & 0xFFFF);
        channel->m_ABusAddress = (channel->m_ABusAddress & 0xFF0000) | ((channel->m_ABusAddress - 1) & 0xFFFF);
        break;

    case ABusStep::fixed:
        break;

    default:
        LOGC(TAG, "Unimplemented ABusStep %d", static_cast<int>(channel->m_ABusStep));
        assert(false);
        break;
    }

    uint8_t* channelCfg = &m_ChannelRegisters[id * kChannelCfgLen];
    channelCfg[kRegDmaA1TL] = channel->m_ABusAddress & 0xFF;
    channelCfg[kRegDmaA1TH] = (channel->m_ABusAddress & 0xFF00) >> 8;
}

void Dma::dumpToFile(FILE* f)
{
    SchedulerTask::dumpToFile(f);

    fwrite(&m_Vblank, sizeof(m_Vblank), 1, f);
    fwrite(&m_DmaChannels, sizeof(m_DmaChannels), 1, f);
    fwrite(&m_HdmaChannels, sizeof(m_HdmaChannels), 1, f);
    fwrite(&m_ChannelRegisters, sizeof(m_ChannelRegisters), 1, f);
    fwrite(&m_ActiveDmaChannels, sizeof(m_ActiveDmaChannels), 1, f);
    fwrite(&m_ActiveHdmaChannels, sizeof(m_ActiveHdmaChannels), 1, f);
}

void Dma::loadFromFile(FILE* f)
{
    SchedulerTask::loadFromFile(f);

    fread(&m_Vblank, sizeof(m_Vblank), 1, f);
    fread(&m_DmaChannels, sizeof(m_DmaChannels), 1, f);
    fread(&m_HdmaChannels, sizeof(m_HdmaChannels), 1, f);
    fread(&m_ChannelRegisters, sizeof(m_ChannelRegisters), 1, f);
    fread(&m_ActiveDmaChannels, sizeof(m_ActiveDmaChannels), 1, f);
    fread(&m_ActiveHdmaChannels, sizeof(m_ActiveHdmaChannels), 1, f);
}

void Dma::onScanStarted()
{
    m_Vblank = false;

    // Look for the next channel to run
    for (int i = 0; i < kChannelCount; i++) {
        if (m_ActiveHdmaChannels & (1 << i)) {
            auto channel = &m_HdmaChannels[i];
            const uint8_t* channelCfg = &m_ChannelRegisters[i * kChannelCfgLen];

            // kRegDmaP
            if (channelCfg[kRegDmaP] & (1 << 7)) {
                channel->m_Params.m_Direction = Direction::bToA;
            } else {
                channel->m_Params.m_Direction = Direction::aToB;
            }

            if (channelCfg[kRegDmaP] & (1 << 6)) {
                channel->m_Params.m_AddressingMode = AddressingMode::indirect;
            } else {
                channel->m_Params.m_AddressingMode = AddressingMode::direct;
            }

            channel->m_IndirectTable = channelCfg[kRegDmaP] & (1 << 6);
            channel->m_Params.m_Mode = channelCfg[kRegDmaP] & 0b111;

            channel->m_TableAddressStart =
                channelCfg[kRegDmaA1TL] |
               (channelCfg[kRegDmaA1TH] << 8) |
               (channelCfg[kRegDmaA1B] << 16);

            channel->m_bBaseBusAddress = 0x2100 | channelCfg[kRegDmaBBAD];

            channel->m_Running = true;

            // Preload first table entry
            channel->m_TableAddress = channel->m_TableAddressStart;
            uint8_t b = m_Membus->readU8(channel->m_TableAddress);
            channel->m_TableAddress++;

            if (channel->m_IndirectTable) {
                channel->m_NextDataAddress =
                        m_Membus->readU16(channel->m_TableAddress) |
                        (channelCfg[kRegDmaDASB] << 16);
            } else {
                channel->m_NextDataAddress = channel->m_TableAddress;
            }

            channel->m_Repeat = b & (1 << 7);
            channel->m_Lines = b & 0b1111111;
            channel->m_RemainingLines = channel->m_Lines;

            channel->m_FirstLine = true;
        }
    }
}

void Dma::onHblank()
{
    if (m_Vblank) {
        return;
    }

    // Look for the next channel to run
    for (int i = 0; i < kChannelCount; i++) {
        if (m_ActiveHdmaChannels & (1 << i)) {
            auto channel = &m_HdmaChannels[i];
            const uint8_t* channelCfg = &m_ChannelRegisters[i * kChannelCfgLen];
            bool doTransfer = false;
            uint32_t tableAddress;

            if (!channel->m_Running) {
                continue;
            }

            if (channel->m_FirstLine) {
                doTransfer = true;
                channel->m_FirstLine = false;
            } else {
                doTransfer = channel->m_Repeat;
            }

            if (doTransfer) {
                switch (channel->m_Params.m_Mode) {
                case 0:
                    m_Membus->writeU8(channel->m_bBaseBusAddress,
                        m_Membus->readU8(channel->m_NextDataAddress));

                    channel->m_NextDataAddress++;

                    break;

                case 1:
                    m_Membus->writeU16(channel->m_bBaseBusAddress,
                        m_Membus->readU16(channel->m_NextDataAddress));

                    channel->m_NextDataAddress += 2;

                    break;

                case 2:
                case 6:
                    m_Membus->writeU8(channel->m_bBaseBusAddress,
                        m_Membus->readU8(channel->m_NextDataAddress));

                    m_Membus->writeU8(channel->m_bBaseBusAddress,
                        m_Membus->readU8(channel->m_NextDataAddress + 1));

                    channel->m_NextDataAddress += 2;

                    break;

                case 3:
                case 7:
                    m_Membus->writeU8(channel->m_bBaseBusAddress,
                        m_Membus->readU8(channel->m_NextDataAddress));

                    m_Membus->writeU8(channel->m_bBaseBusAddress,
                        m_Membus->readU8(channel->m_NextDataAddress + 1));

                    m_Membus->writeU8(channel->m_bBaseBusAddress + 1,
                        m_Membus->readU8(channel->m_NextDataAddress + 2));

                    m_Membus->writeU8(channel->m_bBaseBusAddress + 1,
                        m_Membus->readU8(channel->m_NextDataAddress + 3));

                    channel->m_NextDataAddress += 4;

                    break;

                case 4:
                    for (int i = 0; i < 4; i++) {
                        m_Membus->writeU8(channel->m_bBaseBusAddress + i,
                            m_Membus->readU8(channel->m_NextDataAddress));

                        channel->m_NextDataAddress++;
                    }
                    break;

                default:
                    LOGE(TAG, "Unimplemented HDMA mode %d", channel->m_Params.m_Mode);
                    break;
                }
            }

            channel->m_RemainingLines--;

            if (channel->m_RemainingLines == 0) {
                // Skip data of current line
                if (channel->m_IndirectTable) {
                    // Indirect HDMA has a fixed data size
                    channel->m_TableAddress += 2;
                } else {
                    uint32_t dataSize = 0;

                    switch (channel->m_Params.m_Mode) {
                    case 0:
                        dataSize = 1;
                        break;

                    case 1:
                    case 2:
                        dataSize = 2;
                        break;

                    case 3:
                    case 4:
                        dataSize = 4;
                        break;

                    default:
                        LOGI(TAG, "Unknown data size for mode %d", channel->m_Params.m_Mode);
                        assert(false);
                        break;
                    }

                    if (channel->m_Repeat) {
                        channel->m_TableAddress += dataSize * channel->m_Lines;
                    } else {
                        channel->m_TableAddress += dataSize;
                    }
                }

                // Load data for next line
                uint8_t b = m_Membus->readU8(channel->m_TableAddress);
                channel->m_TableAddress++;

                if (channel->m_IndirectTable) {
                    channel->m_NextDataAddress =
                            m_Membus->readU16(channel->m_TableAddress) |
                            (channelCfg[kRegDmaDASB] << 16);
                } else {
                    channel->m_NextDataAddress = channel->m_TableAddress;
                }

                channel->m_Repeat = b & (1 << 7);
                channel->m_Lines = b & 0b1111111;
                channel->m_RemainingLines = channel->m_Lines;

                if (channel->m_RemainingLines == 0) {
                    channel->m_Running = false;
                } else {
                    channel->m_FirstLine = true;
                }
            }
        }
    }
}

void Dma::onVblank()
{
    m_Vblank = true;
}
