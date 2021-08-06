#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "controller.h"
#include "scheduler.h"
#include "snes_renderer.h"

class Apu;
class BufferMemComponent;
class ControllerPorts;
class Cpu65816;
class Dma;
class IndirectWram;
class Maths;
class Ppu;
class Wram;

enum class AddressingType;

class Snes : public MemComponent, public Scheduler, public std::enable_shared_from_this<Snes> {
public:
    Snes();

    int addRenderer(const std::shared_ptr<SnesRenderer>& renderer);
    int removeRenderer(const std::shared_ptr<SnesRenderer>& renderer);

    int plugCartidge(const char* path);
    std::string getRomBasename() const;

    int start();
    int stop();

    int renderSingleFrame(bool renderPpu = true);

    void resumeTask(SchedulerTask* task, int cycles);

    void setController1(const SnesController& controller);

    void saveState(const std::string& path);
    void loadState(const std::string& path);

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

private:
    int scoreHeader(uint32_t address);

    void loadSram();
    void saveSram();

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

        template <typename Duration>
        int64_t total();
    };

private:
    // Renderer variables
    std::vector<std::shared_ptr<SnesRenderer>> m_RendererList;

    // Rom
    std::string m_RomBasename;
    std::vector<uint8_t> m_RomData;
    AddressingType m_AddressingType;

    // Components
    std::shared_ptr<Wram> m_Ram;
    std::shared_ptr<IndirectWram> m_IndirectWram;
    std::shared_ptr<BufferMemComponent> m_Sram;
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
