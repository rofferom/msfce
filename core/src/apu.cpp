#include <assert.h>

#include "msfce/core/log.h"
#include "registers.h"
#include "apu.h"

#define TAG "apu"

namespace {

const uint8_t kIplRom[64] = {0xcd, 0xef, 0xbd, 0xe8, 0x00, 0xc6, 0x1d, 0xd0,
                             0xfc, 0x8f, 0xaa, 0xf4, 0x8f, 0xbb, 0xf5, 0x78,
                             0xcc, 0xf4, 0xd0, 0xfb, 0x2f, 0x19, 0xeb, 0xf4,
                             0xd0, 0xfc, 0x7e, 0xf4, 0xd0, 0x0b, 0xe4, 0xf5,
                             0xcb, 0xf4, 0xd7, 0x00, 0xfc, 0xd0, 0xf3, 0xab,
                             0x01, 0x10, 0xef, 0x7e, 0xf4, 0x10, 0xeb, 0xba,
                             0xf6, 0xda, 0x00, 0xba, 0xf4, 0xc4, 0xf4, 0xdd,
                             0x5d, 0xd0, 0xdb, 0x1f, 0x00, 0x00, 0xc0, 0xff};

const int kIplRomSize = SIZEOF_ARRAY(kIplRom);

// FIXME: NTSC ratio
constexpr float kClockRatio = 1024000.0f / 21477000.0f;

} // anonymous namespace

namespace msfce::core {

Apu::Apu(const uint64_t& masterClock, RenderSampleCb renderSampleCb)
    : MemComponent(MemComponentType::apu)
    , SchedulerTask()
    , m_RenderSampleCb(renderSampleCb)
    , m_MasterClock(masterClock)
{
    // Init SPC
    m_SPC.init();
    m_SPC.init_rom(kIplRom);
    m_SPC.reset();

    // Plug output buffer
    m_SamplesSize = kSampleSize * kSampleRate / 30; // 30 ms
    m_Samples = new uint8_t[m_SamplesSize];

    m_SPC.set_output(reinterpret_cast<short*>(m_Samples), m_SamplesSize);
}

Apu::~Apu()
{
    delete[] m_Samples;
}

uint8_t Apu::readU8(uint32_t addr)
{
    uint64_t t = kClockRatio * (m_MasterClock - m_Clock);
    return m_SPC.read_port(t, addr & 3);
}

void Apu::writeU8(uint32_t addr, uint8_t value)
{
    uint64_t t = kClockRatio * (m_MasterClock - m_Clock);
    m_SPC.write_port(t, addr & 3, value);
}

void Apu::dumpToFile(FILE* f)
{
    SchedulerTask::dumpToFile(f);

    // Dump SPC state
    uint8_t* spcState = new uint8_t[SNES_SPC::state_size];

    auto stateCb = [](unsigned char** io, void* state, size_t size) {
        memcpy(*io, state, size);
        (*io) += size;
    };

    void* io = spcState;
    m_SPC.copy_state(reinterpret_cast<unsigned char**>(&io), stateCb);

    // Write states to file
    fwrite(&m_Clock, sizeof(m_Clock), 1, f);
    fwrite(spcState, SNES_SPC::state_size, 1, f);

    delete[] spcState;
}

void Apu::loadFromFile(FILE* f)
{
    SchedulerTask::loadFromFile(f);

    uint8_t* spcState = new uint8_t[SNES_SPC::state_size];

    memset(m_Samples, 0, m_SamplesSize);
    m_SPC.set_output(reinterpret_cast<short*>(m_Samples), m_SamplesSize);

    // Load states
    fread(&m_Clock, sizeof(m_Clock), 1, f);
    fread(spcState, SNES_SPC::state_size, 1, f);

    // Restore state to SPC
    auto stateCb = [](unsigned char** io, void* state, size_t size) {
        memcpy(state, *io, size);
        (*io) += size;
    };

    void* io = spcState;
    m_SPC.copy_state(reinterpret_cast<unsigned char**>(&io), stateCb);

    delete[] spcState;
}

int Apu::run()
{
    // Complete frame
    uint64_t t = kClockRatio * (m_MasterClock - m_Clock);
    m_Clock = m_MasterClock;
    m_SPC.end_frame(t);

    // Record samples
    const int sampleCount = m_SPC.sample_count();
    if (sampleCount > 0) {
        // SNES_SPC seems to give a invalid sample count
        m_RenderSampleCb(m_Samples, sampleCount / 2);
        m_SPC.set_output(reinterpret_cast<short*>(m_Samples), m_SamplesSize);
    }

    return 0;
}

} // namespace msfce::core
