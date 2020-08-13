#include <assert.h>
#include <stdint.h>
#include "log.h"
#include "membus.h"
#include "registers.h"
#include "utils.h"
#include "65816.h"

#define TAG "65816"

namespace {

constexpr uint32_t kPRegister_C = 0;
constexpr uint32_t kPRegister_Z = 1;
constexpr uint32_t kPRegister_I = 2;
constexpr uint32_t kPRegister_D = 3;
constexpr uint32_t kPRegister_X = 4;
constexpr uint32_t kPRegister_M = 5;
constexpr uint32_t kPRegister_V = 6;
constexpr uint32_t kPRegister_N = 7;
constexpr uint32_t kPRegister_E = 8;

constexpr uint32_t getBit(uint32_t value, uint8_t bit) {
    return (value >> bit) & 1;
}

constexpr uint32_t setBit(uint32_t value, uint8_t bit) {
    return value | (1 << bit);
}

constexpr uint32_t clearBit(uint32_t value, uint8_t bit) {
    return value & ~(1 << bit);
}

} // anonymous namespace

Cpu65816::Cpu65816(const std::shared_ptr<Membus> membus)
    : m_Membus(membus)
{
    // Tweak P register initial value
    m_Registers.P = setBit(m_Registers.P, kPRegister_E);
    m_Registers.P = setBit(m_Registers.P, kPRegister_M);
    m_Registers.P = setBit(m_Registers.P, kPRegister_X);

    // Load opcodes
    static const OpcodeDesc s_OpcodeList[] = {
        {
            "ADC",
            0x69,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleADCImmediate,
        }, {
            "ADC",
            0x65,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleADC,
        }, {
            "AND",
            0x29,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleANDImmediate,
        }, {
            "AND",
            0x25,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleAND,
        }, {
            "ASL",
            0x0A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleASL_A,
        }, {
            "BEQ",
            0xF0,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBEQ,
        }, {
            "BCC",
            0x90,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBCC,
        }, {
            "BMI",
            0x30,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBMI,
        }, {
            "BNE",
            0xD0,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBNE,
        }, {
            "BPL",
            0x10,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBPL,
        }, {
            "BRA",
            0x80,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBRA,
        }, {
            "BVS",
            0x70,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBVS,
        }, {
            "CLC",
            0x18,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleCLC,
        }, {
            "CLI",
            0x58,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleCLI,
        }, {
            "CMP",
            0xC9,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleCMPImmediate,
        }, {
            "CMP",
            0xCD,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleCMP,
        }, {
            "CPX",
            0xE0,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleCPXImmediate,
        }, {
            "CPY",
            0xC0,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleCPYImmediate,
        }, {
            "DEC",
            0xC6,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleDEC,
        }, {
            "DEC",
            0xCE,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleDEC,
        }, {
            "DEX",
            0xCA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleDEX,
        }, {
            "DEY",
            0x88,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleDEY,
        }, {
            "INC",
            0x1A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleINC_A,
        }, {
            "INC",
            0xE6,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleINC,
        }, {
            "INC",
            0xEE,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleINC,
        }, {
            "INX",
            0xE8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleINX,
        }, {
            "INY",
            0xC8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleINY,
        }, {
            "JMP",
            0x5C,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleJMP,
        }, {
            "JMP",
            0xDC,
            Cpu65816::AddressingMode::AbsoluteIndirectLong,
            &Cpu65816::handleJMP,
        }, {
            "JMP",
            0x4C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleJMP,
        }, {
            "JSL",
            0x22,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleJSL,
        }, {
            "JSR",
            0x20,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleJSR,
        }, {
            "LDA",
            0xA9,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleLDAImmediate,
        }, {
            "LDA",
            0xAD,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xBD,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xB9,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xA5,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xB2,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xA7,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xB7,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xBF,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleLDA,
        }, {
            "LDX",
            0xA2,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleLDXImmediate,
        }, {
            "LDX",
            0xAE,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleLDX,
        }, {
            "LDX",
            0xA6,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleLDX,
        }, {
            "LDY",
            0xA0,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleLDYImmediate,
        }, {
            "LDY",
            0xA4,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleLDY,
        }, {
            "NOP",
            0xEA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleNOP,
        }, {
            "ORA",
            0x05,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x07,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x1D,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleORA,
        }, {
            "PHA",
            0x48,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHA,
        }, {
            "PHB",
            0x8B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHB,
        }, {
            "PHD",
            0x0B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHD,
        }, {
            "PHK",
            0x4B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHK,
        }, {
            "PHP",
            0x08,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHP,
        }, {
            "PHX",
            0xDA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHX,
        }, {
            "PHY",
            0x5A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePHY,
        }, {
            "PLA",
            0x68,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLA,
        }, {
            "PLB",
            0xAB,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLB,
        }, {
            "PLD",
            0x2B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLD,
        }, {
            "PLP",
            0x28,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLP,
        }, {
            "PLX",
            0xFA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLX,
        }, {
            "PLY",
            0x7A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handlePLY,
        }, {
            "REP",
            0xC2,
            Cpu65816::AddressingMode::Immediate,
            &Cpu65816::handleREP,
        }, {
            "ROL",
            0x2A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleROL_A,
        }, {
            "RTI",
            0x40,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleRTI,
        }, {
            "RTL",
            0x6B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleRTL,
        }, {
            "RTS",
            0x60,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleRTS,
        }, {
            "SBC",
            0xE9,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleSBC,
        }, {
            "SEC",
            0x38,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleSEC,
        }, {
            "SEI",
            0x78,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleSEI,
        }, {
            "SEP",
            0xE2,
            Cpu65816::AddressingMode::Immediate,
            &Cpu65816::handleSEP,
        }, {
            "STA",
            0x8D,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x85,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x9D,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x99,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x8F,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x9F,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x87,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x97,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleSTA,
        }, {
            "STX",
            0x86,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSTX,
        }, {
            "STX",
            0x8E,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleSTX,
        }, {
            "STY",
            0x84,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSTY,
        }, {
            "STY",
            0x8C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleSTY,
        }, {
            "STZ",
            0x74,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleSTZ,
        }, {
            "STZ",
            0x64,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSTZ,
        }, {
            "STZ",
            0x9C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleSTZ,
        }, {
            "STZ",
            0x9E,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleSTZ,
        }, {
            "TAX",
            0xAA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTAX,
        }, {
            "TAY",
            0xA8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTAY,
        }, {
            "TCD",
            0x5B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTCD,
        }, {
            "TCS",
            0x1B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTCS,
        }, {
            "TXA",
            0x8A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTXA,
        }, {
            "TXS",
            0x9A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTXS,
        }, {
            "TXY",
            0x9B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTXY,
        }, {
            "TYA",
            0x98,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTYA,
        }, {
            "TYX",
            0xBB,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTYX,
        }, {
            "XBA",
            0xEB,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleXBA,
        }, {
            "XCE",
            0xFB,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleXCE,
        },
    };

    for (size_t i = 0; i < SIZEOF_ARRAY(s_OpcodeList); i++) {
        m_Opcodes[s_OpcodeList[i].m_Value] = s_OpcodeList[i];
    }

    LOGI(TAG, "%zu opcodes registered", SIZEOF_ARRAY(s_OpcodeList));
}

