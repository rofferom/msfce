#pragma once

#include <memory>
#include <string>
#include <vector>

#include "controller.h"
#include "snes_renderer.h"

class Apu;
class BufferMemComponent;
class ControllerPorts;
class Cpu65816;
class Dma;
class IndirectWram;
class Maths;
class Ppu;
class Scheduler;
class Wram;

class Snes {
public:
    Snes(const std::shared_ptr<SnesRenderer>& renderer);

    int plugCartidge(const char* path);
    std::string getRomBasename() const;

    int start();
    int stop();

    int renderSingleFrame(bool renderPpu = true);

    void setController1(const SnesController& controller);

    void saveState(const std::string& path);
    void loadState(const std::string& path);

private:
    int loadRom(const char* romPath, std::vector<uint8_t>* outRom);

    void loadSram();
    void saveSram();

private:
    std::shared_ptr<SnesRenderer> m_Renderer;
    std::string m_RomBasename;

    std::shared_ptr<Wram> m_Ram;
    std::shared_ptr<IndirectWram> m_IndirectWram;
    std::shared_ptr<BufferMemComponent> m_Sram;
    std::shared_ptr<Apu> m_Apu;
    std::shared_ptr<ControllerPorts> m_ControllerPorts;
    std::shared_ptr<Cpu65816> m_Cpu;
    std::shared_ptr<Dma> m_Dma;
    std::shared_ptr<Maths> m_Maths;
    std::shared_ptr<Ppu> m_Ppu;
    std::shared_ptr<Scheduler> m_Scheduler;

    std::vector<uint8_t> romData;
};
