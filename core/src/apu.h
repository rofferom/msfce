#pragma once

#include <stdint.h>

#include <functional>
#include <memory>
#include <mutex>

#include <snes_spc/SNES_SPC.h>

#include "memcomponent.h"
#include "schedulertask.h"

struct SNES_SPC;

namespace msfce::core {

class Apu : public MemComponent, public SchedulerTask {
public:
    using RenderSampleCb = std::function<void(const uint8_t* data, size_t sampleCount)>;

    static constexpr int kSampleSize = 4; // S16 stereo
    static constexpr int kSampleRate = 32000;
    static constexpr int kChannels = 2;

public:
    Apu(const uint64_t& masterClock, RenderSampleCb renderSampleCb);
    ~Apu();

    // MemComponent methods
    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    // SchedulerTask methods
    int run() override;

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    RenderSampleCb m_RenderSampleCb;

    const uint64_t& m_MasterClock;
    uint64_t m_Clock = 0;

    SNES_SPC m_SPC;
    uint8_t* m_Samples = nullptr;
    size_t m_SamplesSize = 0;
};

} // namespace msfce::core
