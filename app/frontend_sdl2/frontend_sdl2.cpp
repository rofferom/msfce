#include <assert.h>

#include <chrono>
#include <thread>

#include <msfce/core/snes.h>
#include <msfce/core/log.h>

#include "recorder/recorder.h"
#include "controller_sdl2.h"
#include "frontend_sdl2.h"

#define TAG "FrontendSdl2"

namespace {

constexpr int kWindowInitialScale = 2;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr int kSpeedupFrameSkip = 3; // x4 (skip 3 frames)

} // anonymous namespace

FrontendSdl2::FrontendSdl2() : Frontend(), msfce::core::Renderer()
{
}

FrontendSdl2::~FrontendSdl2()
{
    m_GlRenderer.reset();
    clearRecorder();
    SDL_Quit();
}

int FrontendSdl2::init(const std::shared_ptr<msfce::core::Snes>& snes)
{
    m_Snes = snes;
    m_SnesConfig = m_Snes->getConfig();

    // Init SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

    // Init joystick
    SDL_JoystickEventState(SDL_ENABLE);

    // Init video
    m_WindowWidth = m_SnesConfig.displayWidth * kWindowInitialScale;
    m_WindowHeight = m_SnesConfig.displayHeight * kWindowInitialScale;

    m_Window = SDL_CreateWindow(
        "Monkey Super Famicom Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_WindowWidth,
        m_WindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    assert(m_Window);

    // Init OpenGl context
    m_GlContext = SDL_GL_CreateContext(m_Window);
    assert(m_GlContext);

    m_GlRenderer = std::make_unique<RendererGl>(m_SnesConfig);
    m_GlRenderer->initContext();
    m_GlRenderer->setWindowSize(m_WindowWidth, m_WindowHeight);

    // Init audio
    SDL_AudioSpec spec;
    SDL_memset(&spec, 0, sizeof(spec));
    spec.freq = m_SnesConfig.audioSampleRate;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 512; // 16 ms

    auto cb = [](void* userdata, Uint8* stream, int len) {
        FrontendSdl2* self = reinterpret_cast<FrontendSdl2*>(userdata);
        self->onSdlPlayCb(stream, len);
    };

    spec.callback = cb;
    spec.userdata = this;

    SDL_AudioDeviceID deviceId = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    SDL_PauseAudioDevice(deviceId, 0);

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
            case SDL_WINDOWEVENT: {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                    if (m_WindowWidth != event.window.data1) {
                        m_WindowWidth = event.window.data1;

                        m_GlRenderer->setWindowSize(
                            m_WindowWidth, m_WindowHeight);
                    }

                    if (m_WindowHeight != event.window.data2) {
                        m_WindowHeight = event.window.data2;

                        m_GlRenderer->setWindowSize(
                            m_WindowWidth, m_WindowHeight);
                    }
                    break;

                default:
                    break;
                }

                break;
            }

            case SDL_KEYDOWN:
                keyHandled = handleContollerKey(
                    &m_Controller1, event.key.keysym.scancode, true);
                if (keyHandled) {
                    break;
                }

                if (event.key.repeat) {
                    break;
                }

                handleShortcut(event.key.keysym.scancode, true);
                break;

            case SDL_KEYUP:
                keyHandled = handleContollerKey(
                    &m_Controller1, event.key.keysym.scancode, false);
                if (keyHandled) {
                    break;
                }

                if (event.key.repeat) {
                    break;
                }

                handleShortcut(event.key.keysym.scancode, false);
                break;

            case SDL_JOYDEVICEADDED:
                onJoystickAdded(event.jdevice.which);
                break;

            case SDL_JOYDEVICEREMOVED:
                onJoystickRemoved(event.jdevice.which);
                break;

            case SDL_JOYHATMOTION: {
                handleHatMotion(
                    &m_Controller1, event.jhat.hat, event.jhat.value);
                break;
            }

            case SDL_JOYBUTTONDOWN:
                handleJoystickKey(
                    &m_Controller1,
                    static_cast<SDL_GameControllerButton>(event.jbutton.button),
                    true);
                break;

            case SDL_JOYBUTTONUP:
                handleJoystickKey(
                    &m_Controller1,
                    static_cast<SDL_GameControllerButton>(event.jbutton.button),
                    false);
                break;

            default:
                break;
            }
        }

        checkRecorder();

        if (m_Running) {
            m_Snes->setController1(m_Controller1);

            if (m_SpeedUp) {
                for (int i = 0; i < kSpeedupFrameSkip; i++) {
                    m_Snes->renderSingleFrame(false);
                }
            }

            // Render texture
            m_TextureData = m_GlRenderer->bindBackbuffer();
            m_Snes->renderSingleFrame();
            m_GlRenderer->unbindBackbuffer();
        }

        // Render screen
        m_GlRenderer->render();

        std::this_thread::sleep_until(presentTp);
        SDL_GL_SwapWindow(m_Window);
        presentTp += kRenderPeriod;
    }

    return true;
}

