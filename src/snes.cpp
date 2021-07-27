#include <assert.h>
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
#include "scheduler.h"
#include "wram.h"

#include "snes.h"

#define TAG "SNES"

constexpr uint32_t kLowRomHeader_Title = 0x7FC0;
constexpr uint32_t kLowRomHeader_TitleSize = 21;

Snes::Snes(const std::shared_ptr<SnesRenderer>& renderer)
    : m_Renderer(renderer)
{
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

    m_RomBasename = std::filesystem::path(path).replace_extension("");

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

    m_Ppu = std::make_shared<Ppu>(m_Renderer);
    membus->plugComponent(m_Ppu);

    m_Maths = std::make_shared<Maths>();
    membus->plugComponent(m_Maths);

    m_Dma = std::make_shared<Dma>(membus);
    membus->plugComponent(m_Dma);

    m_ControllerPorts = std::make_shared<ControllerPorts>();
    membus->plugComponent(m_ControllerPorts);

    m_Cpu = std::make_shared<Cpu65816>(membus);

    m_Scheduler = std::make_shared<Scheduler>(m_Renderer, m_Cpu, m_Dma, m_Ppu, m_ControllerPorts);
    membus->plugComponent(m_Scheduler);

    m_Dma->setScheduler(m_Scheduler);

    return 0;
}

int Snes::stop()
{
    saveSram();

    return 0;
}

int Snes::renderSingleFrame(bool renderPpu)
{
    return m_Scheduler->renderSingleFrame(renderPpu);
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
    m_Scheduler->dumpToFile(f);

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
    m_Scheduler->loadFromFile(f);

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