void Cpu65816::executeSingle()
{
    // Check if NMI has been raised
    if (m_NMI) {
        if (!getBit(m_Registers.P, kPRegister_I)) {
            handleNMI();
        }

        m_NMI = false;
    }

    // Debug stuff
    char strIntruction[32];
    uint32_t opcodePC = (m_Registers.PB << 16) | m_Registers.PC;

    // Load opcode
    uint8_t opcode = m_Membus->readU8(opcodePC);
    const auto& opcodeDesc = m_Opcodes[opcode];

    if (!opcodeDesc.m_Name) {
        LOGC(TAG, "Unknown opcode %02X (Address %06X)", opcode, opcodePC);
        assert(false);
    }

    m_Registers.PC++;

    // Load data
    uint32_t data = 0;

    switch (opcodeDesc.m_AddressingMode) {
    case AddressingMode::Implied:
        snprintf(strIntruction, sizeof(strIntruction), "%s", opcodeDesc.m_Name);
        break;

    case AddressingMode::Immediate:
        data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 1;
        snprintf(strIntruction, sizeof(strIntruction), "%s #$%02X", opcodeDesc.m_Name, data);
        break;

    case AddressingMode::ImmediateA: {
        auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

        // 0: 16 bits, 1: 8 bits
        if (accumulatorSize) {
            data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
            m_Registers.PC += 1;

            snprintf(strIntruction, sizeof(strIntruction), "%s #$%02X", opcodeDesc.m_Name, data);
        } else {
            data = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
            m_Registers.PC += 2;

            snprintf(strIntruction, sizeof(strIntruction), "%s #$%04X", opcodeDesc.m_Name, data);
        }

        break;
    }

    case AddressingMode::ImmediateIndex: {
        auto indexSize = getBit(m_Registers.P, kPRegister_X);

        // 0: 16 bits, 1: 8 bits
        if (indexSize) {
            data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
            m_Registers.PC += 1;

            snprintf(strIntruction, sizeof(strIntruction), "%s #$%02X", opcodeDesc.m_Name, data);
        } else {
            data = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
            m_Registers.PC += 2;

            snprintf(strIntruction, sizeof(strIntruction), "%s #$%04X", opcodeDesc.m_Name, data);
        }

        break;
    }

    case AddressingMode::Absolute: {
        uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        data = (m_Registers.DB << 16) | rawData;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%04X [%06X]", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::AbsoluteIndexedX: {
        uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        data = (m_Registers.DB << 16) | rawData;
        data += m_Registers.X;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%04X,X [%06X]", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::AbsoluteIndexedY: {
        uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        data = (m_Registers.DB << 16) | rawData;
        data += m_Registers.Y;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%04X,Y [%06X]", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::AbsoluteLong: {
        data = m_Membus->readU24((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 3;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%06X ", opcodeDesc.m_Name, data);
        break;
    }

    case AddressingMode::AbsoluteIndirectLong: {
        uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        uint32_t address = (m_Registers.DB << 16) | rawData;
        address = m_Membus->readU24(address);

        data = address;

        snprintf(strIntruction, sizeof(strIntruction), "%s [$%04X] [%06X]", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::AbsoluteLongIndexedX: {
        uint32_t rawData = m_Membus->readU24((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 3;

        data = rawData + m_Registers.X;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%06X,X [%06X] ", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::Dp: {
        uint32_t rawData =  m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC++;

        data = rawData;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%02X [%06X] ", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::DpIndexedX: {
        uint32_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC++;

        data = rawData + m_Registers.X;

        snprintf(strIntruction, sizeof(strIntruction), "%s $%02X,X [%06X] ", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::DpIndirect: {
        uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC++;

        uint32_t address = ((m_Registers.DB << 16) | rawData);
        address = (m_Registers.DB << 16) | m_Membus->readU16(address);

        data = address;

        snprintf(strIntruction, sizeof(strIntruction), "%s ($%02X) [%06X] ", opcodeDesc.m_Name, rawData, address);
        break;
    }

    case AddressingMode::DpIndirectLong: {
        uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC++;

        uint32_t address = ((m_Registers.DB << 16) | rawData);
        address = m_Membus->readU24(address);

        data = address;

        snprintf(strIntruction, sizeof(strIntruction), "%s [$%02X] [%06X] ", opcodeDesc.m_Name, rawData, address);
        break;
    }

    case AddressingMode::DpIndirectLongIndexedY: {
        uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC++;

        uint32_t address = ((m_Registers.DB << 16) | rawData);
        address = m_Membus->readU24(address) + m_Registers.Y;

        data = address;

        snprintf(strIntruction, sizeof(strIntruction), "%s [$%02X],Y [%06X] ", opcodeDesc.m_Name, rawData, address);
        break;
    }

    case AddressingMode::PcRelative: {
        uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 1;

        data = m_Registers.PC + static_cast<int8_t>(rawData);

        snprintf(strIntruction, sizeof(strIntruction), "%s $%02X [%06X]", opcodeDesc.m_Name, rawData, data);
        break;
    }

    case AddressingMode::Invalid:
    default:
        assert(false);
    }

    if (m_State == State::running ) {
        LOGI(TAG, "0X%06X %-32s A:%04X X:%04X Y:%04X S:%04X D:%04X DB:%02X P:%02X",
            opcodePC,
            strIntruction,
            m_Registers.A,
            m_Registers.X,
            m_Registers.Y,
            m_Registers.S,
            m_Registers.D,
            m_Registers.DB,
            m_Registers.P);
    }

    assert(opcodeDesc.m_OpcodeHandler);
    (this->*opcodeDesc.m_OpcodeHandler)(data);
}

void Cpu65816::handleNMI()
{
    // Bank is forced at 0
    uint16_t handlerAddress = m_Membus->readU16(kRegIV_NMI);

    // Save registers
    m_Registers.S--;
    m_Membus->writeU8(m_Registers.S, m_Registers.P);

    m_Registers.S--;
    m_Membus->writeU8(m_Registers.S, m_Registers.PB);

    m_Registers.S -= 2;
    m_Membus->writeU16(m_Registers.S, m_Registers.PC & 0xFFFF);

    // Do jump
    m_Registers.PB = 0;
    m_Registers.PC = handlerAddress;

    m_State = State::interrupt;
}

void Cpu65816::setNFlag(uint16_t value, uint16_t negativeMask)
{
    if (value & negativeMask) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_N);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_N);
    }
}

void Cpu65816::setZFlag(uint16_t value)
{
    if (value == 0) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_Z);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_Z);
    }
}

void Cpu65816::setCFlag(int16_t value)
{
    if (value >= 0) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::setNZFlags(uint16_t value, uint16_t negativeMask)
{
    setNFlag(value, negativeMask);
    setZFlag(value);
}

void Cpu65816::handleADC(uint32_t address)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    assert(!getBit(m_Registers.P, kPRegister_D));

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint32_t data = m_Membus->readU8(address);
        uint32_t result = (m_Registers.A & 0xFF) + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        uint8_t data8 = data;
        if ((m_Registers.A & 0x80) == (data8 & 0x80) && (m_Registers.A & 0x80) != (result & 0x80)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        m_Registers.A = (m_Registers.A & 0xFF00) | (result & 0xFF);

        // Z and N Flag
        setZFlag(m_Registers.A & 0xFF);
        setNFlag(m_Registers.A, 0x80);

        // C Flag
        if (result >= 0x100) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }
    } else {
        uint32_t data = m_Membus->readU16(address);
        uint32_t result = m_Registers.A + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        uint16_t data16 = data;
        if (~(m_Registers.A ^ data16) & (data16 ^ static_cast<uint16_t>(result)) & 0x8000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        m_Registers.A = result & 0xFFFF;

        // Z and N Flag
        setZFlag(m_Registers.A);
        setNFlag(m_Registers.A, 0x8000);

        // C Flag
        if (result >= 0x10000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }
    }
}

void Cpu65816::handleADCImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    assert(!getBit(m_Registers.P, kPRegister_D));

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint32_t result = (m_Registers.A & 0xFF) + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        uint8_t data8 = data;
        if ((m_Registers.A & 0x80) == (data8 & 0x80) && (m_Registers.A & 0x80) != (result & 0x80)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        m_Registers.A = (m_Registers.A & 0xFF00) | (result & 0xFF);

        // Z and N Flag
        setZFlag(m_Registers.A & 0xFF);
        setNFlag(m_Registers.A, 0x80);

        // C Flag
        if (result >= 0x100) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }
    } else {
        uint32_t result = m_Registers.A + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        uint16_t data16 = data;
        if (~(m_Registers.A ^ data16) & (data16 ^ static_cast<uint16_t>(result)) & 0x8000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        m_Registers.A = result & 0xFFFF;

        // Z and N Flag
        setZFlag(m_Registers.A);
        setNFlag(m_Registers.A, 0x8000);

        // C Flag
        if (result >= 0x10000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }
    }
}

void Cpu65816::handleANDImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) & data);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A &= data;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleAND(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = m_Membus->readU8(data);
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) & value);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A &= m_Membus->readU16(data);
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleASL_A(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Registers.A & 0xFF;

        v <<= 1;
        C = v >> 8;

        m_Registers.A = (m_Registers.A & 0xFF00) | (v & 0xFF);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        uint32_t v = m_Registers.A;

        v <<= 1;
        C = v >> 16;

        m_Registers.A = v & 0xFFFF;
        setNZFlags(m_Registers.A, 0x8000);
    }

    if (C) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleBCC(uint32_t data)
{
    if (!getBit(m_Registers.P, kPRegister_C)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBEQ(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_Z)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBMI(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_N)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBNE(uint32_t data)
{
    if (!getBit(m_Registers.P, kPRegister_Z)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBPL(uint32_t data)
{
    if (!getBit(m_Registers.P, kPRegister_N)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBRA(uint32_t data)
{
    m_Registers.PC = data;
}

void Cpu65816::handleBVS(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_V)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleCLC(uint32_t data)
{
    m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
}

void Cpu65816::handleCLI(uint32_t data)
{
    m_Registers.P = clearBit(m_Registers.P, kPRegister_I);
}

void Cpu65816::handleCMPImmediate(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        result = (m_Registers.A & 0xFF) - data;
        negativeMask = 0x80;
    } else {
        result = m_Registers.A - data;
        negativeMask = 0x8000;
    }

    setNZFlags(result, negativeMask);
    setCFlag(result);
}

void Cpu65816::handleCMP(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        result = (m_Registers.A & 0xFF) - m_Membus->readU8(data);
        negativeMask = 0x80;
    } else {
        result = m_Registers.A - m_Membus->readU16(data);
        negativeMask = 0x8000;
    }

    setNZFlags(result, negativeMask);
    setCFlag(result);
}

void Cpu65816::handleCPXImmediate(uint32_t data)
{
    uint16_t negativeMask;
    auto indexSize = getBit(m_Registers.P, kPRegister_X);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        result = (m_Registers.X & 0xFF) - static_cast<uint8_t>(data);
        negativeMask = 0x80;
    } else {
        result  = m_Registers.X - static_cast<uint16_t>(data);
        negativeMask = 0x8000;
    }

    setNZFlags(result, negativeMask);
    setCFlag(result);
}

void Cpu65816::handleCPYImmediate(uint32_t data)
{
    uint16_t negativeMask;
    auto indexSize = getBit(m_Registers.P, kPRegister_X);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        result = (m_Registers.Y & 0xFF) - static_cast<uint8_t>(data);
        negativeMask = 0x80;
    } else {
        result  = m_Registers.Y - static_cast<uint16_t>(data);
        negativeMask = 0x8000;
    }

    setNZFlags(result, negativeMask);
    setCFlag(result);
}

void Cpu65816::handleDEC(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = m_Membus->readU8(data) - 1;
        m_Membus->writeU8(data, value);

        negativeMask = 0x80;
        setNZFlags(value, negativeMask);
    } else {
        uint16_t value = m_Membus->readU16(data) - 1;
        m_Membus->writeU16(data, value);

        negativeMask = 0x8000;
        setNZFlags(value, negativeMask);
    }
}

void Cpu65816::handleDEX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X--;
        m_Registers.X &= 0xFF;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X--;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleDEY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y--;
        m_Registers.Y &= 0xFF;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y--;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleINC_A(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A + 1) & 0xFF);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A++;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleINC(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = m_Membus->readU8(data) + 1;
        m_Membus->writeU8(data, value);

        negativeMask = 0x80;
        setNZFlags(value, negativeMask);
    } else {
        uint16_t value = m_Membus->readU16(data) + 1;
        m_Membus->writeU16(data, value);

        negativeMask = 0x8000;
        setNZFlags(value, negativeMask);
    }
}

