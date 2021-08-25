#include <assert.h>
#include <time.h>

#include <msfce/core/log.h>

#include "framerecorder.h"
#include "imagerecorder.h"
#include "videorecorder.h"
#include "recorder.h"

#define TAG "Recorder"
#include "recorder_utils.h"

namespace {

std::string getDate()
{
#ifdef __MINGW32__
    time_t t;
    struct tm* localTm;

    time(&t);
    localTm = localtime(&t);

    char s[128];
    strftime(s, sizeof(s), "%Y-%m-%d %H-%M-%S", localTm);

    return s;
#else
    time_t t;
    struct tm localTm;

    time(&t);

    localtime_r(&t, &localTm);

    char s[128];
    strftime(s, sizeof(s), "%F %T", &localTm);

    return s;
#endif
}

} // anonymous namespace

namespace msfce::recorder {

Recorder::Recorder(const msfce::core::SnesConfig& snesConfig, const std::string& basename)
    : m_SnesConfig(snesConfig),
      m_Basename(basename),
      m_ImgSize(m_SnesConfig.displayWidth * m_SnesConfig.displayHeight * kRgbSampleSize)
{
}

Recorder::~Recorder()
{
    if (m_ImageRecorder) {
        m_ImageRecorder->stop();
        m_ImageRecorder.reset();
    }

    if (m_VideoRecorder) {
        m_VideoRecorder->stop();
        m_VideoRecorder.reset();
    }

    LOGD(TAG, "Destroyed");
}

void Recorder::scanStarted()
{
    m_Started = true;

    m_BackBuffer = std::make_shared<Frame>(FrameType::video, m_ImgSize);
    m_BackBufferWritter = m_BackBuffer->payload.data();

    m_AudioFrame = std::make_shared<Frame>(FrameType::audio, m_AudioFrameMaxSize);
}

void Recorder::drawPixel(const msfce::core::Color& c)
{
    if (!m_Started) {
        return;
    }

    assert(m_BackBuffer);
    assert(m_BackBufferWritter);

    m_BackBufferWritter[0] = c.r;
    m_BackBufferWritter[1] = c.g;
    m_BackBufferWritter[2] = c.b;

    m_BackBufferWritter += 3;
}

void Recorder::scanEnded()
{
    if (!m_Started) {
        return;
    }

    m_VideoFrameReceived++;

    // HACK: Resync audio (audio is currently produced at a slower way)
    // Avoid to get more than 20 ms of delay
    const int64_t videoTsMs = m_VideoFrameReceived * 1000 / m_SnesConfig.displayRate;
    const int64_t audioTsMs = m_AudioSampleReceived * 1000 / m_SnesConfig.audioSampleRate;
    const int64_t audioDeltaMs = videoTsMs - audioTsMs;
    constexpr int64_t kAudioMaxDeltaMs = 20;

    if (audioDeltaMs >= kAudioMaxDeltaMs) {
        const int silenceSampleCount = m_SnesConfig.audioSampleRate * kAudioMaxDeltaMs / 1000;
        auto silence = std::vector<uint8_t>(silenceSampleCount * m_SnesConfig.audioSampleSize);
        playAudioSamples(silence.data(), silenceSampleCount);
    }

    // Push video frames
    if (m_VideoRecorder) {
        m_VideoRecorder->pushFrame(m_BackBuffer);
        m_VideoRecorder->pushFrame(m_AudioFrame);
    }

    if (m_ImageRecorder) {
        m_ImageRecorder->pushFrame(m_BackBuffer);
    }

    m_BackBuffer = nullptr;
    m_BackBufferWritter = nullptr;

    m_AudioFrame = nullptr;
}

void Recorder::playAudioSamples(const uint8_t* data, size_t sampleCount)
{
    if (!m_Started) {
        return;
    }

    const size_t payloadRequiredSize = (m_AudioFrame->sampleCount + sampleCount) * m_SnesConfig.audioSampleSize;
    if (payloadRequiredSize > m_AudioFrame->payload.size()) {
        m_AudioFrame->payload.resize(payloadRequiredSize);
    }

    uint8_t* payloadWrite = m_AudioFrame->payload.data() + m_AudioFrame->sampleCount * m_SnesConfig.audioSampleSize;
    memcpy(payloadWrite, data, sampleCount * m_SnesConfig.audioSampleSize);
    m_AudioFrame->sampleCount += sampleCount;

    m_AudioSampleReceived += sampleCount;
}

bool Recorder::active()
{
    bool ret = false;

    if (m_ImageRecorder) {
        if (m_ImageRecorder->waitForStop()) {
            m_ImageRecorder->stop();
            m_ImageRecorder.reset();
        } else {
            ret = true;
        }
    }

    if (m_VideoRecorder) {
        if (m_VideoRecorder->waitForStop()) {
            m_VideoRecorder->stop();
            m_VideoRecorder.reset();
        } else {
            ret = true;
        }
    }

    return ret;
}

void Recorder::toggleVideoRecord()
{
    LOGI(TAG, "Toggle video record");

    if (m_VideoRecorder) {
        m_VideoRecorder->stop();
        m_VideoRecorder.reset();
    } else {
        m_VideoRecorder = std::make_unique<FrameRecorder>(
            std::make_unique<VideoRecorder>(getTsBasename(), m_SnesConfig));
        m_VideoRecorder->start();
    }
}

void Recorder::takeScreenshot()
{
    LOGI(TAG, "Take screenshot");

    if (m_ImageRecorder) {
        if (!m_ImageRecorder->waitForStop()) {
            LOGW(TAG, "ImageRecorder busy");
            return;
        }

        m_ImageRecorder->stop();
        m_ImageRecorder.reset();
    }

    m_ImageRecorder = std::make_unique<FrameRecorder>(
        std::make_unique<ImageRecorder>(getTsBasename(), m_SnesConfig));
    m_ImageRecorder->start();
}

std::string Recorder::getTsBasename() const
{
    return m_Basename + " - " + getDate();
}

} // namespace msfce::recorder
