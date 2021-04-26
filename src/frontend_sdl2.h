#pragma once

#include <memory>

#include <SDL.h>

#include "frontend.h"

class SnesController;

class FrontendSdl2 : public Frontend {
public:
    FrontendSdl2();
    ~FrontendSdl2();

    int init() final;
    bool runOnce() final;

    std::shared_ptr<SnesController> getController1() final;

    void drawPoint(int x, int y, const Color& c) final;
    void present() final;

    void setSnes(const std::shared_ptr<Snes>& snes) final;

private:
    bool handleShortcut(
        SDL_Scancode scancode,
        bool pressed);

private:
    SDL_Window* m_Window = nullptr;
    SDL_Renderer* m_Renderer = nullptr;

    std::shared_ptr<SnesController> m_Controller1;

    std::shared_ptr<Snes> m_Snes;
};
