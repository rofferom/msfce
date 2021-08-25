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

struct AVCodecContext;
struct AVFormatContext;
struct AVAudioFifo;
struct AVFrame;
struct AVStream;
struct SwrContext;
struct SwsContext;

class Recorder : public msfce::core::Renderer {
public:
    Recorder(const msfce::core::SnesConfig& snesConfig, const std::string& basename);
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
    enum class FrameType {
        video,
        audio,
    };

    struct Frame {
        FrameType type;
        std::vector<uint8_t> payload;
        int sampleCount = 0;

        Frame(FrameType type, size_t payloadSize)
            : type(type),
              payload(payloadSize)
        {
        }
    };

    class FrameRecorderBackend {
    public:
        virtual ~FrameRecorderBackend() = default;

        virtual int start() = 0;
        virtual int stop() = 0;

        virtual bool onFrameReceived(const std::shared_ptr<Frame>& frame) = 0;
    };

    class FrameRecorder {
    public:
        FrameRecorder(std::unique_ptr<FrameRecorderBackend> backend);

        void pushFrame(const std::shared_ptr<Frame>& frame);

        void start();
        void stop();

        bool waitForStop() const;

    private:
        std::shared_ptr<Frame> popFrame();

        void threadEntry();

    private:
        enum class State {
            idle,
            started,
            stopPending,
            stopped,
        };

    private:
        State m_State = State::idle;

        std::queue<std::shared_ptr<Frame>> m_Queue;
        std::mutex m_Mtx;
        std::condition_variable m_Cv;

        std::unique_ptr<FrameRecorderBackend> m_Backend;

        std::thread m_Thread;
    };

    class ImageRecorder : public FrameRecorderBackend {
    public:
        ImageRecorder(const std::string& basename, const msfce::core::SnesConfig& snesConfig);

        int start() final;
        int stop() final;

        bool onFrameReceived(const std::shared_ptr<Frame>& inputFrame) final;

    private:
        std::string m_Basename;
        const msfce::core::SnesConfig m_SnesConfig;
    };

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
