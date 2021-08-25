#include "schedulertask.h"

namespace msfce::core {

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

void SchedulerTask::dumpToFile(FILE* f)
{
    fwrite(&m_State, sizeof(m_State), 1, f);
    fwrite(&m_NextRunCycle, sizeof(m_NextRunCycle), 1, f);
}

void SchedulerTask::loadFromFile(FILE* f)
{
    fread(&m_State, sizeof(m_State), 1, f);
    fread(&m_NextRunCycle, sizeof(m_NextRunCycle), 1, f);
}

} // namespace msfce::core
