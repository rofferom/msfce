#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "scheduler.h"
#include "msfce/core/snes.h"

namespace msfce::core {

class Apu;
class BufferMemComponent;
class ControllerPorts;
class Cpu65816;
class Dma;
class IndirectWram;
class Maths;
class Ppu;
class Sram;
class Wram;

enum class AddressingType;

class SnesImpl
    : public Snes
    , public MemComponent
    , public Scheduler
    , public std::enable_shared_from_this<SnesImpl> {
public:
    SnesImpl();

    // Snes methods
    int addRenderer(const std::shared_ptr<Renderer>& renderer) final;
    int removeRenderer(const std::shared_ptr<Renderer>& renderer) final;

    int plugCartidge(const char* path) final;
    std::string getRomBasename() const final;

    int start() final;
    int stop() final;

    SnesConfig getConfig() final;

    int renderSingleFrame(bool renderPpu = true) final;

    void setController1(const Controller& controller) final;

    void saveState(const std::string& path) final;
    void loadState(const std::string& path) final;

    // Scheduler methods
    void resumeTask(SchedulerTask* task, int cycles) final;

    // MemComponent methods
    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

private:
    int scoreHeader(uint32_t address);

    void setHVIRQ_Flag(bool v);

private:
    using Clock = std::chrono::high_resolution_clock;

    struct DurationTool {
        // Current measure
        Clock::time_point begin_tp;
        Clock::time_point end_tp;

        // Total measure
        Clock::duration total_duration;

        void reset();

        void begin();

        void end();

        template<typename Duration>
        int64_t total();
    };

private:
    // Renderer variables
    std::vector<std::shared_ptr<Renderer>> m_RendererList;

    // Rom
    std::string m_RomBasename;
    std::vector<uint8_t> m_RomData;
    AddressingType m_AddressingType;
    bool m_FastRom = false;
    int m_SramSize = 0;

    // Components
    std::shared_ptr<Wram> m_Ram;
    std::shared_ptr<IndirectWram> m_IndirectWram;
    std::shared_ptr<Sram> m_Sram;
    std::shared_ptr<Apu> m_Apu;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;
    std::shared_ptr<Cpu65816> m_Cpu;
    std::shared_ptr<Dma> m_Dma;
    std::shared_ptr<Maths> m_Maths;
    std::shared_ptr<Ppu> m_Ppu;

    // MemComponent variables
    // HVBJOY
    uint8_t m_HVBJOY = 0;

    // VBlank interrupt
    bool m_NMIEnabled = false;

    // H/V IRQ
    uint8_t m_HVIRQ_Config = 0;
    bool m_HVIRQ_Flag = false;
    uint16_t m_HVIRQ_H = 0;
    uint16_t m_HVIRQ_V = 0;

    // Joypad interrupt
    bool m_JoypadAutoread = false;
    bool m_JoypadAutoreadRunning = false;
    uint64_t m_JoypadAutoreadEndcycle = 0;

    // IRQ
    bool m_Vblank = false;

    // Scheduling
    uint64_t m_MasterClock = 0;

    DurationTool m_CpuTime;
    DurationTool m_PpuTime;
};

} // namespace msfce::core
