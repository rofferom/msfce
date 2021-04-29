#include "schedulertask.h"

SchedulerTask::State SchedulerTask::getState()
{
    return m_State;
}

void SchedulerTask::setIdle()
{
    m_State = State::idle;
}

void SchedulerTask::setNextRunCycle(uint64_t cycle)
{
    m_State = State::running;
    m_NextRunCycle = cycle;
}

uint64_t SchedulerTask::getNextRunCycle()
{
    return m_NextRunCycle;
}
