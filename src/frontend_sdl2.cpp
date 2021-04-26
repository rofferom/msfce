#include <assert.h>

#include <unordered_map>

#include "controller.h"
#include "log.h"
#include "ppu.h"
#include "snes.h"
#include "frontend_sdl2.h"

#define TAG "FrontendSdl2"

namespace {

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
    const std::shared_ptr<SnesController>& controller,
    const SnesControllerMapping& mapping)
{
    auto rawPtr = (reinterpret_cast<uint8_t *>(controller.get()) + mapping.offset);
    return reinterpret_cast<bool *>(rawPtr);
}

bool handleContollerKey(
    const std::shared_ptr<SnesController>& controller,
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
      m_Controller1(std::make_shared<SnesController>())
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

    m_Renderer = SDL_CreateRenderer(m_Window, -1, 0);
    assert(m_Renderer);

    SDL_SetRenderDrawColor(m_Renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_Renderer);

    return 0;
}

bool FrontendSdl2::runOnce()
{
    SDL_Event event;
    bool keyHandled;

    if (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }

        switch (event.type) {
        case SDL_KEYDOWN:
            keyHandled = handleContollerKey(m_Controller1, event.key.keysym.scancode, true);
            if (keyHandled) {
                return true;
            }

            if (event.key.repeat) {
                return true;
            }

            handleShortcut(event.key.keysym.scancode, true);

            break;

        case SDL_KEYUP:
            keyHandled = handleContollerKey(m_Controller1, event.key.keysym.scancode, false);
            if (keyHandled) {
                return true;
            }

            if (event.key.repeat) {
                return true;
            }

            handleShortcut(event.key.keysym.scancode, false);

            break;

        default:
            break;
        }
    }

    return true;
}

std::shared_ptr<SnesController> FrontendSdl2::getController1()
{
    return m_Controller1;
}

void FrontendSdl2::drawPoint(int x, int y, const Color& c)
{
    SDL_SetRenderDrawColor(m_Renderer, c.r, c.g, c.b, 255);
    SDL_RenderDrawPoint(m_Renderer, x, y);
}

void FrontendSdl2::present()
{
    SDL_RenderPresent(m_Renderer);
}

void FrontendSdl2::setSnes(const std::shared_ptr<Snes>& snes)
{
    m_Snes = snes;
}

bool FrontendSdl2::handleShortcut(
    SDL_Scancode scancode,
    bool pressed)
{
    switch (scancode) {
    case SDL_SCANCODE_P:
        if (pressed) {
            m_Snes->toggleRunning();
        }
        break;

    default:
        break;
    }

    return true;
}
