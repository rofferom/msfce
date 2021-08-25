#pragma once

#include <condition_variable>
#include <queue>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include <msfce/core/renderer.h>
#include <msfce/core/snes.h>

namespace msfce::recorder {

struct Frame;
struct FrameRecorder;

class Recorder : public msfce::core::Renderer {
public:
    Recorder(
        const msfce::core::SnesConfig& snesConfig,
        const std::string& basename);
    ~Recorder();

    // Draw API
    void scanStarted() final;
    void drawPixel(const msfce::core::Color& c) final;
    void scanEnded() final;

    void playAudioSamples(const uint8_t* data, size_t sampleCount);

    // Control API
    bool active();

    void toggleVideoRecord();
    void takeScreenshot();

private:
    std::string getTsBasename() const;

private:
    const msfce::core::SnesConfig m_SnesConfig;
    const std::string m_Basename;

    const int m_ImgSize;

    bool m_Started = false;

    // Video
    std::shared_ptr<Frame> m_BackBuffer;
    uint8_t* m_BackBufferWritter = nullptr;
    int m_VideoFrameReceived = 0;

    // Audio
    std::shared_ptr<Frame> m_AudioFrame;
    size_t m_AudioFrameMaxSize = 0;
    int m_AudioSampleReceived = 0;

    std::unique_ptr<FrameRecorder> m_ImageRecorder;
    std::unique_ptr<FrameRecorder> m_VideoRecorder;
};

} // namespace msfce::recorder
