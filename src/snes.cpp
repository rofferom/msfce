#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <sys/stat.h>
#include <filesystem>

#include "65816.h"
#include "apu.h"
#include "controller.h"
#include "dma.h"
#include "log.h"
#include "maths.h"
#include "membus.h"
#include "ppu.h"
#include "registers.h"
#include "sram.h"
#include "wram.h"

#include "snes.h"

#define TAG "SNES"

namespace {

constexpr uint32_t kLowRomHeaderBase = 0x7FB0;
constexpr uint32_t kHighRomHeaderBase = 0xFFB0;

constexpr uint32_t kHeaderOffset_Title = 0x10;
constexpr uint32_t kHeaderOffset_RomSpeedAndMode = 0x25;
constexpr uint32_t kHeaderOffset_RomRamInfo = 0x26;
constexpr uint32_t kHeaderOffset_SramSize = 0x28;

constexpr uint32_t kHeader_TitleSize = 21;

constexpr bool kLogTimings = true;

} // anonymous namespace

void Snes::DurationTool::reset()
{
    total_duration = Snes::Clock::duration::zero();
}

void Snes::DurationTool::begin()
{
    if (!kLogTimings) {
        return;
    }

    begin_tp = Clock::now();
}

void Snes::DurationTool::end()
{
    if (!kLogTimings) {
        return;
    }

    end_tp = Clock::now();
    total_duration += end_tp - begin_tp;
}

template <typename Duration>
int64_t Snes::DurationTool::total()
{
    if (!kLogTimings) {
        return 0;
    }

    return std::chrono::duration_cast<Duration>(total_duration).count();
}

Snes::Snes()
    : MemComponent(MemComponentType::irq),
      Scheduler(){
}

int Snes::addRenderer(const std::shared_ptr<SnesRenderer>& renderer)
{
    m_RendererList.push_back(renderer);

    return 0;
}

int Snes::removeRenderer(const std::shared_ptr<SnesRenderer>& renderer)
{
    auto it = std::find(m_RendererList.begin(), m_RendererList.end(), renderer);
    if (it == m_RendererList.end()) {
        return -ENOENT;
    }

    LOGD(TAG, "Renderer removed");
    m_RendererList.erase(it);

    return 0;
}

int Snes::plugCartidge(const char* path)
{
    uint8_t romRamInfo;
    uint8_t romSpeedAndMode;
    int ret;

    // Open file and get its size
    LOGI(TAG, "Loading '%s'", path);

    FILE* f = fopen(path, "rb");
    if (!f) {
        ret = -errno;
        LOG_ERRNO(TAG, "fopen");
        return ret;
    }

    struct stat fStat;
    ret = fstat(fileno(f), &fStat);
    if (ret == -1) {
        ret = -errno;
        LOG_ERRNO(TAG, "fstat");
        return ret;
    }

    LOGI(TAG, "Rom size: %lu bytes", fStat.st_size);

    // Read file content
    std::vector<uint8_t> rom(fStat.st_size);

    size_t readRet = fread(rom.data(), 1, fStat.st_size, f);
    if (readRet < static_cast<size_t>(fStat.st_size)) {
        LOGE(TAG, "Fail to read ROM (got only %zu bytes)", readRet);
        ret = -EIO;
        goto close_f;
    }

    m_RomData = std::move(rom);

    // Guess rom type
    int lowRomScore;
    int highRomScore;
    int headerAddress;

    lowRomScore = scoreHeader(kLowRomHeaderBase);
    highRomScore = scoreHeader(kHighRomHeaderBase);

    if (lowRomScore > highRomScore) {
        m_AddressingType = AddressingType::lowrom;
        headerAddress = kLowRomHeaderBase;
    } else {
        m_AddressingType = AddressingType::highrom;
        headerAddress = kHighRomHeaderBase;
    }

    // Parse header
    char title[kHeader_TitleSize+1];
    memset(title, 0, sizeof(title));
    strncpy(title, reinterpret_cast<const char*>(&m_RomData[headerAddress + kHeaderOffset_Title]), kHeader_TitleSize);
    LOGI(TAG, "ROM title: '%s'", title);

    // Get Rom speed
    romSpeedAndMode = m_RomData[headerAddress + kHeaderOffset_RomSpeedAndMode];
    m_FastRom = romSpeedAndMode & (1 << 4);

    // Get Ram size
    romRamInfo = m_RomData[headerAddress + kHeaderOffset_RomRamInfo];
    if (romRamInfo != 0) {
        m_SramSize = (1 << m_RomData[headerAddress + kHeaderOffset_SramSize]) * 1024;
    }

    LOGI(TAG, "SRAM size: %d Bytes", m_SramSize);

    fclose(f);

    m_RomBasename = std::filesystem::path(path).replace_extension("").string();

    return 0;

close_f:
    fclose(f);

    return ret;
}

