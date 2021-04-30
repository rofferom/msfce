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

using Clock = std::chrono::high_resolution_clock;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr auto kVblankDuration = std::chrono::microseconds(2400);

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
    case kRegRDNMI:
    case kRegTIMEUP:
        // Ack interrupt
        return 0;
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
        if (value & (0b11 << 4)) {
            // To be implemented
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
    }
}

int Scheduler::run()
{
    auto nextPresent = Clock::now() + kRenderPeriod;

    // Register components
    m_Dma->setScheduler(shared_from_this());

    while (true) {
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
            int cpuCycles = m_Cpu->run();

            m_Cpu->setNextRunCycle(m_MasterClock + cpuCycles);
        }

        // Always run PPU
        if (m_Ppu->getNextRunCycle() == m_MasterClock) {
            int ppuCycles = m_Ppu->run();
            m_Ppu->setNextRunCycle(m_MasterClock + ppuCycles);

            auto ppuEvents = m_Ppu->getEvents();
            if (ppuEvents & Ppu::Event_VBlankStart) {
                if (m_JoypadAutoread) {
                    m_ControllerPorts->readController();
                }

                // Dirty hack to avoid vblank to be retriggered
                if (m_NMIEnabled) {
                    m_Cpu->setNMI();
                }
            }

            if (ppuEvents & Ppu::Event_ScanEnded) {
                std::this_thread::sleep_until(nextPresent);

                //LOGE(TAG, "Present");
                m_Frontend->present();
                nextPresent += kRenderPeriod;

                // Run frontend after present
                bool frontendContinue = m_Frontend->runOnce();
                if (!frontendContinue) {
                    break;
                }
            }
        }

        m_MasterClock++;
    }

    return 0;
}

void Scheduler::resumeTask(SchedulerTask* task, int cycles)
{
    task->setNextRunCycle(m_MasterClock + cycles);
}