void FrontendSdl2::scanStarted()
{
}

void FrontendSdl2::drawPixel(const msfce::core::Color& c)
{
    m_TextureData[0] = c.r;
    m_TextureData[1] = c.g;
    m_TextureData[2] = c.b;

    m_TextureData += 3;
}

void FrontendSdl2::scanEnded()
{
}

bool FrontendSdl2::handleShortcut(SDL_Scancode scancode, bool pressed)
{
    switch (scancode) {
    case SDL_SCANCODE_O:
        if (pressed) {
            if (!m_Recorder) {
                initRecorder();
            }

            assert(m_Recorder);
            m_Recorder->toggleVideoRecord();
        }

        break;

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

    case SDL_SCANCODE_F:
        if (pressed) {
            m_Fullscreen = !m_Fullscreen;

            if (m_Fullscreen) {
                SDL_SetWindowFullscreen(
                    m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                SDL_ShowCursor(SDL_DISABLE);
            } else {
                SDL_SetWindowFullscreen(m_Window, 0);
                SDL_ShowCursor(SDL_ENABLE);
            }
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
            m_AudioSamplesUsed = 0;
            m_Snes->loadState(getSavestateName());
        }
        break;
    }

    case SDL_SCANCODE_F8: {
        if (pressed) {
            if (!m_Recorder) {
                initRecorder();
            }

            assert(m_Recorder);
            m_Recorder->takeScreenshot();
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
    return m_Snes->getRomBasename() + ".msfce";
}

void FrontendSdl2::onJoystickAdded(int index)
{
    if (m_Joystick) {
        return;
    }

    LOGI(TAG, "Opening joystick '%s'", SDL_JoystickNameForIndex(index));
    m_Joystick = SDL_JoystickOpen(index);
}

void FrontendSdl2::onJoystickRemoved(int index)
{
    if (!m_Joystick) {
        return;
    }

    if (index != SDL_JoystickInstanceID(m_Joystick)) {
        return;
    }

    LOGI(TAG, "Closing joystick '%s'", SDL_JoystickName(m_Joystick));
    SDL_JoystickClose(m_Joystick);
    m_Joystick = nullptr;
}

void FrontendSdl2::initRecorder()
{
    m_Recorder = std::make_shared<msfce::recorder::Recorder>(
        m_SnesConfig, m_Snes->getRomBasename());

    m_Snes->addRenderer(m_Recorder);
}

void FrontendSdl2::clearRecorder()
{
    if (m_Snes) {
        m_Snes->removeRenderer(m_Recorder);
    }

    m_Recorder.reset();
}

void FrontendSdl2::checkRecorder()
{
    if (!m_Recorder) {
        return;
    }

    if (m_Recorder->active()) {
        return;
    }

    clearRecorder();
}

void FrontendSdl2::playAudioSamples(const uint8_t* data, size_t sampleCount)
{
    std::unique_lock<std::mutex> lock(m_AudioSamplesMutex);

    const size_t sampleSize = sampleCount * m_SnesConfig.audioSampleSize;

    if (kAudioSamplesSize >= m_AudioSamplesUsed + sampleSize) {
        memcpy(m_AudioSamples + m_AudioSamplesUsed, data, sampleSize);
        m_AudioSamplesUsed += sampleSize;
    }
}

void FrontendSdl2::onSdlPlayCb(Uint8* stream, int len)
{
    std::unique_lock<std::mutex> lock(m_AudioSamplesMutex);

    if ((int)m_AudioSamplesUsed >= len) {
        memcpy(stream, m_AudioSamples, len);
        memmove(m_AudioSamples, &m_AudioSamples[len], m_AudioSamplesUsed - len);
        m_AudioSamplesUsed -= len;
    } else {
        memset(stream, 0, len);
    }
}