void Cpu65816::handleINX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X = (m_Registers.X & 0xFF00) | ((m_Registers.X + 1) & 0xFF);
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X++;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleINY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y = (m_Registers.Y & 0xFF00) | ((m_Registers.Y + 1) & 0xFF);
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y++;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleJMP(uint32_t data)
{
    m_Registers.PC = data;
}

void Cpu65816::handleJSR(uint32_t data)
{
    m_Registers.S -= 2;
    m_Membus->writeU16(m_Registers.S, m_Registers.PC - 1);

    m_Registers.PC = data;
}

void Cpu65816::handleJSL(uint32_t data)
{
    m_Registers.S -= 1;
    m_Membus->writeU8(m_Registers.S, m_Registers.PC >> 16);

    m_Registers.S -= 2;
    m_Membus->writeU16(m_Registers.S, (m_Registers.PC & 0xFFFF) - 1);

    m_Registers.PB = data >> 16;
    m_Registers.PC = data & 0xFFFF;
}

void Cpu65816::handleLDAImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A &= 0xFF00;
        m_Registers.A |= data & 0xFF;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = data;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleLDA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t value = m_Membus->readU8(data);
        m_Registers.A &= 0xFF00;
        m_Registers.A |= value & 0xFF;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Membus->readU16(data);;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleLDXImmediate(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X = data & 0xFF;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = data;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleLDX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X = m_Membus->readU8(data);
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Membus->readU16(data);
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleLDYImmediate(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y = data & 0xFF;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = data;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleLDY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y = m_Membus->readU8(data);
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = m_Membus->readU16(data);
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleNOP(uint32_t data)
{
}

void Cpu65816::handleORA(uint32_t data)
{
    uint16_t negativeMask;
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = m_Membus->readU8(data);
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) | value);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A |= m_Membus->readU16(data);
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handlePHA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.S--;
        m_Membus->writeU8(m_Registers.S, m_Registers.A & 0xFF);
    } else {
        m_Registers.S -= 2;
        m_Membus->writeU16(m_Registers.S, m_Registers.A);
    }
}

