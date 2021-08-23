#pragma once

#include <memory>
#include <mutex>

#include <epoxy/gl.h>
#include <SDL.h>

#include "controller.h"
#include "registers.h"
#include "snes_renderer.h"

#include "frontend.h"

class Recorder;
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

    void playAudioSamples(const uint8_t* data, size_t sampleCount) final;

private:
    bool handleShortcut(
        SDL_Scancode scancode,
        bool pressed);

    std::string getSavestateName() const;

    int glInitContext();
    void glSetViewport();

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

    SDL_Joystick* m_Joystick = nullptr;

    std::shared_ptr<Snes> m_Snes;
    SnesController m_Controller1;

    // Scheduling
    bool m_Running = true;
    bool m_SpeedUp = false;

    // OpenGL
    bool m_FirstFrame = true;
    GLuint m_Shader = 0;
    GLint m_ScaleMatrixUniform = -1;
    GLint positionLoc = -1;
    GLint texCoordLoc = -1;
    GLuint m_Texture = 0;

    static constexpr int kTextureSize = kPpuDisplayWidth * kPpuDisplayHeight * 3;
    GLubyte m_TextureData[kTextureSize];
    GLubyte* m_TextureDataWriter = nullptr;

    // Audio
    std::mutex m_AudioSamplesMutex;
    static constexpr int kAudioSamplesSize = 32000 * 2 * 2; // 1 s
    uint8_t m_AudioSamples[kAudioSamplesSize];
    size_t m_AudioSamplesUsed = 0;

    // Recorder
    std::shared_ptr<Recorder> m_Recorder;
};
