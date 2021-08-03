#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>

#include "utils.h"
#include "schedulertask.h"

class Membus;

class Cpu65816 : public SchedulerTask {
public:
    Cpu65816(const std::shared_ptr<Membus> membus);
    ~Cpu65816() = default;

    int run() override;

    void setNMI();
    void setIRQ(bool value);

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    template <typename... Args>
    void logInstruction(const char* format, Args... args);

    void printInstructionsLog() const;

    void handleInterrupt(uint32_t addr, int *cycles);
    void handleNMI(int *cycles);
    void handleIRQ(int *cycles);

    void setNFlag(uint16_t value, uint16_t negativeMask);
    void setZFlag(uint16_t value);
    void setCFlag(int32_t value);
    void setNZFlags(uint16_t value, uint16_t negativeMask);

    void handleADC(uint32_t data, int *cycles);
    void handleADCImmediate(uint32_t data, int *cycles);
    void handleANDImmediate(uint32_t data, int *cycles);
    void handleAND(uint32_t data, int *cycles);
    void handleASL_A(uint32_t data, int *cycles);
    void handleASL(uint32_t data, int *cycles);
    void handleBCC(uint32_t data, int *cycles);
    void handleBCS(uint32_t data, int *cycles);
    void handleBEQ(uint32_t data, int *cycles);
    void handleBITImmediate(uint32_t data, int *cycles);
    void handleBIT(uint32_t data, int *cycles);
    void handleBMI(uint32_t data, int *cycles);
    void handleBRA(uint32_t data, int *cycles);
    void handleBRK(uint32_t data, int *cycles);
    void handleBRL(uint32_t data, int *cycles);
    void handleBNE(uint32_t data, int *cycles);
    void handleBPL(uint32_t data, int *cycles);
    void handleBVC(uint32_t data, int *cycles);
    void handleBVS(uint32_t data, int *cycles);
    void handleCLC(uint32_t data, int *cycles);
    void handleCLD(uint32_t data, int *cycles);
    void handleCLI(uint32_t data, int *cycles);
    void handleCLV(uint32_t data, int *cycles);
    void handleCMP(uint32_t data, int *cycles);
    void handleCMPImmediate(uint32_t data, int *cycles);
    void handleCPX(uint32_t data, int *cycles);
    void handleCPXImmediate(uint32_t data, int *cycles);
    void handleCPY(uint32_t data, int *cycles);
    void handleCPYImmediate(uint32_t data, int *cycles);
    void handleDEC_A(uint32_t data, int *cycles);
    void handleDEC(uint32_t data, int *cycles);
    void handleDEX(uint32_t data, int *cycles);
    void handleDEY(uint32_t data, int *cycles);
    void handleEOR(uint32_t data, int *cycles);
    void handleEORImmediate(uint32_t data, int *cycles);
    void handleINC_A(uint32_t data, int *cycles);
    void handleINC(uint32_t data, int *cycles);
    void handleINX(uint32_t data, int *cycles);
    void handleINY(uint32_t data, int *cycles);
    void handleJMP(uint32_t data, int *cycles);
    void handleJSR(uint32_t data, int *cycles);
    void handleJSL(uint32_t data, int *cycles);
    void handleLDAImmediate(uint32_t data, int *cycles);
    void handleLDA(uint32_t data, int *cycles);
    void handleLDXImmediate(uint32_t data, int *cycles);
    void handleLDX(uint32_t data, int *cycles);
    void handleLDYImmediate(uint32_t data, int *cycles);
    void handleLDY(uint32_t data, int *cycles);
    void handleLSR_A(uint32_t data, int *cycles);
    void handleLSR(uint32_t data, int *cycles);
    void handleMVN(uint32_t data, int *cycles);
    void handleMVP(uint32_t data, int *cycles);
    void handleNOP(uint32_t data, int *cycles);
    void handleORA(uint32_t data, int *cycles);
    void handleORAImmediate(uint32_t data, int *cycles);
    void handlePEA(uint32_t data, int *cycles);
    void handlePEI(uint32_t data, int *cycles);
    void handlePER(uint32_t data, int *cycles);
    void handlePHA(uint32_t data, int *cycles);
    void handlePHB(uint32_t data, int *cycles);
    void handlePHD(uint32_t data, int *cycles);
    void handlePHK(uint32_t data, int *cycles);
    void handlePHP(uint32_t data, int *cycles);
    void handlePHX(uint32_t data, int *cycles);
    void handlePHY(uint32_t data, int *cycles);
    void handlePLA(uint32_t data, int *cycles);
    void handlePLB(uint32_t data, int *cycles);
    void handlePLD(uint32_t data, int *cycles);
    void handlePLP(uint32_t data, int *cycles);
    void handlePLX(uint32_t data, int *cycles);
    void handlePLY(uint32_t data, int *cycles);
    void handleREP(uint32_t data, int *cycles);
    void handleROL_A(uint32_t data, int *cycles);
    void handleROL(uint32_t data, int *cycles);
    void handleROR_A(uint32_t data, int *cycles);
    void handleROR(uint32_t data, int *cycles);
    void handleRTI(uint32_t data, int *cycles);
    void handleRTL(uint32_t data, int *cycles);
    void handleRTS(uint32_t data, int *cycles);
    void handleSBCImmediate(uint32_t data, int *cycles);
    void handleSBC(uint32_t data, int *cycles);
    void handleSEC(uint32_t data, int *cycles);
    void handleSED(uint32_t data, int *cycles);
    void handleSEI(uint32_t data, int *cycles);
    void handleSEP(uint32_t data, int *cycles);
    void handleSTA(uint32_t data, int *cycles);
    void handleSTX(uint32_t data, int *cycles);
    void handleSTY(uint32_t data, int *cycles);
    void handleSTZ(uint32_t data, int *cycles);
    void handleTAX(uint32_t data, int *cycles);
    void handleTAY(uint32_t data, int *cycles);
    void handleTCD(uint32_t data, int *cycles);
    void handleTCS(uint32_t data, int *cycles);
    void handleTDC(uint32_t data, int *cycles);
    void handleTRB(uint32_t data, int *cycles);
    void handleTSB(uint32_t data, int *cycles);
    void handleTSC(uint32_t data, int *cycles);
    void handleTSX(uint32_t data, int *cycles);
    void handleTXA(uint32_t data, int *cycles);
    void handleTXS(uint32_t data, int *cycles);
    void handleTXY(uint32_t data, int *cycles);
    void handleTYA(uint32_t data, int *cycles);
    void handleTYX(uint32_t data, int *cycles);
    void handleXBA(uint32_t data, int *cycles);
    void handleXCE(uint32_t data, int *cycles);
    void handleWAI(uint32_t data, int *cycles);

private:
    enum class State {
        running,
        interrupt,
    };

