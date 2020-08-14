#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory>

class Membus;

class Dma {
public:
    Dma(const std::shared_ptr<Membus> membus);
    ~Dma() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

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

private:
    void runDma(uint8_t value);
    void runSingleDmaChannel(Channel* channel);
    void incrementABusAddress(const Channel* channel, uint32_t* aBusAddress);

private:
    Channel m_Channels[kChannelCount];
    std::shared_ptr<Membus> m_Membus;
};