void Cpu65816::handlePHB(uint32_t data)
{
    m_Registers.S--;
    m_Membus->writeU8(m_Registers.S, m_Registers.DB);
}

void Cpu65816::handlePHD(uint32_t data)
{
    m_Registers.S -= 2;
    m_Membus->writeU16(m_Registers.S, m_Registers.D);
}

void Cpu65816::handlePHK(uint32_t data)
{
    m_Registers.S--;
    m_Membus->writeU8(m_Registers.S, m_Registers.PB);
}

void Cpu65816::handlePHP(uint32_t data)
{
    m_Registers.S--;
    m_Membus->writeU8(m_Registers.S, m_Registers.P);
}

void Cpu65816::handlePHX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.S--;
        m_Membus->writeU8(m_Registers.S, m_Registers.X);
    } else {
        m_Registers.S -= 2;
        m_Membus->writeU16(m_Registers.S, m_Registers.X);
    }
}

void Cpu65816::handlePHY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.S--;
        m_Membus->writeU8(m_Registers.S, m_Registers.Y);
    } else {
        m_Registers.S -= 2;
        m_Membus->writeU16(m_Registers.S, m_Registers.Y);
    }
}

void Cpu65816::handlePLA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A = (m_Registers.A & 0xFF00) | m_Membus->readU8(m_Registers.S);
        m_Registers.S++;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Membus->readU16(m_Registers.S);
        m_Registers.S += 2;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handlePLB(uint32_t data)
{
    m_Registers.DB = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    setNZFlags(m_Registers.DB, 0x80);
}

