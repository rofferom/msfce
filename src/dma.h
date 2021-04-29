#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory>

#include "memcomponent.h"

class Membus;

class Dma : public MemComponent {
public:
    Dma(const std::shared_ptr<Membus> membus);
    ~Dma() = default;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    int run();

private:
    static constexpr int kChannelCount = 8;

    enum class Direction {
        aToB,
        bToA,
    };

    enum class AddressingMode {
        direct,
        indirect,
    };

    enum class ABusStep {
        increment,
        decrement,
        fixed,
    };

    struct Channel {
        Direction m_Direction;
        AddressingMode m_AddressingMode;
        ABusStep m_ABusStep;
        int m_Mode;

        uint8_t m_BBusAddress;
        uint32_t m_ABusAddress;

        uint16_t m_DMAByteCounter;

        uint16_t& m_HDMATableOffset = m_DMAByteCounter;
        uint16_t m_HDMATableBank;
        uint16_t m_HDMACurrentAddress;
        uint8_t m_HDMALineCounter;
    };

    static constexpr int kUnknownChannelId = -1;

    struct RunningCtx {
        int id = kUnknownChannelId;
        Channel* m_Registers = nullptr;

        uint32_t m_bBaseBusAddress;
        uint32_t m_SrcAddress;
        uint32_t m_DestAddress;
        uint32_t* m_aBusAddress;
        uint32_t* m_bBusAddress;
    };

private:
    void channelStart(int id, Channel* channel, int *cycles);
    void channelContinue(int *cycles);

    void incrementABusAddress(const Channel* channel, uint32_t* aBusAddress);

private:
    Channel m_Channels[kChannelCount];
    std::shared_ptr<Membus> m_Membus;

    uint8_t m_ActiveDmaChannels = 0;

    RunningCtx m_RunningCtx;
};
