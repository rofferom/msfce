#include <chrono>

#include "65816.h"
#include "controller.h"
#include "ppu.h"
#include "scheduler.h"

using Clock = std::chrono::high_resolution_clock;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr auto kVblankDuration = std::chrono::microseconds(2400);

Scheduler::Scheduler(
    const std::shared_ptr<Frontend>& frontend,
    const std::shared_ptr<Cpu65816>& cpu,
    const std::shared_ptr<Ppu>& ppu,
    const std::shared_ptr<ControllerPorts>& controllerPorts)
    : m_Frontend(frontend),
      m_ControllerPorts(controllerPorts),
      m_Cpu(cpu),
      m_Ppu(ppu)
{
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
                nextRender = nextRender + kRenderPeriod;
            } else {
                nextRender = Clock::now() + kRenderPeriod;
            }
            nextVblank = nextRender - kVblankDuration;
        } else if (now >= nextVblank) {
            if (m_Ppu->isJoypadAutoreadEnabled()) {
                m_ControllerPorts->readController();
            }

            // Dirty hack to avoid vblank to be retriggered
            if (m_Ppu->isNMIEnabled()) {
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
        m_Cpu->executeSingle();
    }
}
