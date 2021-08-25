#include <unordered_map>

#include <msfce/core/log.h>

#include "controller_sdl2.h"

#define TAG "ControllerSdl2"

namespace {

struct SnesControllerMapping {
    const char* name;
    off64_t offset;
};

// clang-format off
static const std::unordered_map<SDL_Scancode, SnesControllerMapping> s_ControllerMapping = {
    { SDL_SCANCODE_UP,    { "Up",    offsetof(msfce::core::Controller, up) } },
    { SDL_SCANCODE_DOWN,  { "Down",  offsetof(msfce::core::Controller, down) } },
    { SDL_SCANCODE_LEFT,  { "Left",  offsetof(msfce::core::Controller, left) } },
    { SDL_SCANCODE_RIGHT, { "Right", offsetof(msfce::core::Controller, right) } },

    { SDL_SCANCODE_RETURN, { "Start",  offsetof(msfce::core::Controller, start) } },
    { SDL_SCANCODE_RSHIFT, { "Select", offsetof(msfce::core::Controller, select) } },

    { SDL_SCANCODE_Q, { "L", offsetof(msfce::core::Controller, l) } },
    { SDL_SCANCODE_W, { "R", offsetof(msfce::core::Controller, r) } },

    { SDL_SCANCODE_A, { "Y", offsetof(msfce::core::Controller, y) } },
    { SDL_SCANCODE_S, { "X", offsetof(msfce::core::Controller, x) } },
    { SDL_SCANCODE_Z, { "B", offsetof(msfce::core::Controller, b) } },
    { SDL_SCANCODE_X, { "A", offsetof(msfce::core::Controller, a) } },
};

static const std::unordered_map<SDL_GameControllerButton, SnesControllerMapping> s_JoystickMapping = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP,    { "Up",    offsetof(msfce::core::Controller, up) } },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN,  { "Down",  offsetof(msfce::core::Controller, down) } },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT,  { "Left",  offsetof(msfce::core::Controller, left) } },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, { "Right", offsetof(msfce::core::Controller, right) } },

    { SDL_CONTROLLER_BUTTON_START, { "Start",  offsetof(msfce::core::Controller, start) } },
    { SDL_CONTROLLER_BUTTON_BACK, { "Select", offsetof(msfce::core::Controller, select) } },

    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, { "L", offsetof(msfce::core::Controller, l) } },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, { "R", offsetof(msfce::core::Controller, r) } },

    { SDL_CONTROLLER_BUTTON_Y, { "Y", offsetof(msfce::core::Controller, y) } },
    { SDL_CONTROLLER_BUTTON_X, { "X", offsetof(msfce::core::Controller, x) } },
    { SDL_CONTROLLER_BUTTON_B, { "B", offsetof(msfce::core::Controller, b) } },
    { SDL_CONTROLLER_BUTTON_A, { "A", offsetof(msfce::core::Controller, a) } },
};

static const std::unordered_map<uint8_t, SnesControllerMapping> s_HatMapping = {
    { SDL_HAT_UP,    { "Up",    offsetof(msfce::core::Controller, up) } },
    { SDL_HAT_DOWN,  { "Down",  offsetof(msfce::core::Controller, down) } },
    { SDL_HAT_LEFT,  { "Left",  offsetof(msfce::core::Controller, left) } },
    { SDL_HAT_RIGHT, { "Right", offsetof(msfce::core::Controller, right) } },
};

// clang-format on

bool* controllerGetButton(
    msfce::core::Controller* controller,
    const SnesControllerMapping& mapping)
{
    auto rawPtr = reinterpret_cast<uint8_t*>(controller) + mapping.offset;
    return reinterpret_cast<bool*>(rawPtr);
}

} // anonymous namespace

bool handleContollerKey(
    msfce::core::Controller* controller,
    SDL_Scancode scancode,
    bool pressed)
{
    LOGD(TAG, "Scancode %d %s", scancode, pressed ? "pressed" : "released");

    auto it = s_ControllerMapping.find(scancode);
    if (it == s_ControllerMapping.end()) {
        return false;
    }

    const auto& mapping = it->second;

    LOGD(
        TAG,
        "Button %s (scancode %d) %s",
        mapping.name,
        scancode,
        pressed ? "pressed" : "released");

    auto buttonValue = controllerGetButton(controller, mapping);
    *buttonValue = pressed;

    return true;
}

bool handleJoystickKey(
    msfce::core::Controller* controller,
    SDL_GameControllerButton button,
    bool pressed)
{
    LOGD(
        TAG, "Joystick button %d %s", button, pressed ? "pressed" : "released");

    auto it = s_JoystickMapping.find(button);
    if (it == s_JoystickMapping.end()) {
        return false;
    }

    const auto& mapping = it->second;

    LOGD(
        TAG,
        "Joystick button %s (button %d) %s",
        mapping.name,
        button,
        pressed ? "pressed" : "released");

    auto buttonValue = controllerGetButton(controller, mapping);
    *buttonValue = pressed;

    return true;
}

bool handleHatMotion(
    msfce::core::Controller* controller,
    uint8_t hat,
    uint8_t value)
{
    for (auto& it : s_HatMapping) {
        auto buttonValue = controllerGetButton(controller, it.second);
        *buttonValue = value & it.first;
    }

    return true;
}