    struct Registers {
        uint16_t A = 0;
        uint16_t X = 0;
        uint16_t Y = 0;

        uint16_t S = 0x1FD;
        uint8_t DB = 0;
        uint16_t D = 0;

        uint8_t PB = 0;
        uint16_t PC = 0;

        uint16_t P = 0;
    };

    enum class AddressingMode : uint8_t {
        Implied,
        Immediate,
        ImmediateA,
        ImmediateIndex,
        Absolute,
        AbsoluteJMP,
        AbsoluteJMPIndirectIndexedX,
        AbsoluteIndexedX,
        AbsoluteIndexedY,
        AbsoluteLong,
        AbsoluteIndirect,
        AbsoluteIndirectLong,
        AbsoluteLongIndexedX,
        Dp,
        DpIndexedX,
        DpIndexedY,
        DpIndirect,
        DpIndirectIndexedX,
        DpIndexedIndirectY,
        DpIndirectLong,
        DpIndirectLongIndexedY,
        PcRelative,
        PcRelativeLong,
        StackRelative,
        StackRelativeIndirectIndexedY,
        BlockMove,

        Count,
    };

    enum {
        OpcodeFlag_AutoIncrementPC = (1 << 1),
        OpcodeFlag_CheckIndexCross = (1 << 2),

        OpcodeFlag_Default = OpcodeFlag_AutoIncrementPC,
    };

    static constexpr uint8_t kAddressingMode = enumToInt(AddressingMode::Count);

    typedef void (Cpu65816::*OpcodeHandler)(uint32_t data, int *cycles);

    struct OpcodeDesc {
        const char* m_Name = nullptr;
        uint8_t m_Value = 0;
        AddressingMode m_AddressingMode = AddressingMode::Count;
        uint32_t m_Flags = OpcodeFlag_Default;
        OpcodeHandler m_OpcodeHandler = nullptr;
    };

    typedef void (Cpu65816::*AddressingModeHandler)(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

private:
    void handleImplied(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleImmediate(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleImmediateA(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleImmediateIndex(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsolute(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteJMP(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteJMPIndirectIndexedX(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteIndexedX(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteIndexedY(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteLong(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteIndirect(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteIndirectLong(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleAbsoluteLongIndexedX(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDp(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndexedX(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndexedY(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndirect(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndirectIndexedX(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndexedIndirectY(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndirectLong(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleDpIndirectLongIndexedY(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handlePcRelative(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handlePcRelativeLong(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleStackRelative(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleStackRelativeIndirectIndexedY(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void handleBlockMove(
        const OpcodeDesc& opcodeDesc,
        uint32_t* data,
        int *cycles);

    void addCyclesDp(int *cycles);
    void addCyclesIndexed(int *cycles);
    void addCyclesIndexCross(int *cycles, uint32_t addr, uint32_t shiftedAddr);

private:
    std::shared_ptr<Membus> m_Membus;

    OpcodeDesc m_Opcodes[0x100];
    AddressingModeHandler m_AddressingModes[kAddressingMode];
    Registers m_Registers;

    uint32_t m_CurrentOpcodePC = 0;

    State m_IRQState = State::running;
    bool m_NMI = false;
    bool m_IRQ = false;
    bool m_WaitInterrupt = false;

    using InstructionLogBuilder = std::function<std::string()>;
    std::list<InstructionLogBuilder> m_InstructionsLog;
};
