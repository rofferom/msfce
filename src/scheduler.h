#pragma once

#include <memory>
#include <thread>

#include "memcomponent.h"

class ControllerPorts;
class Cpu65816;
class Dma;
class Frontend;
class Ppu;
class SchedulerTask;

class Scheduler : public MemComponent,
                  public std::enable_shared_from_this<Scheduler> {
public:
    using Clock = std::chrono::high_resolution_clock;

public:
    Scheduler(
        const std::shared_ptr<Frontend>& frontend,
        const std::shared_ptr<Cpu65816>& cpu,
        const std::shared_ptr<Dma>& dma,
        const std::shared_ptr<Ppu>& ppu,
        const std::shared_ptr<ControllerPorts>& controllerPorts);

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    int run();

    void toggleRunning();
    void pause();
    void resume();

    void resumeTask(SchedulerTask* task, int cycles);

    void speedUp();
    void speedNormal();

private:
    bool runRunning();
    bool runPaused();

    void setHVIRQ_Flag(bool v);

private:
    struct DurationTool {
        // Current measure
        Scheduler::Clock::time_point begin_tp;
        Scheduler::Clock::time_point end_tp;

        // Total measure
        Scheduler::Clock::duration total_duration;

        void reset();

        void begin();

        void end();

        template <typename Duration>
        int64_t total();
    };

private:
    // Frontend and controller variables
    std::shared_ptr<Frontend> m_Frontend;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;

    // State
    bool m_Running = false;

    struct {
        bool active = false;
        int framesToSkip = 0;
    } m_SpeedUp;

    // Components
    std::shared_ptr<Cpu65816> m_Cpu;
    std::shared_ptr<Dma> m_Dma;

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

    // IRQ
    bool m_Vblank = false;

    // Scheduling
    uint64_t m_MasterClock = 0;
    Clock::time_point m_NextPresent;

    DurationTool m_CpuTime;
    DurationTool m_PpuTime;

};
