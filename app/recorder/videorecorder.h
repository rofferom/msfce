#pragma once

#include <memory>
#include <string>

#include <msfce/core/snes.h>

#include "framerecorder.h"

struct AVAudioFifo;
struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct AVStream;
struct SwrContext;
struct SwsContext;

namespace msfce::recorder {

class VideoRecorder : public FrameRecorderBackend {
public:
    VideoRecorder(const std::string& basename, const msfce::core::SnesConfig& snesConfig);

    int start() final;
    int stop() final;
    int getAudioFrameSize() const;

    bool onFrameReceived(const std::shared_ptr<Frame>& inputFrame) final;

private:
    int initVideo();
    int clearVideo();

    bool onVideoFrameReceived(const std::shared_ptr<Frame>& inputFrame);

    int initAudio();
    int clearAudio();

    bool onAudioFrameReceived(const std::shared_ptr<Frame>& inputFrame);
    int encodeAudioFrame(AVFrame* avFrameSnes);

private:
    const msfce::core::SnesConfig m_SnesConfig;
    std::string m_Basename;

    AVFormatContext* m_ContainerCtx = nullptr;

    // Video
    AVCodecContext* m_VideoCodecCtx = nullptr;
    AVStream* m_VideoStream = nullptr;
    SwsContext* m_VideoSwsCtx = nullptr;
    int m_VideoFrameIdx = 0;

    // Audio
    AVCodecContext* m_AudioCodecCtx = nullptr;
    AVStream* m_AudioStream = nullptr;
    AVAudioFifo* m_AudioFifo = nullptr;
    SwrContext* m_AudioSwrCtx = nullptr;
    int m_AudioFrameIdx = 0;
    int m_AudioInSamples = 0;
    int m_AudioOutSamples = 0;
    int m_AudioSnesFrameSize = 0;
};

} // namespace msfce::recorder
