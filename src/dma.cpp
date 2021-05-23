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

    channel->m_ABusAddress =
        channelCfg[kRegDmaA1TL] |
       (channelCfg[kRegDmaA1TH] << 8) |
       (channelCfg[kRegDmaA1B] << 16);

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
    auto channel = m_DmaRunningCtx.m_Registers;

    switch (channel->m_Params.m_Mode) {
    case 0:
        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
        channel->m_DMAByteCounter--;

        *cycles += kTimingDmaAccess;
        break;

    case 1:
        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
        (*m_DmaRunningCtx.m_bBusAddress)++;
        *cycles += kTimingDmaAccess;

        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
        *m_DmaRunningCtx.m_bBusAddress = m_DmaRunningCtx.m_bBaseBusAddress;
        *cycles += kTimingDmaAccess;

        if (channel->m_DMAByteCounter >= 2) {
            channel->m_DMAByteCounter -=2;
        } else {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
            (*m_DmaRunningCtx.m_bBusAddress)++;
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter = 0;
        }
        break;

    case 2:
    case 6:
        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
        *cycles += kTimingDmaAccess;

        m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
        incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
        *cycles += kTimingDmaAccess;

        if (channel->m_DMAByteCounter >= 2) {
            channel->m_DMAByteCounter -=2;
        } else {
            m_Membus->writeU8(m_DmaRunningCtx.m_DestAddress, m_Membus->readU8(m_DmaRunningCtx.m_SrcAddress));
            incrementABusAddress(channel, m_DmaRunningCtx.m_aBusAddress);
            *cycles += kTimingDmaAccess;

            channel->m_DMAByteCounter = 0;
        }
        break;

    default:
        LOGC(TAG, "Unimplemented mode %d", channel->m_Params.m_Mode);
        assert(false);
        break;
    }
}

void Dma::incrementABusAddress(const DmaChannel* channel, uint32_t* aBusAddress)
{
    switch (channel->m_ABusStep) {
    case ABusStep::increment:
        (*aBusAddress)++;
        break;

    case ABusStep::decrement:
        (*aBusAddress)--;
        break;

    case ABusStep::fixed:
        break;

    default:
        LOGC(TAG, "Unimplemented ABusStep %d", static_cast<int>(channel->m_ABusStep));
        assert(false);
        break;
    }
}

void Dma::dumpToFile(FILE* f)
{
    fwrite(&m_DmaChannels, sizeof(m_DmaChannels), 1, f);
    fwrite(&m_ChannelRegisters, sizeof(m_ChannelRegisters), 1, f);
    fwrite(&m_ActiveDmaChannels, sizeof(m_ActiveDmaChannels), 1, f);
}

void Dma::loadFromFile(FILE* f)
{
    fread(&m_DmaChannels, sizeof(m_DmaChannels), 1, f);
    fread(&m_ChannelRegisters, sizeof(m_ChannelRegisters), 1, f);
    fread(&m_ActiveDmaChannels, sizeof(m_ActiveDmaChannels), 1, f);
}