void Cpu65816::handlePLD(uint32_t data)
{
    m_Registers.D = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    setNZFlags(m_Registers.D, 0x80);
}

void Cpu65816::handlePLP(uint32_t data)
{
    m_Registers.P = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    if (getBit(m_Registers.P, kPRegister_X)) {
        m_Registers.X &= 0xFF;
        m_Registers.Y &= 0xFF;
    }
}

void Cpu65816::handlePLX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X = m_Membus->readU8(m_Registers.S);
        m_Registers.S++;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Membus->readU16(m_Registers.S);
        m_Registers.S += 2;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handlePLY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y = m_Membus->readU8(m_Registers.S);
        m_Registers.S++;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = m_Membus->readU16(m_Registers.S);
        m_Registers.S += 2;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleREP(uint32_t data)
{
    m_Registers.P &= ~data;
}

void Cpu65816::handleROL_A(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Registers.A & 0xFF;

        v = (v << 1) | C;
        C = v >> 8;

        m_Registers.A = (m_Registers.A & 0xFF00) | (v & 0xFF);
    } else {
        uint32_t v = m_Registers.A;

        v = (v << 1) | C;
        C = v >> 16;

        m_Registers.A = v & 0xFFFF;
    }

    if (C) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleRTI(uint32_t data)
{
    // Save registers
    uint16_t PC = m_Membus->readU16(m_Registers.S);
    m_Registers.S += 2;

    uint16_t PB = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    uint16_t P = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    // Do jump
    m_Registers.PB = PB;
    m_Registers.PC = PC;
    m_Registers.P = P;

    if (getBit(m_Registers.P, kPRegister_X)) {
        m_Registers.X &= 0xFF;
        m_Registers.Y &= 0xFF;
    }

    m_State = State::running;
}

