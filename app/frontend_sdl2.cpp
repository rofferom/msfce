#include <assert.h>

#include <chrono>
#include <thread>
#include <unordered_map>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <msfce/core/snes.h>
#include <msfce/core/log.h>
#include "recorder/recorder.h"
#include "frontend_sdl2.h"

#define TAG "FrontendSdl2"

namespace {

#define SIZEOF_ARRAY(x)  (sizeof(x) / sizeof((x)[0]))

constexpr int kWindowInitialScale = 2;

constexpr auto kRenderPeriod = std::chrono::microseconds(16666);
constexpr int kSpeedupFrameSkip = 3; // x4 (skip 3 frames)

const char *vertexShader =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;"
    "layout (location = 1) in vec2 aTexCoord;"

    "uniform mat4 scaleMatrix;"
    "out vec2 TexCoord;"

    "void main()"
    "{"
    "    gl_Position = scaleMatrix * vec4(aPos, 1.0);"
    "    TexCoord = vec2(aTexCoord.x, aTexCoord.y);"
    "}";

const char *fragmentShader =
    "#version 330 core\n"
    "out vec4 FragColor;"

    "in vec3 ourColor;"
    "in vec2 TexCoord;"

    "uniform sampler2D texture1;"

    "void main()"
    "{"
    "    FragColor = texture(texture1, TexCoord);"
    "}";

struct SnesControllerMapping {
    const char* name;
    off64_t offset;
};

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

bool* controllerGetButton(
    msfce::core::Controller* controller,
    const SnesControllerMapping& mapping)
{
    auto rawPtr = reinterpret_cast<uint8_t *>(controller) + mapping.offset;
    return reinterpret_cast<bool *>(rawPtr);
}

bool handleContollerKey(
    msfce::core::Controller* controller,
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

bool handleJoystickKey(
    msfce::core::Controller* controller,
    SDL_GameControllerButton button,
    bool pressed)
{
    LOGD(TAG, "Joystick button %d %s",
        button,
        pressed ? "pressed" : "released");

    auto it = s_JoystickMapping.find(button);
    if (it == s_JoystickMapping.end()) {
        return false;
    }

    const auto& mapping = it->second;

    LOGD(TAG, "Joystick button %s (button %d) %s",
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
    for (auto& it: s_HatMapping) {
        auto buttonValue = controllerGetButton(controller, it.second);
        *buttonValue = value & it.first;
    }

    return true;
}

void checkCompileErrors(GLuint shader, const std::string& type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            LOGE(TAG, "Shader compilation error of type: '%s': '%s'", type.c_str(), infoLog);
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            LOGE(TAG, "Shader link error of type: '%s'", infoLog);
        }
    }
}

GLuint compileShader(const char* vShaderCode, const char* fShaderCode)
{
    GLuint vertex, fragment;

    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    // fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    // shader Program
    GLuint ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // delete the shaders as they're linked into our program now and no longer necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return ID;
}

} // anonymous namespace

FrontendSdl2::FrontendSdl2()
    : Frontend(),
      msfce::core::Renderer()
{
}

FrontendSdl2::~FrontendSdl2()
{
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
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        m_WindowWidth, m_WindowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    assert(m_Window);

    m_GlContext = SDL_GL_CreateContext(m_Window);
    assert(m_GlContext);

    glInitContext();
    glSetViewport();

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
        bool windowSizeUpdated = false;

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
                        windowSizeUpdated = true;
                    }

                    if (m_WindowHeight != event.window.data2) {
                        m_WindowHeight = event.window.data2;
                        windowSizeUpdated = true;
                    }
                    break;

                default:
                    break;
                }

                break;
            }

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

            case SDL_JOYDEVICEADDED:
                onJoystickAdded(event.jdevice.which);
                break;

            case SDL_JOYDEVICEREMOVED:
                onJoystickRemoved(event.jdevice.which);
                break;

            case SDL_JOYHATMOTION: {
                handleHatMotion(
                    &m_Controller1,
                    event.jhat.hat,
                    event.jhat.value);
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
            glBindTexture(GL_TEXTURE_2D, m_Texture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_PBO);

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 224, GL_RGB, GL_UNSIGNED_BYTE, 0);

            glBufferData(GL_PIXEL_UNPACK_BUFFER, m_TextureSize, 0, GL_STREAM_DRAW);
            m_TextureData = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

            m_Snes->renderSingleFrame();

            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Render screen
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (windowSizeUpdated) {
            glSetViewport();
        }

        glBindTexture(GL_TEXTURE_2D, m_Texture);

        glUseProgram(m_Shader);
        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES, m_VAO_ElemSize, GL_UNSIGNED_INT, 0);

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

bool FrontendSdl2::handleShortcut(
    SDL_Scancode scancode,
    bool pressed)
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
                SDL_SetWindowFullscreen(m_Window, SDL_WINDOW_FULLSCREEN_DESKTOP);
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

int FrontendSdl2::glInitContext()
{
    m_Shader = compileShader(vertexShader, fragmentShader);
    m_ScaleMatrixUniform = glGetUniformLocation(m_Shader, "scaleMatrix");

    // Create VAO
    static const float vertices[] = {
         // Coords            // Texture
         1.0f,  1.0f, 0.0f,   1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,   0.0f, 0.0f
    };

    static const GLuint indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    GLuint VBO, EBO;
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    m_VAO_ElemSize = SIZEOF_ARRAY(indices);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create texture
    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_TextureSize = m_SnesConfig.displayWidth * m_SnesConfig.displayHeight * 3;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        m_SnesConfig.displayWidth,
        m_SnesConfig.displayHeight,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Create PBO

    glGenBuffers(1, &m_PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_TextureSize, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 0;
}

void FrontendSdl2::glSetViewport()
{
    glm::mat4 m;

    const float windowRatio = (float) m_WindowWidth / (float) m_WindowHeight;
    const float ppuRatio = (float) m_SnesConfig.displayWidth / (float) m_SnesConfig.displayHeight;

    if (windowRatio > ppuRatio) {
        // Window is wider than expected
        const float displayedWidth = (float) m_SnesConfig.displayWidth * ((float) m_WindowHeight / (float) m_SnesConfig.displayHeight);
        const float widthRatio = displayedWidth / (float) m_WindowWidth;
        m = glm::scale(glm::vec3(widthRatio, 1.0f, 1));
    } else {
        // Window is higher than expected
        const float displayedHeight = (float) m_SnesConfig.displayHeight * ((float) m_WindowWidth / (float) m_SnesConfig.displayWidth);
        const float heightRatio = (float) displayedHeight / (float) m_WindowHeight;
        m = glm::scale(glm::vec3(1.0f, heightRatio, 1));
    }

    glUseProgram(m_Shader);
    glUniformMatrix4fv(m_ScaleMatrixUniform, 1, GL_FALSE, glm::value_ptr(m));
    glUseProgram(0);

    glViewport(0, 0, m_WindowWidth, m_WindowHeight);
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
        m_SnesConfig,
        m_Snes->getRomBasename());

    m_Snes->addRenderer(m_Recorder);
}

void FrontendSdl2::clearRecorder()
{
    m_Snes->removeRenderer(m_Recorder);
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

    if ((int) m_AudioSamplesUsed >= len) {
        memcpy(stream, m_AudioSamples, len);
        memmove(m_AudioSamples, &m_AudioSamples[len], m_AudioSamplesUsed - len);
        m_AudioSamplesUsed -= len;
    } else {
        memset(stream, 0, len);
    }
}