int Snes::scoreHeader(uint32_t address)
{
    int score = 0;

    if(m_RomData.size() < address + 0x50) {
        return score;
    }

    uint8_t  mapMode     = m_RomData[address + 0x25] & ~0x10;  //ignore FastROM bit
    uint16_t complement  = m_RomData[address + 0x2c] << 0 | m_RomData[address + 0x2d] << 8;
    uint16_t checksum    = m_RomData[address + 0x2e] << 0 | m_RomData[address + 0x2f] << 8;
    uint16_t resetVector = m_RomData[address + 0x4c] << 0 | m_RomData[address + 0x4d] << 8;

    if(resetVector < 0x8000) {
        return score;  //$00:0000-7fff is never ROM data
    }

    uint8_t opcode = m_RomData[(address & ~0x7fff) | (resetVector & 0x7fff)];  //first instruction executed

    // most likely opcodes
    if (opcode == 0x78      //sei
      || opcode == 0x18     //clc (clc; xce)
      || opcode == 0x38     //sec (sec; xce)
      || opcode == 0x9c     //stz $nnnn (stz $4200)
      || opcode == 0x4c     //jmp $nnnn
      || opcode == 0x5c) {  //jml $nnnnnn
        score += 8;
    }

    // plausible opcodes
    if (opcode == 0xc2    //rep #$nn
     || opcode == 0xe2    //sep #$nn
     || opcode == 0xad    //lda $nnnn
     || opcode == 0xae    //ldx $nnnn
     || opcode == 0xac    //ldy $nnnn
     || opcode == 0xaf    //lda $nnnnnn
     || opcode == 0xa9    //lda #$nn
     || opcode == 0xa2    //ldx #$nn
     || opcode == 0xa0    //ldy #$nn
     || opcode == 0x20    //jsr $nnnn
     || opcode == 0x22) { //jsl $nnnnnn
        score += 4;
    }

    // implausible opcodes
    if (opcode == 0x40    //rti
     || opcode == 0x60    //rts
     || opcode == 0x6b    //rtl
     || opcode == 0xcd    //cmp $nnnn
     || opcode == 0xec    //cpx $nnnn
     || opcode == 0xcc) { //cpy $nnnn
        score -= 4;
    }

    // least likely opcodes
    if (opcode == 0x00    //brk #$nn
     || opcode == 0x02    //cop #$nn
     || opcode == 0xdb    //stp
     || opcode == 0x42    //wdm
     || opcode == 0xff) { //sbc $nnnnnn,x
        score -= 8;
    }

    if (checksum + complement == 0xffff) {
        score += 4;
    }

    if (address == 0x7fb0 && mapMode == 0x20) {
        score += 2;
    }

    if (address == 0xffb0 && mapMode == 0x21) {
        score += 2;
    }

    return std::max(0, score);

}

std::string Snes::getRomBasename() const
{
    return m_RomBasename;
}

