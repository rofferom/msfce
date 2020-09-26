#pragma once

#include <memory>
#include <thread>

class ControllerPorts;
class Cpu65816;
class Frontend;
class Ppu;

class Scheduler {
public:
    Scheduler(
        const std::shared_ptr<Frontend>& frontend,
        const std::shared_ptr<Cpu65816>& cpu,
        const std::shared_ptr<Ppu>& ppu,
        const std::shared_ptr<ControllerPorts>& controllerPorts);

    int run();

private:
    void cpuLoop();

private:
    std::shared_ptr<Frontend> m_Frontend;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;

    std::shared_ptr<Cpu65816> m_Cpu;
    bool m_RunCpu = false;
    std::thread m_CpuThread;

    std::shared_ptr<Ppu> m_Ppu;
};