void Cpu65816::handleRTL(uint32_t data)
{
    uint16_t PC = m_Membus->readU16(m_Registers.S) + 1;
    m_Registers.S += 2;

    uint16_t PB = m_Membus->readU8(m_Registers.S);
    m_Registers.S++;

    m_Registers.PC = PC;
    m_Registers.PB = PB;
}

void Cpu65816::handleRTS(uint32_t data)
{
    m_Registers.PC = m_Membus->readU16(m_Registers.S) + 1;
    m_Registers.S += 2;
}

void Cpu65816::handleSBC(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_C)) {
        m_Registers.A -= data;
    } else {
        m_Registers.A -= data + 1;
    }
}

void Cpu65816::handleSEC(uint32_t data)
{
    m_Registers.P = setBit(m_Registers.P, kPRegister_C);
}

void Cpu65816::handleSEI(uint32_t data)
{
    m_Registers.P = setBit(m_Registers.P, kPRegister_I);
}

void Cpu65816::handleSEP(uint32_t data)
{
    m_Registers.P |= data;

    if (getBit(m_Registers.P, kPRegister_X)) {
        m_Registers.X &= 0xFF;
        m_Registers.Y &= 0xFF;
    }
}

void Cpu65816::handleSTA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Membus->writeU8(data, m_Registers.A & 0xFF);
    } else {
        m_Membus->writeU16(data, m_Registers.A);
    }
}