int Snes::start()
{
    auto membus = std::make_shared<Membus>(m_AddressingType, m_FastRom);

    auto rom = std::make_shared<BufferMemComponent>(
        MemComponentType::rom,
        std::move(m_RomData));
    membus->plugComponent(rom);

    m_Ram = std::make_shared<Wram>();
    membus->plugComponent(m_Ram);

    m_IndirectWram = std::make_shared<IndirectWram>(m_Ram);
    membus->plugComponent(m_IndirectWram);

    if (m_SramSize > 0) {
        m_Sram = std::make_shared<Sram>(m_SramSize);
        membus->plugComponent(m_Sram);
        m_Sram->load(m_RomBasename + ".srm");
    }

    auto audioRenderCb = [this](const uint8_t* data, size_t sampleCount) {
        for (const auto& renderer : m_RendererList) {
            renderer->playAudioSamples(data, sampleCount);
        }
    };

    m_Apu = std::make_shared<Apu>(m_MasterClock, audioRenderCb);
    membus->plugComponent(m_Apu);

    auto renderCb = [this](const SnesColor& c) {
        for (const auto& renderer : m_RendererList) {
            renderer->drawPixel(c);
        }
    };

    m_Ppu = std::make_shared<Ppu>(renderCb);
    membus->plugComponent(m_Ppu);

    m_Maths = std::make_shared<Maths>();
    membus->plugComponent(m_Maths);

    m_Dma = std::make_shared<Dma>(membus);
    membus->plugComponent(m_Dma);

    m_ControllerPorts = std::make_shared<ControllerPorts>();
    membus->plugComponent(m_ControllerPorts);

    m_Cpu = std::make_shared<Cpu65816>(membus);

    auto snes = shared_from_this();
    membus->plugComponent(snes);

    m_Dma->setScheduler(snes);

    return 0;
}

int Snes::stop()
{
    if (m_Sram) {
        m_Sram->save(m_RomBasename + ".srm");
    }

    return 0;
}

int Snes::renderSingleFrame(bool renderPpu)
{
    bool scanEnded = false;

    if (renderPpu) {
        m_Ppu->setDrawConfig(Ppu::DrawConfig::Draw);
    } else {
        m_Ppu->setDrawConfig(Ppu::DrawConfig::Skip);
    }

    while (!scanEnded) {
        // DMA has priority over CPU
        if (m_Dma->getState() == SchedulerTask::State::running) {
            if (m_Dma->getNextRunCycle() <= m_MasterClock) {
                int dmaCycles = m_Dma->run();

                if (dmaCycles > 0) {
                    m_Dma->setNextRunCycle(m_MasterClock + dmaCycles);
                } else {
                    m_Dma->setIdle();

                    // Resume CPU at a multiple of 8 cycles. 0 is forbidden
                    auto cpuSync = m_MasterClock % 8;
                    if (cpuSync == 0) {
                        cpuSync = 8;
                    }

                    m_Cpu->setNextRunCycle(m_MasterClock + cpuSync);
                }
            }
        } else if (m_Cpu->getNextRunCycle() <= m_MasterClock) {
            m_CpuTime.begin();
            int cpuCycles = m_Cpu->run();
            m_CpuTime.end();

            m_Cpu->setNextRunCycle(m_MasterClock + cpuCycles);
        }

        // Check if Joypad autoread is complete
        if (m_JoypadAutoreadEndcycle && m_JoypadAutoreadEndcycle <= m_MasterClock) {
            m_HVBJOY &= ~1; // Autoread
            m_JoypadAutoreadEndcycle = 0;
            m_ControllerPorts->readController();
        }

        // Always run PPU
        if (m_Ppu->getNextRunCycle() <= m_MasterClock) {
            m_PpuTime.begin();
            int ppuCycles = m_Ppu->run();
            m_PpuTime.end();

            m_Ppu->setNextRunCycle(m_MasterClock + ppuCycles);
            auto ppuEvents = m_Ppu->getEvents();

            if (ppuEvents & Ppu::Event_ScanStarted) {
                for (const auto& renderer : m_RendererList) {
                    renderer->scanStarted();
                }

                m_Dma->onScanStarted();
            }

            if (ppuEvents & Ppu::Event_HBlankStart) {
                m_Dma->onHblank();

                m_HVBJOY |= 1 << 6;
            }

            if (ppuEvents & Ppu::Event_HBlankEnd) {
                m_HVBJOY &= ~(1 << 6);
                m_Apu->run();
            }

            if (ppuEvents & Ppu::Event_HV_IRQ) {
                setHVIRQ_Flag(true);
            }

            if (ppuEvents & Ppu::Event_VBlankStart) {
                m_Vblank = true;
                m_HVBJOY |= 1 << 7; // Vblank

                m_Dma->onVblank();

                if (m_JoypadAutoread) {
                    m_HVBJOY |= 1; // Autoread
                    m_JoypadAutoreadEndcycle = m_MasterClock + 4224;
                }

                // Dirty hack to avoid vblank to be retriggered
                if (m_NMIEnabled) {
                    m_Cpu->setNMI();
                }
            }

            if (ppuEvents & Ppu::Event_ScanEnded) {
                m_Vblank = false;
                m_HVBJOY &= ~(1 << 7);

                if (kLogTimings) {
                    LOGI(TAG, "CPU: %" PRId64 " ms - PPU: %" PRId64 " ms",
                         m_CpuTime.total<std::chrono::milliseconds>(),
                         m_PpuTime.total<std::chrono::milliseconds>());

                    m_CpuTime.reset();
                    m_PpuTime.reset();
                }

                for (const auto& renderer : m_RendererList) {
                    renderer->scanEnded();
                }

                scanEnded = true;
            }
        }

        m_MasterClock++;
    }

    return 0;
}

