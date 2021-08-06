#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <memory>

#include "memcomponent.h"
#include "schedulertask.h"

class Membus;
class Scheduler;

class Dma : public MemComponent, public SchedulerTask {
public:
    Dma(const std::shared_ptr<Membus>& membus);
    ~Dma() = default;

    void setScheduler(const std::shared_ptr<Scheduler>& scheduler);

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

    int run() override;

    void onScanStarted();
    void onHblank();
    void onVblank();

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

    struct DmaChannelParams {
        Direction m_Direction;
        AddressingMode m_AddressingMode;
        int m_Mode;
    };

    struct DmaChannel {
        DmaChannelParams m_Params;
        ABusStep m_ABusStep;

        uint8_t m_BBusAddress;
        uint32_t m_ABusAddress;

        uint16_t m_DMAByteCounter;
    };

    static constexpr int kUnknownChannelId = -1;

    struct DmaRunningCtx {
        int id = kUnknownChannelId;
        DmaChannel* m_Registers = nullptr;

        uint32_t m_bBaseBusAddress;
        uint32_t m_SrcAddress;
        uint32_t m_DestAddress;
        uint32_t* m_aBusAddress;
        uint32_t* m_bBusAddress;
    };

    struct HdmaChannel {
        bool m_Running;

        DmaChannelParams m_Params;
        bool m_IndirectTable;
        uint32_t m_bBaseBusAddress;
        uint32_t m_TableAddressStart;

        uint32_t m_TableAddress;
        uint32_t m_NextDataAddress;
        bool m_FirstLine;
        bool m_Repeat;
        uint8_t m_Lines;
        uint8_t m_RemainingLines;
    };

private:
    void dmaChannelStart(int id, DmaChannel* channel, int *cycles);
    void dmaChannelContinue(int *cycles);

    void incrementABusAddress(int id, DmaChannel* channel, uint32_t* aBusAddress);

private:
    std::shared_ptr<Scheduler> m_Scheduler;
    bool m_Vblank = false;

    static constexpr int kChannelCfgLen = 0x10;
    uint8_t m_ChannelRegisters[kChannelCount * kChannelCfgLen];
    DmaChannel m_DmaChannels[kChannelCount];
    HdmaChannel m_HdmaChannels[kChannelCount];
    std::shared_ptr<Membus> m_Membus;

    uint8_t m_ActiveHdmaChannels = 0;
    uint8_t m_ActiveDmaChannels = 0;

    DmaRunningCtx m_DmaRunningCtx;
};
