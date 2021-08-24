#pragma once

class SchedulerTask;

class Scheduler {
public:
    virtual ~Scheduler() = default;

    virtual void resumeTask(SchedulerTask* task, int cycles) = 0;
};
