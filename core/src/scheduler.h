#pragma once

namespace msfce::core {

class SchedulerTask;

class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual void resumeTask(SchedulerTask* task, int cycles) = 0;
};

} // namespace msfce::core
