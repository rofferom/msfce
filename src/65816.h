#pragma once

#include <list>
#include <memory>
#include <string>

#include "utils.h"

class Membus;

class Cpu65816 {
public:
    Cpu65816(const std::shared_ptr<Membus> membus);
    ~Cpu65816() = default;

    void executeSingle();

    void setNMI();

private:
    void logInstruction(uint32_t opcodePC, const char* strIntruction);
    void printInstructionsLog() const;

    void handleNMI();

    void setNFlag(uint16_t value, uint16_t negativeMask);
    void setZFlag(uint16_t value);
    void setCFlag(int32_t value);
    void setNZFlags(uint16_t value, uint16_t negativeMask);

    void handleADC(uint32_t data);
    void handleADCImmediate(uint32_t data);
    void handleANDImmediate(uint32_t data);
    void handleAND(uint32_t data);
    void handleASL_A(uint32_t data);
    void handleASL(uint32_t data);
    void handleBCC(uint32_t data);
    void handleBCS(uint32_t data);
    void handleBEQ(uint32_t data);
    void handleBITImmediate(uint32_t data);
    void handleBIT(uint32_t data);
    void handleBMI(uint32_t data);
    void handleBRA(uint32_t data);
    void handleBRL(uint32_t data);
    void handleBNE(uint32_t data);
    void handleBPL(uint32_t data);
    void handleBVC(uint32_t data);
    void handleBVS(uint32_t data);
    void handleCLC(uint32_t data);
    void handleCLD(uint32_t data);
    void handleCLI(uint32_t data);
    void handleCLV(uint32_t data);
    void handleCMP(uint32_t data);
    void handleCMPImmediate(uint32_t data);
    void handleCPX(uint32_t data);
    void handleCPXImmediate(uint32_t data);
    void handleCPY(uint32_t data);
    void handleCPYImmediate(uint32_t data);
    void handleDEC_A(uint32_t data);
    void handleDEC(uint32_t data);
    void handleDEX(uint32_t data);
    void handleDEY(uint32_t data);
    void handleEOR(uint32_t data);
    void handleEORImmediate(uint32_t data);
    void handleINC_A(uint32_t data);
    void handleINC(uint32_t data);
    void handleINX(uint32_t data);
    void handleINY(uint32_t data);
    void handleJMP(uint32_t data);
    void handleJSR(uint32_t data);
    void handleJSL(uint32_t data);
    void handleLDAImmediate(uint32_t data);
    void handleLDA(uint32_t data);
    void handleLDXImmediate(uint32_t data);
    void handleLDX(uint32_t data);
    void handleLDYImmediate(uint32_t data);
    void handleLDY(uint32_t data);
    void handleLSR_A(uint32_t data);
    void handleLSR(uint32_t data);
    void handleMVN(uint32_t data);
    void handleNOP(uint32_t data);
    void handleORA(uint32_t data);
    void handleORAImmediate(uint32_t data);
    void handlePEA(uint32_t data);
    void handlePEI(uint32_t data);
    void handlePER(uint32_t data);
    void handlePHA(uint32_t data);
    void handlePHB(uint32_t data);
    void handlePHD(uint32_t data);
    void handlePHK(uint32_t data);
    void handlePHP(uint32_t data);
    void handlePHX(uint32_t data);
    void handlePHY(uint32_t data);
    void handlePLA(uint32_t data);
    void handlePLB(uint32_t data);
    void handlePLD(uint32_t data);
    void handlePLP(uint32_t data);
    void handlePLX(uint32_t data);
    void handlePLY(uint32_t data);
    void handleREP(uint32_t data);
    void handleROL_A(uint32_t data);
    void handleROL(uint32_t data);
    void handleROR_A(uint32_t data);
    void handleROR(uint32_t data);
    void handleRTI(uint32_t data);
    void handleRTL(uint32_t data);
    void handleRTS(uint32_t data);
    void handleSBCImmediate(uint32_t data);
    void handleSBC(uint32_t data);
    void handleSEC(uint32_t data);
    void handleSED(uint32_t data);
    void handleSEI(uint32_t data);
    void handleSEP(uint32_t data);
    void handleSTA(uint32_t data);
    void handleSTX(uint32_t data);
    void handleSTY(uint32_t data);
    void handleSTZ(uint32_t data);
    void handleTAX(uint32_t data);
    void handleTAY(uint32_t data);
    void handleTCD(uint32_t data);
    void handleTCS(uint32_t data);
    void handleTDC(uint32_t data);
    void handleTRB(uint32_t data);
    void handleTSB(uint32_t data);
    void handleTSC(uint32_t data);
    void handleTSX(uint32_t data);
    void handleTXA(uint32_t data);
    void handleTXS(uint32_t data);
    void handleTXY(uint32_t data);
    void handleTYA(uint32_t data);
    void handleTYX(uint32_t data);
    void handleXBA(uint32_t data);
    void handleXCE(uint32_t data);
    void handleWAI(uint32_t data);

private:
    enum class State {
        running,
        interrupt,
    };

    struct Registers {
        uint16_t A = 0;
        uint16_t X = 0;
        uint16_t Y = 0;

        uint16_t S = 0x1FF;
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
        DpIndirectIndexedY,
        DpIndirectLong,
        DpIndirectLongIndexedY,
        PcRelative,
        PcRelativeLong,
        StackRelative,
        StackRelativeIndirectIndexedY,
        BlockMove,

        Count,
    };

    static constexpr uint8_t kAddressingMode = enumToInt(AddressingMode::Count);

    typedef void (Cpu65816::*OpcodeHandler)(uint32_t data);

    struct OpcodeDesc {
        const char* m_Name = nullptr;
        uint8_t m_Value = 0;
        AddressingMode m_AddressingMode = AddressingMode::Count;
        OpcodeHandler m_OpcodeHandler = nullptr;
        bool m_AutoStepPC = true;
    };

    static constexpr int kStrInstructionLen = 32;

    typedef void (Cpu65816::*AddressingModeHandler)(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

private:
    void handleImplied(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleImmediate(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleImmediateA(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleImmediateIndex(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsolute(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteJMP(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteJMPIndirectIndexedX(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteIndexedX(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteIndexedY(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteLong(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteIndirect(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteIndirectLong(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleAbsoluteLongIndexedX(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDp(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndexedX(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndexedY(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndirect(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndirectIndexedX(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndirectIndexedY(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndirectLong(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleDpIndirectLongIndexedY(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handlePcRelative(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handlePcRelativeLong(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleStackRelative(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleStackRelativeIndirectIndexedY(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

    void handleBlockMove(
        const OpcodeDesc& opcodeDesc,
        char strIntruction[kStrInstructionLen],
        uint32_t* data);

private:
    std::shared_ptr<Membus> m_Membus;

    OpcodeDesc m_Opcodes[0x100];
    AddressingModeHandler m_AddressingModes[kAddressingMode];
    Registers m_Registers;

    State m_State = State::running;
    bool m_NMI = false;
    bool m_WaitInterrupt = false;

    std::list<std::string> m_InstructionsLog;
};
