#pragma once

#include <condition_variable>
#include <queue>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include "snes_renderer.h"

struct AVCodecContext;
struct AVStream;
struct AVFormatContext;
struct SwsContext;

class Recorder : public SnesRenderer {
public:
    Recorder(int width, int height, int framerate, const std::string& basename);
    ~Recorder();

    // Draw API
    void scanStarted() final;
    void drawPixel(const SnesColor& c) final;
    void scanEnded() final;

    // Control API
    bool active();

    void toggleVideoRecord();
    void takeScreenshot();

private:
    using Frame = std::vector<uint8_t>;

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
        ImageRecorder(const std::string& basename, int frameWidth, int frameHeight);

        int start() final;
        int stop() final;

        bool onFrameReceived(const std::shared_ptr<Frame>& frame) final;

    private:
        std::string m_Basename;
        const int m_FrameWidth;
        const int m_FrameHeight;
    };

    class VideoRecorder : public FrameRecorderBackend {
    public:
        VideoRecorder(const std::string& basename, int frameWidth, int frameHeight, int framerate);

        int start() final;
        int stop() final;

        bool onFrameReceived(const std::shared_ptr<Frame>& frame) final;

    private:
        std::string m_Basename;
        const int m_FrameWidth;
        const int m_FrameHeight;
        const int m_Framerate;

        AVCodecContext* m_CodecCtx = nullptr;
        AVStream* m_VideoStream = nullptr;
        AVFormatContext* m_FormatCtx = nullptr;
        SwsContext* m_SwsCtx = nullptr;
        int m_FrameIdx = 0;
    };

private:
    std::string getTsBasename() const;

private:
    const int m_FrameWidth;
    const int m_FrameHeight;
    const int m_Framerate;
    const std::string m_Basename;

    const int m_ImgSize;

    bool m_Started = false;
    std::shared_ptr<Frame> m_BackBuffer;
    uint8_t* m_BackBufferWritter = nullptr;

    std::unique_ptr<FrameRecorder> m_ImageRecorder;
    std::unique_ptr<FrameRecorder> m_VideoRecorder;
};
