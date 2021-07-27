#pragma once

#include <memory>

#include <SDL.h>

#include "controller.h"
#include "snes_renderer.h"

#include "frontend.h"

class SnesController;

class FrontendSdl2 : public Frontend, public SnesRenderer {
public:
    FrontendSdl2();
    ~FrontendSdl2();

    /// Frontend methods
    int init() final;
    int run() final;

    void setSnes(const std::shared_ptr<Snes>& snes) final;

    // SnesRenderer methods
    void scanStarted() final;
    void drawPixel(const SnesColor& c) final;
    void scanEnded() final;

private:
    bool handleShortcut(
        SDL_Scancode scancode,
        bool pressed);

    std::string getSavestateName() const;

private:
    SDL_Window* m_Window = nullptr;

    SDL_Surface* m_Surface = nullptr;

    uint8_t* m_SurfaceData = nullptr;

    std::shared_ptr<Snes> m_Snes;
    SnesController m_Controller1;

    // Scheduling
    bool m_Running = true;
    bool m_SpeedUp = false;
};
