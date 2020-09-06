#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <cstddef>
#include <chrono>
#include <unordered_map>
#include <memory>
#include <thread>
#include <vector>
#include <SDL.h>
#include "65816.h"
#include "apu.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "maths.h"
#include "membus.h"
#include "ppu.h"
#include "registers.h"

#define TAG "main"

constexpr uint32_t kLowRomHeader_Title = 0x7FC0;
constexpr uint32_t kLowRomHeader_TitleSize = 21;

using Clock = std::chrono::high_resolution_clock;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr auto kVblankDuration = std::chrono::microseconds(2400);

struct SnesControllerMapping {
    const char* name;
    off64_t offset;
};

static const std::unordered_map<SDL_Scancode, SnesControllerMapping> s_ControllerMapping = {
    { SDL_SCANCODE_UP,    { "Up",    offsetof(SnesController, up) } },
    { SDL_SCANCODE_DOWN,  { "Down",  offsetof(SnesController, down) } },
    { SDL_SCANCODE_LEFT,  { "Left",  offsetof(SnesController, left) } },
    { SDL_SCANCODE_RIGHT, { "Right", offsetof(SnesController, right) } },

    { SDL_SCANCODE_RETURN, { "Start",  offsetof(SnesController, start) } },
    { SDL_SCANCODE_RSHIFT, { "Select", offsetof(SnesController, select) } },

    { SDL_SCANCODE_Q, { "L", offsetof(SnesController, l) } },
    { SDL_SCANCODE_W, { "R", offsetof(SnesController, r) } },

    { SDL_SCANCODE_A, { "Y", offsetof(SnesController, y) } },
    { SDL_SCANCODE_S, { "X", offsetof(SnesController, x) } },
    { SDL_SCANCODE_Z, { "B", offsetof(SnesController, b) } },
    { SDL_SCANCODE_X, { "A", offsetof(SnesController, a) } },
};

static bool* controllerGetButton(
    const std::shared_ptr<SnesController>& controller,
    const SnesControllerMapping& mapping)
{
    auto rawPtr = (reinterpret_cast<uint8_t *>(controller.get()) + mapping.offset);
    return reinterpret_cast<bool *>(rawPtr);
}

static void handleKey(
    const std::shared_ptr<SnesController>& controller,
    SDL_Scancode scancode,
    bool pressed)
{
    LOGD(TAG, "Scancode %d %s",
        scancode,
        pressed ? "pressed" : "released");

    auto it = s_ControllerMapping.find(scancode);
    if (it == s_ControllerMapping.end()) {
        return;
    }

    const auto& mapping = it->second;

    LOGD(TAG, "Button %s (scancode %d) %s",
        mapping.name,
        scancode,
        pressed ? "pressed" : "released");

    auto buttonValue = controllerGetButton(controller, mapping);
    *buttonValue = pressed;
}

static int loadRom(const char* romPath, std::vector<uint8_t>* outRom)
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
    std::vector<uint8_t> rom(fStat.st_size);

    size_t readRet = fread(rom.data(), 1, fStat.st_size, f);
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
    auto controller = std::make_shared<SnesController>();
    int ret;

    if (argc < 2) {
        LOGE(TAG, "Missing ROM arg");
        return 1;
    }

    //logSetLevel(LOG_DEBUG);

    LOGI(TAG, "Welcome to Monkey Super Famicom Emulator");

    // Init SDL
    SDL_Window* window;
    SDL_Renderer* renderer;

    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow(
        "Monkey Super Famicom Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kPpuDisplayWidth, kPpuDisplayHeight,
        0);
    assert(window);

    renderer = SDL_CreateRenderer(window, -1, 0);
    assert(renderer);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    auto drawPointCb = [renderer](int x, int y, const Color& c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderDrawPoint(renderer, x, y);
    };

    // Load ROM
    const char* romPath = argv[1];

    std::vector<uint8_t> romData;
    ret = loadRom(romPath, &romData);
    if (ret < 0) {
        return 1;
    }

    const uint8_t* romRawPtr = romData.data();

    // Parse header
    char title[kLowRomHeader_TitleSize+1];
    memset(title, 0, sizeof(title));
    strncpy(title, reinterpret_cast<const char*>(&romRawPtr[kLowRomHeader_Title]), kLowRomHeader_TitleSize);
    LOGI(TAG, "ROM title: '%s'", title);

    // Create components
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

    auto ppu = std::make_shared<Ppu>();
    membus->plugComponent(ppu);

    auto maths = std::make_shared<Maths>();
    membus->plugComponent(maths);

    auto dma = std::make_shared<Dma>(membus);
    membus->plugComponent(dma);

    auto controllerPorts = std::make_shared<ControllerPorts>(controller);
    membus->plugComponent(controllerPorts);

    auto cpu = std::make_shared<Cpu65816>(membus);
    bool cpuRun = true;

    auto cpuThreadEntry = [cpu, &cpuRun]() {
        while (cpuRun) {
            cpu->executeSingle();
        }
    };

    std::thread cpuThread = std::thread(cpuThreadEntry);

    // Run
    auto nextRender = Clock::now() + kRenderPeriod;
    auto nextVblank = nextRender - kVblankDuration;

    while (true) {
        // Poll SDL event
        SDL_Event event;
        int pendingEvent = SDL_PollEvent(&event);
        if (pendingEvent) {
            if (event.type == SDL_QUIT) {
                break;
            }

            switch (event.type) {
            case SDL_KEYDOWN:
                handleKey(controller, event.key.keysym.scancode, true);
                break;

            case SDL_KEYUP:
                handleKey(controller, event.key.keysym.scancode, false);
                break;

            default:
                break;
            }
        }

        // Emulation loop
        auto now = Clock::now();

        if (now >= nextRender) {
            auto beginRender = Clock::now();
            ppu->render(drawPointCb);
            SDL_RenderPresent(renderer);
            auto endRender = Clock::now();

            auto renderDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endRender - beginRender);

            if (renderDuration < kRenderPeriod) {
                nextRender = nextRender + kRenderPeriod;
            } else {
                nextRender = Clock::now() + kRenderPeriod;
            }
            nextVblank = nextRender - kVblankDuration;
        } else if (now >= nextVblank) {
            if (ppu->isJoypadAutoreadEnabled()) {
                controllerPorts->readController();
            }

            // Dirty hack to avoid vblank to be retriggered
            if (ppu->isNMIEnabled()) {
                cpu->setNMI();
            }

            nextVblank += kRenderPeriod;
        }
    }

    cpuRun = false;
    cpuThread.join();

    SDL_Quit();

    return 0;
}
