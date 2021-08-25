#pragma once

#include <SDL.h>

#include <msfce/core/controller.h>

bool handleContollerKey(
    msfce::core::Controller* controller,
    SDL_Scancode scancode,
    bool pressed);

bool handleJoystickKey(
    msfce::core::Controller* controller,
    SDL_GameControllerButton button,
    bool pressed);

bool handleHatMotion(
    msfce::core::Controller* controller,
    uint8_t hat,
    uint8_t value);
