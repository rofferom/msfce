#pragma once

#include <memory>
#include <thread>

#include "memcomponent.h"

class ControllerPorts;
class Cpu65816;
class Frontend;
class Ppu;

class Scheduler : public MemComponent {
public:
    Scheduler(
        const std::shared_ptr<Frontend>& frontend,
        const std::shared_ptr<Cpu65816>& cpu,
        const std::shared_ptr<Ppu>& ppu,
        const std::shared_ptr<ControllerPorts>& controllerPorts);

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    int run();

private:
    void cpuLoop();

private:
    // Frontend and controller variables
    std::shared_ptr<Frontend> m_Frontend;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;

    // CPU and PPU variables
    std::shared_ptr<Cpu65816> m_Cpu;
    bool m_RunCpu = false;
    std::thread m_CpuThread;

    std::shared_ptr<Ppu> m_Ppu;

    // MemComponent variables
    // VBlank interrupt
    bool m_NMIEnabled = false;

    // Joypad interrupt
    bool m_JoypadAutoread = false;

};