void Cpu65816::handleSTX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Membus->writeU8(data, m_Registers.X & 0xFF);
    } else {
        m_Membus->writeU16(data, m_Registers.X);
    }
}

void Cpu65816::handleSTY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Membus->writeU8(data, m_Registers.Y & 0xFF);
    } else {
        m_Membus->writeU16(data, m_Registers.Y);
    }
}

void Cpu65816::handleSTZ(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Membus->writeU8(data, 0);
    } else {
        m_Membus->writeU16(data, 0);
    }
}

void Cpu65816::handleTAX(uint32_t data)
{
    uint16_t negativeMask;
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X &= 0xFF00;
        m_Registers.X |= m_Registers.A & 0xFF;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Registers.A;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleTAY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y &= 0xFF00;
        m_Registers.Y |= m_Registers.A & 0xFF;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = m_Registers.A;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleTCD(uint32_t data)
{
    m_Registers.D = m_Registers.A;

    setNZFlags(m_Registers.D, 0x8000);
}

void Cpu65816::handleTCS(uint32_t data)
{
    m_Registers.S = m_Registers.A;
}

void Cpu65816::handleTXA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A &= 0xFF00;
        m_Registers.A |= m_Registers.X & 0xFF;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Registers.X;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleTXS(uint32_t data)
{
    m_Registers.S = m_Registers.X;
}

void Cpu65816::handleTXY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y &= 0xFF00;
        m_Registers.Y |= m_Registers.X & 0xFF;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = m_Registers.X;
        setNZFlags(m_Registers.Y, 0x8000);
    }
}

void Cpu65816::handleTYA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A &= 0xFF00;
        m_Registers.A |= m_Registers.Y & 0xFF;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Registers.Y;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleTYX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X &= 0xFF00;
        m_Registers.X |= m_Registers.Y & 0xFF;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Registers.Y;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handleXBA(uint32_t data)
{
    m_Registers.A = ((m_Registers.A & 0xFF) << 8) | (m_Registers.A >> 8);

    setNZFlags(m_Registers.A, 0x8000);
}

void Cpu65816::handleXCE(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_C)) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_E);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_E);
    }
}

void Cpu65816::setNMI()
{
    m_NMI = true;
}
