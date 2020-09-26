#include <assert.h>
#include "log.h"
#include "membus.h"
#include "registers.h"
#include "dma.h"

#define TAG "dma"

Dma::Dma(const std::shared_ptr<Membus> membus)
    : MemComponent(MemComponentType::dma),
      m_Membus(membus)
{
}

uint8_t Dma::readU8(uint32_t addr)
{
    switch (addr) {
    default:
        LOGW(TAG, "Unsupported readU8 %04X", addr);
        assert(false);
        break;
    }

    return 0;
}

void Dma::writeU8(uint32_t addr, uint8_t value)
{
    if (addr == kRegisterMDMAEN) {
        LOGD(TAG, "Start DMA: %02X", value);
        m_ActiveDmaChannels = value;
        return;
    } else if (addr == kRegisterHDMAEN) {
        // Start HDMA
        LOGD(TAG, "Start HDMA: %02X", value);
        return;
    }

    assert(kRegDmaStart <= addr && addr <= kRegDmaEnd);

    // Address format: 0x43XY => X: Channel, Y: ChannelRegister
    int channelIdx = (addr >> 4) & 0xF;
    Channel& channel = m_Channels[channelIdx];
    int reg = addr & 0xF;

    LOGD(TAG, "Configuring Channel %d, Register 0x%X (%02X)", channelIdx, reg, value);

    switch (reg) {
    case kRegDmaP:
        if (value & (1 << 7)) {
            channel.m_Direction = Direction::bToA;
        } else {
            channel.m_Direction = Direction::aToB;
        }
        if (value & (1 << 6)) {
            channel.m_AddressingMode = AddressingMode::indirect;
        } else {
            channel.m_AddressingMode = AddressingMode::direct;
        }

        switch ((value >> 3) & 0b11) {
        case 0:
            channel.m_ABusStep = ABusStep::increment;
            break;

        case 2:
            channel.m_ABusStep = ABusStep::decrement;
            break;

        case 1:
        case 3:
            channel.m_ABusStep = ABusStep::fixed;
            break;

        default:
            break;
        }

        channel.m_Mode = value & 0b111;

        break;

    case kRegDmaBBAD:
        channel.m_BBusAddress = value;
        break;

    case kRegDmaA1TL:
        channel.m_ABusAddress = (channel.m_ABusAddress & 0xFFFF00) | value;
        break;

    case kRegDmaA1TH:
        channel.m_ABusAddress = (channel.m_ABusAddress & 0xFF00FF) | (value << 8);
        break;

    case kRegDmaA1B:
        channel.m_ABusAddress = (channel.m_ABusAddress & 0xFFFF) | (value << 16);
        break;

    case kRegDmaDASL:
        channel.m_DMAByteCounter = (channel.m_DMAByteCounter & 0xFFFF00) | value;
        break;

    case kRegDmaDASH:
        channel.m_DMAByteCounter = (channel.m_DMAByteCounter & 0xFF00FF) | (value << 8);
        break;

    case kRegDmaDASB:
        break;

    case kRegDmaA2AL:
        break;

    case kRegDmaA2AH:
        break;

    case kRegDmaNTRL:
        break;

    default:
        LOGW(TAG, "Unsupported writeU8 %04X", static_cast<uint32_t>(addr));
        break;
    }
}

void Dma::run()
{
    if (!m_ActiveDmaChannels) {
        return;
    }

    for (int i = 0; i < kChannelCount; i++) {
        if (m_ActiveDmaChannels & (1 << i)) {
            LOGD(TAG, "Start DMA channel %d", i);
            runSingleDmaChannel(&m_Channels[i]);
        }
    }

    m_ActiveDmaChannels = 0;
}

void Dma::runSingleDmaChannel(Channel* channel)
{
    const uint32_t bBaseBusAddress = 0x2100 | channel->m_BBusAddress;
    uint32_t srcAddress;
    uint32_t destAddress;
    uint32_t* aBusAddress;
    uint32_t* bBusAddress;

    LOGD(TAG, "\tDirection: %d", static_cast<int>(channel->m_Direction));
    LOGD(TAG, "\tABusStep: %d", static_cast<int>(channel->m_ABusStep));
    LOGD(TAG, "\tMode: %d", static_cast<int>(channel->m_Mode));
    LOGD(TAG, "\tB-Bus address: 0x%04X", bBaseBusAddress);
    LOGD(TAG, "\tA-Bus address: 0x%06X", channel->m_ABusAddress);
    LOGD(TAG, "\tBytes: 0x%04X", channel->m_DMAByteCounter);

    assert(channel->m_DMAByteCounter);

    switch (channel->m_Direction) {
    case Direction::aToB:
        srcAddress = channel->m_ABusAddress;
        aBusAddress = &srcAddress;
        destAddress = bBaseBusAddress;
        bBusAddress = &destAddress;
        break;

    case Direction::bToA:
        srcAddress = bBaseBusAddress;
        bBusAddress = &srcAddress;
        destAddress = channel->m_ABusAddress;
        aBusAddress = &destAddress;
        break;

    default:
        aBusAddress = nullptr;
        bBusAddress = nullptr;
        assert(false);
        break;
    }

    while (channel->m_DMAByteCounter) {
        switch (channel->m_Mode) {
        case 0:
            m_Membus->writeU8(destAddress, m_Membus->readU8(srcAddress));
            incrementABusAddress(channel, aBusAddress);
            channel->m_DMAByteCounter--;
            break;

        case 1:
            m_Membus->writeU8(destAddress, m_Membus->readU8(srcAddress));
            incrementABusAddress(channel, aBusAddress);
            (*bBusAddress)++;

            m_Membus->writeU8(destAddress, m_Membus->readU8(srcAddress));
            incrementABusAddress(channel, aBusAddress);
            *bBusAddress = bBaseBusAddress;

            if (channel->m_DMAByteCounter >= 2) {
                channel->m_DMAByteCounter -=2;
            } else {
                m_Membus->writeU8(destAddress, m_Membus->readU8(srcAddress));
                incrementABusAddress(channel, aBusAddress);
                (*bBusAddress)++;

                channel->m_DMAByteCounter = 0;
            }
            break;

        default:
            LOGC(TAG, "Unimplemented mode %d", channel->m_Mode);
            assert(false);
            break;
        }
    }
}

void Dma::incrementABusAddress(const Channel* channel, uint32_t* aBusAddress)
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
