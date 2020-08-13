#include <stdio.h>
#include <sys/stat.h>
#include <chrono>
#include <memory>
#include <vector>
#include "65816.h"
#include "apu.h"
#include "log.h"
#include "ppu.h"
#include "membus.h"

#define TAG "main"

constexpr uint32_t kLowRomHeader_Title = 0x7FC0;
constexpr uint32_t kLowRomHeader_TitleSize = 21;

using Clock = std::chrono::high_resolution_clock;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr auto kVblankDuration = std::chrono::milliseconds(1);

static int loadRom(const char* romPath, std::unique_ptr<std::vector<uint8_t>>* outRom)
{
    int ret;

    // Open file and get its size
    LOGI(TAG, "Loading '%s'", romPath);

    FILE* f = fopen(romPath, "rb");
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
    auto rom = std::make_unique<std::vector<uint8_t>>();
    rom->reserve(fStat.st_size);

    size_t readRet = fread(rom->data(), 1, fStat.st_size, f);
    if (readRet < static_cast<size_t>(fStat.st_size)) {
        LOGE(TAG, "Fail to read ROM (got only %zu bytes)", readRet);
        ret = -EIO;
        goto close_f;
    }

    *outRom = std::move(rom);

    fclose(f);

    return 0;

close_f:
    fclose(f);

    return ret;
}

int main(int argc, char* argv[])
{
    int ret;

    if (argc < 2) {
        LOGE(TAG, "Missing ROM arg");
        return 1;
    }

    logSetLevel(LOG_DEBUG);

    LOGI(TAG, "Welcome to Monkey Super Famicom Emulator");

    // Load ROM
    const char* romPath = argv[1];

    std::unique_ptr<std::vector<uint8_t>> romPtr;
    ret = loadRom(romPath, &romPtr);
    if (ret < 0) {
        return 1;
    }

    const uint8_t* rom = romPtr->data();

    // Parse header
    char title[kLowRomHeader_TitleSize+1];
    memset(title, 0, sizeof(title));
    strncpy(title, reinterpret_cast<const char*>(&rom[kLowRomHeader_Title]), kLowRomHeader_TitleSize);
    LOGI(TAG, "ROM title: '%s'", title);

    // Create components
    auto membus = std::make_shared<Membus>();

    membus->plugRom(std::move(romPtr));
    rom = nullptr;

    auto apu = std::make_shared<Apu>();
    membus->plugApu(apu);

    auto ppu = std::make_shared<Ppu>();
    membus->plugPpu(ppu);

    auto cpu = std::make_unique<Cpu65816>(membus);

    // Run
    auto nextRender = Clock::now() + kRenderPeriod;
    auto nextVblank = nextRender - kVblankDuration;

    while (true) {
        auto now = Clock::now();

        if (now >= nextRender) {
            // Rendering should be triggered here
            nextRender = Clock::now() + kRenderPeriod;
            nextVblank = nextRender - kVblankDuration;
        } else if (now >= nextVblank) {
            // Dirty hack to avoid vblank to be retriggered
            if (ppu->isNMIEnabled()) {
                cpu->setNMI();
            }

            nextVblank += kRenderPeriod;
        }

        cpu->executeSingle();
    }

    return 0;
}
