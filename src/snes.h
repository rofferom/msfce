#pragma once

#include <memory>
#include <string>
#include <vector>

class Apu;
class BufferMemComponent;
class ControllerPorts;
class Cpu65816;
class Dma;
class Frontend;
class IndirectWram;
class Maths;
class Ppu;
class Scheduler;
class Wram;

class Snes {
public:
    Snes(const std::shared_ptr<Frontend>& frontend);

    int plugCartidge(const char* path);
    std::string getRomBasename() const;

    void run();

    void toggleRunning();

    void speedUp();
    void speedNormal();

    void saveState(const std::string& path);
    void loadState(const std::string& path);

private:
    int loadRom(const char* romPath, std::vector<uint8_t>* outRom);

private:
    std::shared_ptr<Frontend> m_Frontend;
    std::string m_RomBasename;

    std::shared_ptr<Wram> m_Ram;
    std::shared_ptr<IndirectWram> m_IndirectWram;
    std::shared_ptr<Apu> m_Apu;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;
    std::shared_ptr<Cpu65816> m_Cpu;
    std::shared_ptr<Dma> m_Dma;
    std::shared_ptr<Maths> m_Maths;
    std::shared_ptr<Ppu> m_Ppu;
    std::shared_ptr<Scheduler> m_Scheduler;

    std::vector<uint8_t> romData;
};
