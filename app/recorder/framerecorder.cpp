#include "framerecorder.h"

namespace msfce::recorder {

FrameRecorder::FrameRecorder(std::unique_ptr<FrameRecorderBackend> backend)
    : m_Backend(std::move(backend))
{
}

void FrameRecorder::pushFrame(const std::shared_ptr<Frame>& frame)
{
    std::unique_lock<std::mutex> lock(m_Mtx);

    m_Queue.push(frame);
    m_Cv.notify_one();
}

std::shared_ptr<Frame> FrameRecorder::popFrame()
{
    std::unique_lock<std::mutex> lock(m_Mtx);

    m_Cv.wait(lock, [this]() { return !m_Queue.empty(); });

    auto frame = m_Queue.front();
    m_Queue.pop();

    return frame;
}

void FrameRecorder::start()
{
    m_State = State::started;
    m_Thread = std::thread(&FrameRecorder::threadEntry, this);
}

void FrameRecorder::stop()
{
    if (m_Thread.joinable()) {
        pushFrame(nullptr);
        m_Thread.join();
    }

    m_State = State::stopped;
}

bool FrameRecorder::waitForStop() const
{
    return m_State == State::stopPending;
}

void FrameRecorder::threadEntry()
{
    m_Backend->start();

    while (auto frame = popFrame()) {
        bool ret = m_Backend->onFrameReceived(frame);
        if (!ret) {
            break;
        }
    }

    m_Backend->stop();
    m_State = State::stopPending;
}

} // namespace msfce::recorder
