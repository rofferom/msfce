#pragma once

#include <memory>
#include <mutex>

#include <epoxy/gl.h>
#include <SDL.h>

#include "renderer_gl.h"
#include "frontend.h"

namespace msfce::recorder {

class Recorder;

} // namespace msfce::recorder

class FrontendSdl2 : public Frontend, public msfce::core::Renderer {
public:
    FrontendSdl2();
    ~FrontendSdl2();

    /// Frontend methods
    int init(const std::shared_ptr<msfce::core::Snes>& snes) final;
    int run() final;

    // msfce::core::Renderer methods
    void scanStarted() final;
    void drawPixel(const msfce::core::Color& c) final;
    void scanEnded() final;

    void playAudioSamples(const uint8_t* data, size_t sampleCount) final;

private:
    bool handleShortcut(
        SDL_Scancode scancode,
        bool pressed);

    std::string getSavestateName() const;

    void onJoystickAdded(int index);
    void onJoystickRemoved(int index);

    void initRecorder();
    void clearRecorder();
    void checkRecorder();

private:
    void onSdlPlayCb(Uint8* stream, int len);

private:
    SDL_Window* m_Window = nullptr;
    int m_WindowWidth;
    int m_WindowHeight;
    bool m_Fullscreen = false;

    SDL_GLContext m_GlContext = nullptr;
    std::unique_ptr<RendererGl> m_GlRenderer;
    GLubyte* m_TextureData = nullptr;

    SDL_Joystick* m_Joystick = nullptr;

    std::shared_ptr<msfce::core::Snes> m_Snes;
    msfce::core::SnesConfig m_SnesConfig;
    msfce::core::Controller m_Controller1;

    // Scheduling
    bool m_Running = true;
    bool m_SpeedUp = false;

    // Audio
    std::mutex m_AudioSamplesMutex;
    static constexpr int kAudioSamplesSize = 32000 * 2 * 2; // 1 s
    uint8_t m_AudioSamples[kAudioSamplesSize];
    size_t m_AudioSamplesUsed = 0;

    // Recorder
    std::shared_ptr<msfce::recorder::Recorder> m_Recorder;
};
