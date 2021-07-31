#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <filesystem>

#include "65816.h"
#include "apu.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "maths.h"
#include "membus.h"
#include "ppu.h"
#include "registers.h"
#include "wram.h"

#include "snes.h"

#define TAG "SNES"

namespace {

constexpr uint32_t kLowRomHeader_Title = 0x7FC0;
constexpr uint32_t kLowRomHeader_TitleSize = 21;

constexpr bool kLogTimings = true;

} // anonymous namespace

void Snes::DurationTool::reset()
{
    total_duration = Snes::Clock::duration::zero();
}

void Snes::DurationTool::begin()
{
    if (!kLogTimings) {
        return;
    }

    begin_tp = Clock::now();
}

void Snes::DurationTool::end()
{
    if (!kLogTimings) {
        return;
    }

    end_tp = Clock::now();
    total_duration += end_tp - begin_tp;
}

template <typename Duration>
int64_t Snes::DurationTool::total()
{
    if (!kLogTimings) {
        return 0;
    }

    return std::chrono::duration_cast<Duration>(total_duration).count();
}

Snes::Snes()
    : MemComponent(MemComponentType::irq),
      Scheduler(){
}

int Snes::addRenderer(const std::shared_ptr<SnesRenderer>& renderer)
{
    m_RendererList.push_back(renderer);

    return 0;
}

int Snes::removeRenderer(const std::shared_ptr<SnesRenderer>& renderer)
{
    auto it = std::find(m_RendererList.begin(), m_RendererList.end(), renderer);
    if (it == m_RendererList.end()) {
        return -ENOENT;
    }

    LOGD(TAG, "Renderer removed");
    m_RendererList.erase(it);

    return 0;
}

int Snes::plugCartidge(const char* path)
{
    int ret;

    // Open file and get its size
    LOGI(TAG, "Loading '%s'", path);

    FILE* f = fopen(path, "rb");
    if (!f) {
        ret = -errno;
        LOG_ERRNO(TAG, "fopen");
        return ret;
    }

    struct stat fStat;
    ret = fstat(fileno(f), &fStat);
    if (ret == -1) {
        ret = -errno;
        LOG_ERRNO(TAG, "fstat");
        return ret;
    }

    LOGI(TAG, "Rom size: %lu bytes", fStat.st_size);

    // Read file content
    std::vector<uint8_t> rom(fStat.st_size);

    size_t readRet = fread(rom.data(), 1, fStat.st_size, f);
    if (readRet < static_cast<size_t>(fStat.st_size)) {
        LOGE(TAG, "Fail to read ROM (got only %zu bytes)", readRet);
        ret = -EIO;
        goto close_f;
    }

    romData = std::move(rom);

    // Parse header
    char title[kLowRomHeader_TitleSize+1];
    memset(title, 0, sizeof(title));
    strncpy(title, reinterpret_cast<const char*>(&romData[kLowRomHeader_Title]), kLowRomHeader_TitleSize);
    LOGI(TAG, "ROM title: '%s'", title);

    fclose(f);

    m_RomBasename = std::filesystem::path(path).replace_extension("").string();

    return 0;

close_f:
    fclose(f);

    return ret;
}

std::string Snes::getRomBasename() const
{
    return m_RomBasename;
}

int Snes::start()
{
    auto membus = std::make_shared<Membus>(AddressingType::lowrom);

    auto rom = std::make_shared<BufferMemComponent>(
        MemComponentType::rom,
        std::move(romData));
    membus->plugComponent(rom);

    m_Ram = std::make_shared<Wram>();
    membus->plugComponent(m_Ram);

    m_IndirectWram = std::make_shared<IndirectWram>(m_Ram);
    membus->plugComponent(m_IndirectWram);

    m_Sram = std::make_shared<BufferMemComponent>(
        MemComponentType::sram,
        kSramSize);
    membus->plugComponent(m_Sram);
    loadSram();

    m_Apu = std::make_shared<Apu>();
    membus->plugComponent(m_Apu);

    auto renderCb = [this](const SnesColor& c) {
        for (const auto& renderer : m_RendererList) {
            renderer->drawPixel(c);
        }
    };

    m_Ppu = std::make_shared<Ppu>(renderCb);
    membus->plugComponent(m_Ppu);

    m_Maths = std::make_shared<Maths>();
    membus->plugComponent(m_Maths);

    m_Dma = std::make_shared<Dma>(membus);
    membus->plugComponent(m_Dma);

    m_ControllerPorts = std::make_shared<ControllerPorts>();
    membus->plugComponent(m_ControllerPorts);

    m_Cpu = std::make_shared<Cpu65816>(membus);

    auto snes = shared_from_this();
    membus->plugComponent(snes);

    m_Dma->setScheduler(snes);

    return 0;
}

