#pragma once

#include <list>
#include <memory>

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
    void setCFlag(int16_t value);
    void setNZFlags(uint16_t value, uint16_t negativeMask);

    void handleADC(uint32_t data);
    void handleADCImmediate(uint32_t data);
    void handleANDImmediate(uint32_t data);
    void handleAND(uint32_t data);
    void handleASL_A(uint32_t data);
    void handleBCC(uint32_t data);
    void handleBCS(uint32_t data);
    void handleBEQ(uint32_t data);
    void handleBMI(uint32_t data);
    void handleBRA(uint32_t data);
    void handleBNE(uint32_t data);
    void handleBPL(uint32_t data);
    void handleBVS(uint32_t data);
    void handleCLC(uint32_t data);
    void handleCLI(uint32_t data);
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
    void handleNOP(uint32_t data);
    void handleORA(uint32_t data);
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
    void handleRTI(uint32_t data);
    void handleRTL(uint32_t data);
    void handleRTS(uint32_t data);
    void handleSBCImmediate(uint32_t data);
    void handleSBC(uint32_t data);
    void handleSEC(uint32_t data);
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
    void handleTXA(uint32_t data);
    void handleTXS(uint32_t data);
    void handleTXY(uint32_t data);
    void handleTYA(uint32_t data);
    void handleTYX(uint32_t data);
    void handleXBA(uint32_t data);
    void handleXCE(uint32_t data);

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
        uint16_t PC = 0x8000;

        uint16_t P = 0;
    };

    enum class AddressingMode {
        Invalid,
        Implied,
        Immediate,
        ImmediateA,
        ImmediateIndex,
        Absolute,
        AbsoluteIndexedX,
        AbsoluteIndexedY,
        AbsoluteLong,
        AbsoluteIndirectLong,
        AbsoluteLongIndexedX,
        Dp,
        DpIndexedX,
        DpIndirect,
        DpIndirectLong,
        DpIndirectLongIndexedY,
        PcRelative,
    };

    typedef void (Cpu65816::*OpcodeHandler)(uint32_t data);

    struct OpcodeDesc {
        const char* m_Name = nullptr;
        uint8_t m_Value = 0;
        AddressingMode m_AddressingMode = AddressingMode::Invalid;
        OpcodeHandler m_OpcodeHandler = nullptr;
    };

private:
    std::shared_ptr<Membus> m_Membus;

    OpcodeDesc m_Opcodes[0x100];
    Registers m_Registers;

    State m_State = State::running;
    bool m_NMI = false;

    std::list<std::string> m_InstructionsLog;
};
