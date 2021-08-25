#pragma once

#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>
#include <thread>

namespace msfce::recorder {

enum class FrameType {
    video,
    audio,
};

struct Frame {
    FrameType type;
    std::vector<uint8_t> payload;
    int sampleCount = 0;

    Frame(FrameType type, size_t payloadSize) : type(type), payload(payloadSize)
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

} // namespace msfce::recorder