int Snes::stop()
{
    saveSram();

    return 0;
}

int Snes::renderSingleFrame(bool renderPpu)
{
    bool scanEnded = false;

    if (renderPpu) {
        m_Ppu->setDrawConfig(Ppu::DrawConfig::Draw);
    } else {
        m_Ppu->setDrawConfig(Ppu::DrawConfig::Skip);
    }

    while (!scanEnded) {
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
                for (const auto& renderer : m_RendererList) {
                    renderer->scanStarted();
                }

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
                m_Vblank = false;
                m_HVBJOY &= ~(1 << 7);

                if (kLogTimings) {
                    LOGI(TAG, "CPU: %" PRId64 " ms - PPU: %" PRId64 " ms",
                         m_CpuTime.total<std::chrono::milliseconds>(),
                         m_PpuTime.total<std::chrono::milliseconds>());

                    m_CpuTime.reset();
                    m_PpuTime.reset();
                }

                for (const auto& renderer : m_RendererList) {
                    renderer->scanEnded();
                }

                scanEnded = true;
            }
        }

        m_MasterClock++;
    }

    return 0;
}

void Snes::resumeTask(SchedulerTask* task, int cycles)
{
    task->setNextRunCycle(m_MasterClock + cycles);
}

void Snes::setHVIRQ_Flag(bool v)
{
    m_HVIRQ_Flag = v;
    m_Cpu->setIRQ(v);
}


void Snes::setController1(const SnesController& controller)
{
    m_ControllerPorts->setController1(controller);
}

void Snes::saveState(const std::string& path)
{
    LOGI(TAG, "Save state to %s", path.c_str());

    FILE* f = fopen(path.c_str(), "wb");
    assert(f);

    m_Ram->dumpToFile(f);
    m_IndirectWram->dumpToFile(f);
    m_Apu->dumpToFile(f);
    m_Ppu->dumpToFile(f);
    m_Maths->dumpToFile(f);
    m_Dma->dumpToFile(f);
    m_ControllerPorts->dumpToFile(f);
    m_Cpu->dumpToFile(f);

    fwrite(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fwrite(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fwrite(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fwrite(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fwrite(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fwrite(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fwrite(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fwrite(&m_Vblank, sizeof(m_Vblank), 1, f);
    fwrite(&m_MasterClock, sizeof(m_MasterClock), 1, f);

    fclose(f);
}

void Snes::loadState(const std::string& path)
{
    LOGI(TAG, "Load state from %s", path.c_str());

    FILE* f = fopen(path.c_str(), "rb");
    assert(f);

    m_Ram->loadFromFile(f);
    m_IndirectWram->loadFromFile(f);
    m_Apu->loadFromFile(f);
    m_Ppu->loadFromFile(f);
    m_Maths->loadFromFile(f);
    m_Dma->loadFromFile(f);
    m_ControllerPorts->loadFromFile(f);
    m_Cpu->loadFromFile(f);

    fread(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fread(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fread(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fread(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fread(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fread(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fread(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fread(&m_Vblank, sizeof(m_Vblank), 1, f);
    fread(&m_MasterClock, sizeof(m_MasterClock), 1, f);

    fclose(f);
}

void Snes::loadSram()
{
    auto srmPath = m_RomBasename + ".srm";

    FILE* f = fopen(srmPath.c_str(), "rb");
    if (!f) {
        return;
    }

    LOGI(TAG, "Loading srm %s", srmPath.c_str());
    m_Sram->loadFromFile(f);
    fclose(f);
}

void Snes::saveSram()
{
    auto srmPath = m_RomBasename + ".srm";

    FILE* f = fopen(srmPath.c_str(), "wb");
    if (!f) {
        return;
    }

    LOGI(TAG, "Saving to srm %s", srmPath.c_str());
    m_Sram->dumpToFile(f);
    fclose(f);
}

uint8_t Snes::readU8(uint32_t addr)
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

void Snes::writeU8(uint32_t addr, uint8_t value)
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
