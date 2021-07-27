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

Snes::Snes(const std::shared_ptr<Frontend>& frontend)
    : m_Frontend(frontend)
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

void Snes::run()
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

    auto sram = std::make_shared<BufferMemComponent>(
        MemComponentType::sram,
        kSramSize);
    membus->plugComponent(sram);

    m_Apu = std::make_shared<Apu>();
    membus->plugComponent(m_Apu);

    m_Ppu = std::make_shared<Ppu>(m_Frontend);
    membus->plugComponent(m_Ppu);

    m_Maths = std::make_shared<Maths>();
    membus->plugComponent(m_Maths);

    m_Dma = std::make_shared<Dma>(membus);
    membus->plugComponent(m_Dma);

    m_ControllerPorts = std::make_shared<ControllerPorts>(m_Frontend);
    membus->plugComponent(m_ControllerPorts);

    m_Cpu = std::make_shared<Cpu65816>(membus);

    m_Scheduler = std::make_shared<Scheduler>(m_Frontend, m_Cpu, m_Dma, m_Ppu, m_ControllerPorts);
    membus->plugComponent(m_Scheduler);

    m_Scheduler->run();
}

void Snes::toggleRunning()
{
    m_Scheduler->toggleRunning();
}

void Snes::speedUp()
{
    m_Scheduler->speedUp();
}

void Snes::speedNormal()
{
    m_Scheduler->speedNormal();
}

void Snes::saveState(const std::string& path)
{
    LOGI(TAG, "Save state to %s", path.c_str());

    FILE* f = fopen(path.c_str(), "wb");
    assert(f);

    m_Scheduler->pause();

    m_Ram->dumpToFile(f);
    m_IndirectWram->dumpToFile(f);
    m_Apu->dumpToFile(f);
    m_Ppu->dumpToFile(f);
    m_Maths->dumpToFile(f);
    m_Dma->dumpToFile(f);
    m_ControllerPorts->dumpToFile(f);
    m_Cpu->dumpToFile(f);
    m_Scheduler->dumpToFile(f);

    m_Scheduler->resume();

    fclose(f);
}

void Snes::loadState(const std::string& path)
{
    LOGI(TAG, "Load state from %s", path.c_str());

    FILE* f = fopen(path.c_str(), "rb");
    assert(f);

    m_Scheduler->pause();

    m_Ram->loadFromFile(f);
    m_IndirectWram->loadFromFile(f);
    m_Apu->loadFromFile(f);
    m_Ppu->loadFromFile(f);
    m_Maths->loadFromFile(f);
    m_Dma->loadFromFile(f);
    m_ControllerPorts->loadFromFile(f);
    m_Cpu->loadFromFile(f);
    m_Scheduler->loadFromFile(f);

    m_Scheduler->resume();

    fclose(f);
}
