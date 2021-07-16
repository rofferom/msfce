#include <assert.h>

#include <chrono>

#include "65816.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "ppu.h"
#include "schedulertask.h"
#include "scheduler.h"

#define TAG "scheduler"

namespace {

constexpr bool kLogTimings = true;

constexpr int kSpeedupFrameSkip = 2; // x3

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);

} // anonymous namespace

void Scheduler::DurationTool::reset()
{
    total_duration = Scheduler::Clock::duration::zero();
}

void Scheduler::DurationTool::begin()
{
    if (!kLogTimings) {
        return;
    }

    begin_tp = Scheduler::Clock::now();
}

void Scheduler::DurationTool::end()
{
    if (!kLogTimings) {
        return;
    }

    end_tp = Scheduler::Clock::now();
    total_duration += end_tp - begin_tp;
}

template <typename Duration>
int64_t Scheduler::DurationTool::total()
{
    if (!kLogTimings) {
        return 0;
    }

    return std::chrono::duration_cast<Duration>(total_duration).count();
}

Scheduler::Scheduler(
    const std::shared_ptr<Frontend>& frontend,
    const std::shared_ptr<Cpu65816>& cpu,
    const std::shared_ptr<Dma>& dma,
    const std::shared_ptr<Ppu>& ppu,
    const std::shared_ptr<ControllerPorts>& controllerPorts)
    : MemComponent(MemComponentType::irq),
      m_Frontend(frontend),
      m_ControllerPorts(controllerPorts),
      m_Cpu(cpu),
      m_Dma(dma),
      m_Ppu(ppu)
{
}

uint8_t Scheduler::readU8(uint32_t addr)
{
    switch (addr) {
    case kRegRDNMI: {
        return (m_Vblank << 7) | (1 << 6);
    }

    case kRegTIMEUP: {
        uint8_t ret = m_HVIRQ_Flag << 7;
        setHVIRQ_Flag(0);
        return ret;
    }

    case kRegHVBJOY:
        return m_HVBJOY;
    }

    LOGW(TAG, "Ignore ReadU8 at %06X", addr);
    assert(false);
    return 0;
}

void Scheduler::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr) {
    case kRegNmitimen: {
        // H/V IRQ
        uint8_t enableHVIRQ = (value >> 4) & 0b11;
        if (m_HVIRQ_Config != enableHVIRQ) {
            LOGD(TAG, "H/V IRQ is now %s", m_HVIRQ_Config ? "enabled" : "disabled");
            m_HVIRQ_Config = enableHVIRQ;

            m_Ppu->setHVIRQConfig(
                static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
                m_HVIRQ_H,
                m_HVIRQ_V);
            break;
        }

        // NMI
        bool enableNMI = value & (1 << 7);
        if (m_NMIEnabled != enableNMI) {
            m_NMIEnabled = enableNMI;
            LOGI(TAG, "NMI is now %s", m_NMIEnabled ? "enabled" : "disabled");
        }

        // Joypad
        bool joypadAutoread = value & 1;
        if (m_JoypadAutoread != joypadAutoread) {
            LOGI(TAG, "Joypad autoread is now %s", joypadAutoread ? "enabled" : "disabled");
            m_JoypadAutoread = joypadAutoread;
        }

        break;
    }

    case kRegisterHTIMEL:
        m_HVIRQ_H = (m_HVIRQ_H & 0xFF00) | value;
        break;

    case kRegisterHTIMEH:
        m_HVIRQ_H = (m_HVIRQ_H & 0xFF) | (value << 8);
        break;

    case kRegisterVTIMEL:
        m_HVIRQ_V = (m_HVIRQ_V & 0xFF00) | value;
        break;

    case kRegisterVTIMEH:
        m_HVIRQ_V = (m_HVIRQ_V & 0xFF) | (value << 8);
        break;
    }
}

void Scheduler::toggleRunning()
{
    m_Running = !m_Running;
}

int Scheduler::run()
{
    bool run = true;

    m_NextPresent = Clock::now() + kRenderPeriod;

    // Register components
    m_Dma->setScheduler(shared_from_this());

    m_Running = true;

    while (run) {
        if (m_Running) {
            run = runRunning();
        } else {
            run = runPaused();
        }
    }

    return 0;
}

