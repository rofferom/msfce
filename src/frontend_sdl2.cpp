#include <assert.h>

#include <chrono>
#include <thread>
#include <unordered_map>

#include "controller.h"
#include "log.h"
#include "ppu.h"
#include "snes.h"
#include "frontend_sdl2.h"

#define TAG "FrontendSdl2"

namespace {

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr int kSpeedupFrameSkip = 3; // x4 (skip 3 frames)

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

bool* controllerGetButton(
    SnesController* controller,
    const SnesControllerMapping& mapping)
{
    auto rawPtr = reinterpret_cast<uint8_t *>(controller) + mapping.offset;
    return reinterpret_cast<bool *>(rawPtr);
}

bool handleContollerKey(
    SnesController* controller,
    SDL_Scancode scancode,
    bool pressed)
{
    LOGD(TAG, "Scancode %d %s",
        scancode,
        pressed ? "pressed" : "released");

    auto it = s_ControllerMapping.find(scancode);
    if (it == s_ControllerMapping.end()) {
        return false;
    }

    const auto& mapping = it->second;

    LOGD(TAG, "Button %s (scancode %d) %s",
        mapping.name,
        scancode,
        pressed ? "pressed" : "released");

    auto buttonValue = controllerGetButton(controller, mapping);
    *buttonValue = pressed;

    return true;
}

} // anonymous namespace

FrontendSdl2::FrontendSdl2()
    : Frontend(),
      SnesRenderer()
{
}

FrontendSdl2::~FrontendSdl2()
{
    SDL_Quit();
}

int FrontendSdl2::init()
{
    SDL_Init(SDL_INIT_VIDEO);

    m_Window = SDL_CreateWindow(
        "Monkey Super Famicom Emulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kPpuDisplayWidth, kPpuDisplayHeight,
        0);
    assert(m_Window);

    m_Surface = SDL_GetWindowSurface(m_Window);
    assert(m_Surface);

    return 0;
}

int FrontendSdl2::run()
{
    bool run = true;

    auto presentTp = std::chrono::high_resolution_clock::now() + kRenderPeriod;

    while (run) {
        SDL_Event event;

        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                run = false;
                break;
            }

            bool keyHandled;

            switch (event.type) {
            case SDL_KEYDOWN:
                keyHandled = handleContollerKey(&m_Controller1, event.key.keysym.scancode, true);
                if (keyHandled) {
                    break;
                }

                if (event.key.repeat) {
                    break;
                }

                handleShortcut(event.key.keysym.scancode, true);
                break;

            case SDL_KEYUP:
                keyHandled = handleContollerKey(&m_Controller1, event.key.keysym.scancode, false);
                if (keyHandled) {
                    break;
                }

                if (event.key.repeat) {
                    break;
                }

                handleShortcut(event.key.keysym.scancode, false);
                break;

            default:
                break;
            }
        }

        if (m_Running) {
            m_Snes->setController1(m_Controller1);

            if (m_SpeedUp) {
                for (int i = 0; i < kSpeedupFrameSkip; i++) {
                    m_Snes->renderSingleFrame(false);
                }
            }

            m_Snes->renderSingleFrame();
        }

        std::this_thread::sleep_until(presentTp);
        SDL_UpdateWindowSurface(m_Window);
        presentTp += kRenderPeriod;
    }

    return true;
}

void FrontendSdl2::setSnes(const std::shared_ptr<Snes>& snes)
{
    m_Snes = snes;
}

void FrontendSdl2::scanStarted()
{
    SDL_LockSurface(m_Surface);
    m_SurfaceData = reinterpret_cast<uint8_t*>(m_Surface->pixels);;
}

void FrontendSdl2::drawPixel(const SnesColor& c)
{
    m_SurfaceData[0] = c.b;
    m_SurfaceData[1] = c.g;
    m_SurfaceData[2] = c.r;

    m_SurfaceData += m_Surface->format->BytesPerPixel;
}

void FrontendSdl2::scanEnded()
{
    m_SurfaceData = nullptr;
    SDL_UnlockSurface(m_Surface);
}

bool FrontendSdl2::handleShortcut(
    SDL_Scancode scancode,
    bool pressed)
{
    switch (scancode) {
    case SDL_SCANCODE_P:
        if (pressed) {
            m_Running = !m_Running;
        }
        break;

    case SDL_SCANCODE_GRAVE:
        if (pressed) {
            LOGI(TAG, "Speedup");
            m_SpeedUp = true;
        } else {
            LOGI(TAG, "Normal speed");
            m_SpeedUp = false;
        }
        break;

    case SDL_SCANCODE_F2: {
        if (pressed) {
            m_Snes->saveState(getSavestateName());
        }
        break;
    }

    case SDL_SCANCODE_F4: {
        if (pressed) {
            m_Snes->loadState(getSavestateName());
        }
        break;
    }

    default:
        break;
    }

    return true;
}

std::string FrontendSdl2::getSavestateName() const
{
    return m_Snes->getRomBasename() + ".msfe";
}
