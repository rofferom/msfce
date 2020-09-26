#include <stdio.h>
#include <sys/stat.h>

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

    return 0;

close_f:
    fclose(f);

    return ret;
}

void Snes::run()
{
    auto membus = std::make_shared<Membus>(AddressingType::lowrom);

    auto rom = std::make_shared<BufferMemComponent>(
        MemComponentType::rom,
        std::move(romData));
    membus->plugComponent(rom);

    auto ram = std::make_shared<BufferMemComponent>(
        MemComponentType::ram,
        kWramSize);
    membus->plugComponent(ram);

    auto sram = std::make_shared<BufferMemComponent>(
        MemComponentType::sram,
        kSramSize);
    membus->plugComponent(sram);

    auto apu = std::make_shared<Apu>();
    membus->plugComponent(apu);

    auto ppu = std::make_shared<Ppu>(m_Frontend);
    membus->plugComponent(ppu);

    auto maths = std::make_shared<Maths>();
    membus->plugComponent(maths);

    auto dma = std::make_shared<Dma>(membus);
    membus->plugComponent(dma);

    auto controllerPorts = std::make_shared<ControllerPorts>(m_Frontend);
    membus->plugComponent(controllerPorts);

    auto cpu = std::make_shared<Cpu65816>(membus);

    auto scheduler = std::make_shared<Scheduler>(m_Frontend, cpu, dma, ppu, controllerPorts);
    membus->plugComponent(scheduler);

    scheduler->run();
}