bool Scheduler::runRunning()
{
    bool keepRunning = true;

    // DMA has priority over CPU
    if (m_Dma->getState() == SchedulerTask::State::running) {
        if (m_Dma->getNextRunCycle() == m_MasterClock) {
            int dmaCycles = m_Dma->run();

            if (dmaCycles > 0) {
                m_Dma->setNextRunCycle(m_MasterClock + dmaCycles);
            } else {
                m_Dma->setIdle();

                // Resume CPU at a multiple of 8 cycles. 0 is forbidden
                auto cpuSync = m_MasterClock % 8;
                if (cpuSync == 0) {
                    cpuSync = 8;
                }

                m_Cpu->setNextRunCycle(m_MasterClock + cpuSync);
            }
        }
    } else if (m_Cpu->getNextRunCycle() == m_MasterClock) {
        m_CpuTime.begin();
        int cpuCycles = m_Cpu->run();
        m_CpuTime.end();

        m_Cpu->setNextRunCycle(m_MasterClock + cpuCycles);
    }

    // Always run PPU
    if (m_Ppu->getNextRunCycle() == m_MasterClock) {
        m_PpuTime.begin();
        int ppuCycles = m_Ppu->run();
        m_PpuTime.end();

        m_Ppu->setNextRunCycle(m_MasterClock + ppuCycles);
        auto ppuEvents = m_Ppu->getEvents();

        if (ppuEvents & Ppu::Event_ScanStarted) {
            m_Dma->onScanStarted();
        }

        if (ppuEvents & Ppu::Event_HBlankStart) {
            m_Dma->onHblank();

            m_HVBJOY |= 1 << 6;
        }

        if (ppuEvents & Ppu::Event_HBlankEnd) {
            m_HVBJOY &= ~(1 << 6);
        }

        if (ppuEvents & Ppu::Event_HV_IRQ) {
            setHVIRQ_Flag(true);
        }

        if (ppuEvents & Ppu::Event_VBlankStart) {
            m_Vblank = true;
            m_HVBJOY |= 1 << 7;

            m_Dma->onVblank();

            if (m_JoypadAutoread) {
                m_ControllerPorts->readController();
            }

            // Dirty hack to avoid vblank to be retriggered
            if (m_NMIEnabled) {
                m_Cpu->setNMI();
            }
        }

        if (ppuEvents & Ppu::Event_ScanEnded) {
            // In case of speedup, present can be skipped
            bool doPresent = true;

            if (m_SpeedUp.active) {
                if (m_SpeedUp.framesToSkip == 0) {
                    m_Ppu->setDrawConfig(Ppu::DrawConfig::Draw);
                    m_SpeedUp.framesToSkip = kSpeedupFrameSkip;
                } else {
                    m_Ppu->setDrawConfig(Ppu::DrawConfig::Skip);
                    m_SpeedUp.framesToSkip--;
                    doPresent = false;
                }
            }

            m_Vblank = false;
            m_HVBJOY &= ~(1 << 7);

            if (kLogTimings) {
                LOGI(TAG, "CPU: %lu ms - PPU: %lu ms",
                     m_CpuTime.total<std::chrono::milliseconds>(),
                     m_PpuTime.total<std::chrono::milliseconds>());

                m_CpuTime.reset();
                m_PpuTime.reset();
            }

            if (doPresent) {
                std::this_thread::sleep_until(m_NextPresent);
                m_Frontend->present();
                m_NextPresent += kRenderPeriod;
            }

            // Run frontend after present
            keepRunning = m_Frontend->runOnce();
        }
    }

    m_MasterClock++;

    return keepRunning;
}

bool Scheduler::runPaused()
{
    // Continue to run frontend to process inputs
    std::this_thread::sleep_until(m_NextPresent);
    m_NextPresent += kRenderPeriod;

    // Run frontend after present
    return m_Frontend->runOnce();
}

void Scheduler::resumeTask(SchedulerTask* task, int cycles)
{
    task->setNextRunCycle(m_MasterClock + cycles);
}

void Scheduler::speedUp()
{
    m_SpeedUp.active = true;
    m_Ppu->setDrawConfig(Ppu::DrawConfig::Skip);
    m_SpeedUp.framesToSkip = kSpeedupFrameSkip;
}

void Scheduler::speedNormal()
{
    m_SpeedUp.active = false;
    m_Ppu->setDrawConfig(Ppu::DrawConfig::Draw);
}

void Scheduler::pause()
{
    m_Running = false;
}

void Scheduler::resume()
{
    m_Running = true;
}

void Scheduler::setHVIRQ_Flag(bool v)
{
    m_HVIRQ_Flag = v;
    m_Cpu->setIRQ(v);
}

void Scheduler::dumpToFile(FILE* f)
{
    fwrite(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fwrite(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fwrite(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fwrite(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fwrite(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fwrite(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fwrite(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fwrite(&m_Vblank, sizeof(m_Vblank), 1, f);
}

void Scheduler::loadFromFile(FILE* f)
{
    fread(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fread(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fread(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fread(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fread(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fread(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fread(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fread(&m_Vblank, sizeof(m_Vblank), 1, f);
}
