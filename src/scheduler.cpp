#include <assert.h>

#include <chrono>

#include "65816.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "ppu.h"
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
            LOGD(TAG, "NMI is now %s", m_NMIEnabled ? "enabled" : "disabled");
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
    // Start CPU
    m_RunCpu = true;
    m_CpuThread = std::thread(&Scheduler::cpuLoop, this);

    // Main loop
    auto nextRender = Clock::now() + kRenderPeriod;
    auto nextVblank = nextRender - kVblankDuration;

    while (true) {
        // Run frontend
        bool frontendContinue = m_Frontend->runOnce();
        if (!frontendContinue) {
            break;
        }

        // Display
        auto now = Clock::now();

        if (now >= nextRender) {
            auto beginRender = Clock::now();
            m_Ppu->render();
            m_Frontend->present();
            auto endRender = Clock::now();

            auto renderDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endRender - beginRender);

            if (renderDuration < kRenderPeriod) {
                nextRender += kRenderPeriod;
            } else {
                nextRender = Clock::now() + kRenderPeriod;
            }

            nextVblank = nextRender - kVblankDuration;
        } else if (now >= nextVblank) {
            if (m_JoypadAutoread) {
                m_ControllerPorts->readController();
            }

            // Dirty hack to avoid vblank to be retriggered
            if (m_NMIEnabled) {
                m_Cpu->setNMI();
            }

            nextVblank += kRenderPeriod;
        }
    }

    // Stop CPU
    m_RunCpu = false;
    m_CpuThread.join();

    return 0;
}

void Scheduler::cpuLoop()
{
    while (m_RunCpu) {
        int dmaCycles = m_Dma->run();

        if (dmaCycles == 0) {
            m_Cpu->executeSingle();
        }
    }
}