void Snes::resumeTask(SchedulerTask* task, int cycles)
{
    task->setNextRunCycle(m_MasterClock + cycles);
}

void Snes::setHVIRQ_Flag(bool v)
{
    m_HVIRQ_Flag = v;
    m_Cpu->setIRQ(v);
}


void Snes::setController1(const SnesController& controller)
{
    m_ControllerPorts->setController1(controller);
}

void Snes::saveState(const std::string& path)
{
    LOGI(TAG, "Save state to %s", path.c_str());

    FILE* f = fopen(path.c_str(), "wb");
    assert(f);

    m_Ram->dumpToFile(f);
    m_IndirectWram->dumpToFile(f);
    m_Apu->dumpToFile(f);
    m_Ppu->dumpToFile(f);
    m_Maths->dumpToFile(f);
    m_Dma->dumpToFile(f);
    m_ControllerPorts->dumpToFile(f);
    m_Cpu->dumpToFile(f);

    fwrite(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fwrite(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fwrite(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fwrite(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fwrite(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fwrite(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fwrite(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fwrite(&m_JoypadAutoreadRunning, sizeof(m_JoypadAutoreadRunning), 1, f);
    fwrite(&m_JoypadAutoreadEndcycle, sizeof(m_JoypadAutoreadEndcycle), 1, f);
    fwrite(&m_Vblank, sizeof(m_Vblank), 1, f);
    fwrite(&m_MasterClock, sizeof(m_MasterClock), 1, f);

    fclose(f);
}

void Snes::loadState(const std::string& path)
{
    LOGI(TAG, "Load state from %s", path.c_str());

    FILE* f = fopen(path.c_str(), "rb");
    assert(f);

    m_Ram->loadFromFile(f);
    m_IndirectWram->loadFromFile(f);
    m_Apu->loadFromFile(f);
    m_Ppu->loadFromFile(f);
    m_Maths->loadFromFile(f);
    m_Dma->loadFromFile(f);
    m_ControllerPorts->loadFromFile(f);
    m_Cpu->loadFromFile(f);

    fread(&m_HVBJOY, sizeof(m_HVBJOY), 1, f);
    fread(&m_NMIEnabled, sizeof(m_NMIEnabled), 1, f);
    fread(&m_HVIRQ_Config, sizeof(m_HVIRQ_Config), 1, f);
    fread(&m_HVIRQ_Flag, sizeof(m_HVIRQ_Flag), 1, f);
    fread(&m_HVIRQ_H, sizeof(m_HVIRQ_H), 1, f);
    fread(&m_HVIRQ_V, sizeof(m_HVIRQ_V), 1, f);
    fread(&m_JoypadAutoread, sizeof(m_JoypadAutoread), 1, f);
    fread(&m_JoypadAutoreadRunning, sizeof(m_JoypadAutoreadRunning), 1, f);
    fread(&m_JoypadAutoreadEndcycle, sizeof(m_JoypadAutoreadEndcycle), 1, f);
    fread(&m_Vblank, sizeof(m_Vblank), 1, f);
    fread(&m_MasterClock, sizeof(m_MasterClock), 1, f);

    fclose(f);
}

uint8_t Snes::readU8(uint32_t addr)
{
    switch (addr) {
    case kRegRDNMI: {
        return (m_Vblank << 7) | (1 << 6);
    }

    case kRegTIMEUP: {
        uint8_t ret = m_HVIRQ_Flag << 7;
        setHVIRQ_Flag(false);
        return ret;
    }

    case kRegHVBJOY:
        return m_HVBJOY;
    }

    LOGW(TAG, "Ignore ReadU8 at %06X", addr);
    assert(false);
    return 0;
}

void Snes::writeU8(uint32_t addr, uint8_t value)
{
    switch (addr) {
    case kRegNmitimen: {
        // H/V IRQ
        uint8_t enableHVIRQ = (value >> 4) & 0b11;
        if (m_HVIRQ_Config != enableHVIRQ) {
            LOGD(TAG, "H/V IRQ is now %s", m_HVIRQ_Config ? "enabled" : "disabled");
            m_HVIRQ_Config = enableHVIRQ;

            m_Ppu->setHVIRQConfig(
                static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
                m_HVIRQ_H,
                m_HVIRQ_V);
        }

        // NMI
        bool enableNMI = value & (1 << 7);
        if (m_NMIEnabled != enableNMI) {
            m_NMIEnabled = enableNMI;
            LOGI(TAG, "NMI is now %s", m_NMIEnabled ? "enabled" : "disabled");
        }

        // Joypad
        bool joypadAutoread = value & 1;
        if (m_JoypadAutoread != joypadAutoread) {
            LOGI(TAG, "Joypad autoread is now %s", joypadAutoread ? "enabled" : "disabled");
            m_JoypadAutoread = joypadAutoread;
        }

        break;
    }

    case kRegisterHTIMEL:
        m_HVIRQ_H = (m_HVIRQ_H & 0xFF00) | value;

        // FIXME: NTSC setting
        m_HVIRQ_H = std::min(m_HVIRQ_H, static_cast<uint16_t>(339));

        m_Ppu->setHVIRQConfig(
            static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
            m_HVIRQ_H,
            m_HVIRQ_V);

        break;

    case kRegisterHTIMEH:
        m_HVIRQ_H = (m_HVIRQ_H & 0xFF) | (value << 8);

        m_HVIRQ_H = std::min(m_HVIRQ_H, static_cast<uint16_t>(339));

        m_Ppu->setHVIRQConfig(
            static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
            m_HVIRQ_H,
            m_HVIRQ_V);

        break;

    case kRegisterVTIMEL:
        m_HVIRQ_V = (m_HVIRQ_V & 0xFF00) | value;

        // FIXME: NTSC setting
        m_HVIRQ_V = std::min(m_HVIRQ_V, static_cast<uint16_t>(261));

        m_Ppu->setHVIRQConfig(
            static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
            m_HVIRQ_H,
            m_HVIRQ_V);
        break;

    case kRegisterVTIMEH:
        m_HVIRQ_V = (m_HVIRQ_V & 0xFF) | (value << 8);

        m_Ppu->setHVIRQConfig(
            static_cast<Ppu::HVIRQConfig>(m_HVIRQ_Config),
            m_HVIRQ_H,
            m_HVIRQ_V);
        break;
    }
}
