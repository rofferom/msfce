#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include "log.h"
#include "membus.h"
#include "registers.h"
#include "65816.h"

#define TAG "65816"

namespace {

constexpr size_t kInstructionsLogSize = 10;

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

    m_Registers.PC = m_Membus->readU16(kRegIV_RESET);

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
            "ADC",
            0x72,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x67,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x61,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x71,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x77,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x75,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x6D,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x6F,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x7F,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x7D,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x79,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x63,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleADC,
        }, {
            "ADC",
            0x73,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
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
            "AND",
            0x32,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x27,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x35,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x21,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x31,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x37,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x2D,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x2F,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x3D,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x39,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x3F,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x23,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleAND,
        }, {
            "AND",
            0x33,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleAND,
        }, {
            "ASL",
            0x0A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleASL_A,
        }, {
            "ASL",
            0x06,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleASL,
        }, {
            "ASL",
            0x16,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleASL,
        }, {
            "ASL",
            0x0E,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleASL,
        }, {
            "ASL",
            0x1E,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleASL,
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
            "BCS",
            0xB0,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBCS,
        }, {
            "BIT",
            0x89,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleBITImmediate,
        }, {
            "BIT",
            0x24,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleBIT,
        }, {
            "BIT",
            0x34,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleBIT,
        }, {
            "BIT",
            0x2C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleBIT,
        }, {
            "BIT",
            0x3C,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleBIT,
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
            "BRL",
            0x82,
            Cpu65816::AddressingMode::PcRelativeLong,
            &Cpu65816::handleBRL,
        }, {
            "BVC",
            0x50,
            Cpu65816::AddressingMode::PcRelative,
            &Cpu65816::handleBVC,
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
            "CLD",
            0xD8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleCLD,
        }, {
            "CLI",
            0x58,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleCLI,
        }, {
            "CLV",
            0xB8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleCLV,
        }, {
            "CMP",
            0xC9,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleCMPImmediate,
        }, {
            "CMP",
            0xC5,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD2,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xC7,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xC1,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD1,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD5,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD7,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xCD,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xCF,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xDD,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD9,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xDF,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xC3,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleCMP,
        }, {
            "CMP",
            0xD3,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleCMP,
        }, {
            "CPX",
            0xE0,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleCPXImmediate,
        }, {
            "CPX",
            0xE4,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleCPX,
        }, {
            "CPX",
            0xEC,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleCPX,
        }, {
            "CPY",
            0xC0,
            Cpu65816::AddressingMode::ImmediateIndex,
            &Cpu65816::handleCPYImmediate,
        }, {
            "CPY",
            0xC4,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleCPY,
        }, {
            "CPY",
            0xCC,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleCPY,
        }, {
            "DEC",
            0x3A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleDEC_A,
        }, {
            "DEC",
            0xC6,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleDEC,
        }, {
            "DEC",
            0xD6,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleDEC,
        }, {
            "DEC",
            0xCE,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleDEC,
        }, {
            "DEC",
            0xDE,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
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
            "EOR",
            0x45,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x52,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x41,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x51,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x47,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x57,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x55,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x4D,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x4F,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x5F,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x5D,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x59,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x43,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x53,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleEOR,
        }, {
            "EOR",
            0x49,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleEORImmediate,
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
            0xF6,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleINC,
        }, {
            "INC",
            0xEE,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleINC,
        }, {
            "INC",
            0xFE,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
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
            0x6C,
            Cpu65816::AddressingMode::AbsoluteIndirect,
            &Cpu65816::handleJMP,
        }, {
            "JMP",
            0x7C,
            Cpu65816::AddressingMode::AbsoluteJMPIndirectIndexedX,
            &Cpu65816::handleJMP,
        }, {
            "JMP",
            0xDC,
            Cpu65816::AddressingMode::AbsoluteIndirectLong,
            &Cpu65816::handleJMP,
        }, {
            "JMP",
            0x4C,
            Cpu65816::AddressingMode::AbsoluteJMP,
            &Cpu65816::handleJMP,
        }, {
            "JSL",
            0x22,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleJSL,
        }, {
            "JSR",
            0x20,
            Cpu65816::AddressingMode::AbsoluteJMP,
            &Cpu65816::handleJSR,
        }, {
            "JSR",
            0xFC,
            Cpu65816::AddressingMode::AbsoluteJMPIndirectIndexedX,
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
            0xAF,
            Cpu65816::AddressingMode::AbsoluteLong,
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
            0xB5,
            Cpu65816::AddressingMode::DpIndexedX,
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
            0xA1,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xB1,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xBF,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xA3,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleLDA,
        }, {
            "LDA",
            0xB3,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
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
            0xBE,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleLDX,
        }, {
            "LDX",
            0xA6,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleLDX,
        }, {
            "LDX",
            0xB6,
            Cpu65816::AddressingMode::DpIndexedY,
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
            "LDY",
            0xB4,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleLDY,
        }, {
            "LDY",
            0xAC,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleLDY,
        }, {
            "LDY",
            0xBC,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleLDY,
        }, {
            "LSR",
            0x4A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleLSR_A,
        }, {
            "LSR",
            0x46,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleLSR,
        }, {
            "LSR",
            0x4E,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleLSR,
        }, {
            "LSR",
            0x56,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleLSR,
        }, {
            "LSR",
            0x5E,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleLSR,
        }, {
            "MVN",
            0x54,
            Cpu65816::AddressingMode::BlockMove,
            &Cpu65816::handleMVN,
            false,
        }, {
            "NOP",
            0xEA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleNOP,
        }, {
            "ORA",
            0x09,
            Cpu65816::AddressingMode::ImmediateA,
            &Cpu65816::handleORAImmediate,
        }, {
            "ORA",
            0x05,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x15,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x12,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x01,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x11,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x17,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x0D,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x0F,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x1F,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x19,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
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
            "ORA",
            0x03,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleORA,
        }, {
            "ORA",
            0x13,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleORA,
        }, {
            "PEA",
            0xF4,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handlePEA,
        }, {
            "PEI",
            0xD4,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handlePEA,
        }, {
            "PER",
            0x62,
            Cpu65816::AddressingMode::PcRelativeLong,
            &Cpu65816::handlePER,
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
            "ROL",
            0x26,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleROL,
        }, {
            "ROL",
            0x36,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleROL,
        }, {
            "ROL",
            0x2E,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleROL,
        }, {
            "ROL",
            0x3E,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleROL,
        }, {
            "ROR",
            0x6A,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleROR_A,
        }, {
            "ROR",
            0x6E,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleROR,
        }, {
            "ROR",
            0x7E,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleROR,
        }, {
            "ROR",
            0x66,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleROR,
        }, {
            "ROR",
            0x76,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleROR,
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
            &Cpu65816::handleSBCImmediate,
        }, {
            "SBC",
            0xE5,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF2,
            Cpu65816::AddressingMode::DpIndirect,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xE7,
            Cpu65816::AddressingMode::DpIndirectLong,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF7,
            Cpu65816::AddressingMode::DpIndirectLongIndexedY,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xE1,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF1,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF5,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xED,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xFD,
            Cpu65816::AddressingMode::AbsoluteIndexedX,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF9,
            Cpu65816::AddressingMode::AbsoluteIndexedY,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xEF,
            Cpu65816::AddressingMode::AbsoluteLong,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xFF,
            Cpu65816::AddressingMode::AbsoluteLongIndexedX,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xE3,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleSBC,
        }, {
            "SBC",
            0xF3,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleSBC,
        }, {
            "SEC",
            0x38,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleSEC,
        }, {
            "SED",
            0xF8,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleSED,
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
            0x95,
            Cpu65816::AddressingMode::DpIndexedX,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x81,
            Cpu65816::AddressingMode::DpIndirectIndexedX,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x91,
            Cpu65816::AddressingMode::DpIndirectIndexedY,
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
            0x92,
            Cpu65816::AddressingMode::DpIndirect,
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
            "STA",
            0x83,
            Cpu65816::AddressingMode::StackRelative,
            &Cpu65816::handleSTA,
        }, {
            "STA",
            0x93,
            Cpu65816::AddressingMode::StackRelativeIndirectIndexedY,
            &Cpu65816::handleSTA,
        }, {
            "STX",
            0x86,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleSTX,
        }, {
            "STX",
            0x96,
            Cpu65816::AddressingMode::DpIndexedY,
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
            0x94,
            Cpu65816::AddressingMode::DpIndexedX,
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
            "TDC",
            0x7B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTDC,
        }, {
            "TRB",
            0x14,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleTRB,
        }, {
            "TRB",
            0x1C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleTRB,
        }, {
            "TSB",
            0x04,
            Cpu65816::AddressingMode::Dp,
            &Cpu65816::handleTSB,
        }, {
            "TSB",
            0x0C,
            Cpu65816::AddressingMode::Absolute,
            &Cpu65816::handleTSB,
        }, {
            "TSC",
            0x3B,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTSC,
        }, {
            "TSX",
            0xBA,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleTSX,
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
        }, {
            "WAI",
            0xCB,
            Cpu65816::AddressingMode::Implied,
            &Cpu65816::handleWAI,
        },
    };

    for (size_t i = 0; i < SIZEOF_ARRAY(s_OpcodeList); i++) {
        m_Opcodes[s_OpcodeList[i].m_Value] = s_OpcodeList[i];
    }

    LOGI(TAG, "%zu opcodes registered", SIZEOF_ARRAY(s_OpcodeList));

    // Load adressing modes
    static const struct {
        AddressingMode mode;
        AddressingModeHandler handler;
    } s_AdressingModeList[] = {
        { AddressingMode::Implied, &Cpu65816::handleImplied },
        { AddressingMode::Immediate, &Cpu65816::handleImmediate },
        { AddressingMode::ImmediateA, &Cpu65816::handleImmediateA },
        { AddressingMode::ImmediateIndex, &Cpu65816::handleImmediateIndex },
        { AddressingMode::Absolute, &Cpu65816::handleAbsolute },
        { AddressingMode::AbsoluteJMP, &Cpu65816::handleAbsoluteJMP },
        { AddressingMode::AbsoluteJMPIndirectIndexedX, &Cpu65816::handleAbsoluteJMPIndirectIndexedX },
        { AddressingMode::AbsoluteIndexedX, &Cpu65816::handleAbsoluteIndexedX },
        { AddressingMode::AbsoluteIndexedY, &Cpu65816::handleAbsoluteIndexedY },
        { AddressingMode::AbsoluteLong, &Cpu65816::handleAbsoluteLong },
        { AddressingMode::AbsoluteIndirect, &Cpu65816::handleAbsoluteIndirect },
        { AddressingMode::AbsoluteIndirectLong, &Cpu65816::handleAbsoluteIndirectLong },
        { AddressingMode::AbsoluteLongIndexedX, &Cpu65816::handleAbsoluteLongIndexedX },
        { AddressingMode::Dp, &Cpu65816::handleDp },
        { AddressingMode::DpIndexedX, &Cpu65816::handleDpIndexedX },
        { AddressingMode::DpIndexedY, &Cpu65816::handleDpIndexedY },
        { AddressingMode::DpIndirect, &Cpu65816::handleDpIndirect },
        { AddressingMode::DpIndirectIndexedX, &Cpu65816::handleDpIndirectIndexedX },
        { AddressingMode::DpIndirectIndexedY, &Cpu65816::handleDpIndirectIndexedY },
        { AddressingMode::DpIndirectLong, &Cpu65816::handleDpIndirectLong },
        { AddressingMode::DpIndirectLongIndexedY, &Cpu65816::handleDpIndirectLongIndexedY },
        { AddressingMode::PcRelative, &Cpu65816::handlePcRelative },
        { AddressingMode::PcRelativeLong, &Cpu65816::handlePcRelativeLong },
        { AddressingMode::StackRelative, &Cpu65816::handleStackRelative },
        { AddressingMode::StackRelativeIndirectIndexedY, &Cpu65816::handleStackRelativeIndirectIndexedY },
        { AddressingMode::BlockMove, &Cpu65816::handleBlockMove },
    };

    for (size_t i = 0; i < SIZEOF_ARRAY(s_AdressingModeList); i++) {
        const auto& mode = s_AdressingModeList[i];
        m_AddressingModes[enumToInt(mode.mode)] = mode.handler;
    }
}

void Cpu65816::executeSingle()
{
    // Check if NMI has been raised
    if (m_NMI) {
        handleNMI();
        m_NMI = false;
        m_WaitInterrupt = false;
    }

    if (m_WaitInterrupt) {
        return;
    }

    // Debug stuff
    char strIntruction[32];
    uint32_t opcodePC = (m_Registers.PB << 16) | m_Registers.PC;

    // Load opcode
    uint8_t opcode = m_Membus->readU8(opcodePC);
    const auto& opcodeDesc = m_Opcodes[opcode];

    if (!opcodeDesc.m_Name) {
        LOGC(TAG, "Unknown instruction detected");
        LOGC(TAG, "Last %zu executed instructions", kInstructionsLogSize);
        printInstructionsLog();
        LOGC(TAG, "Unknown opcode 0x%02X (Address %06X)", opcode, opcodePC);
        assert(false);
    }

    if (opcodeDesc.m_AutoStepPC) {
        m_Registers.PC++;
    }

    // Load data
    uint32_t data = 0;
    const auto addressingModeHandler = m_AddressingModes[enumToInt(opcodeDesc.m_AddressingMode)];
    (this->*addressingModeHandler)(opcodeDesc, strIntruction, &data);

    // Log instruction
    logInstruction(opcodePC, strIntruction);

    // Execute instruction
    assert(opcodeDesc.m_OpcodeHandler);
    (this->*opcodeDesc.m_OpcodeHandler)(data);
}

void Cpu65816::logInstruction(uint32_t opcodePC, const char* strIntruction)
{
    char s[256];

    snprintf(s, sizeof(s), "%06X %-32s A:%04X X:%04X Y:%04X S:%04X D:%04X DB:%02X P:%02X",
        opcodePC,
        strIntruction,
        m_Registers.A,
        m_Registers.X,
        m_Registers.Y,
        m_Registers.S,
        m_Registers.D,
        m_Registers.DB,
        m_Registers.P);

    m_InstructionsLog.push_back(s);

    while (m_InstructionsLog.size() > kInstructionsLogSize) {
        m_InstructionsLog.pop_front();
    }

    //LOGC(TAG, "%s", s);
}

void Cpu65816::printInstructionsLog() const
{
    for (const auto& s: m_InstructionsLog) {
        LOGE(TAG, "\t%s", s.c_str());
    }
}

void Cpu65816::handleNMI()
{
    // Bank is forced at 0
    uint16_t handlerAddress = m_Membus->readU16(kRegIV_NMI);
    if (!handlerAddress) {
        return;
    }

    // Save registers
    m_Membus->writeU8(m_Registers.S, m_Registers.PB);
    m_Registers.S--;

    m_Membus->writeU16(m_Registers.S - 1, m_Registers.PC & 0xFFFF);
    m_Registers.S -= 2;

    m_Membus->writeU8(m_Registers.S, m_Registers.P);
    m_Registers.S--;

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

void Cpu65816::setCFlag(int32_t value)
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

void Cpu65816::handleASL(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);
    uint32_t finalC;

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t v = m_Membus->readU8(data);

        finalC = v >> 7;
        v <<= 1;

        m_Membus->writeU8(data, v);
        setNZFlags(v, 0x80);
    } else {
        uint16_t v = m_Membus->readU16(data);

        finalC = v >> 15;
        v <<= 1;

        m_Membus->writeU16(data, v);
        setNZFlags(v, 0x8000);
    }

    if (finalC) {
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

void Cpu65816::handleBCS(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_C)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBEQ(uint32_t data)
{
    if (getBit(m_Registers.P, kPRegister_Z)) {
        m_Registers.PC = data;
    }
}

void Cpu65816::handleBITImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t result = (m_Registers.A & 0xFF) & data;

        setZFlag(result);

    } else {
        uint16_t result = m_Registers.A & data;

        setZFlag(result);
    }
}

void Cpu65816::handleBIT(uint32_t address)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t data = m_Membus->readU8(address);
        uint8_t result = (m_Registers.A & 0xFF) & data;

        setZFlag(result);

        if (data & (1 << 7)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_N);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_N);
        }

        if (data & (1 << 6)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }
    } else {
        uint16_t data = m_Membus->readU16(address);
        uint16_t result = m_Registers.A & data;

        setZFlag(result);

        if (data & (1 << 15)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_N);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_N);
        }

        if (data & (1 << 14)) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }
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

void Cpu65816::handleBRL(uint32_t data)
{
    m_Registers.PB = data >> 16;
    m_Registers.PC = data & 0xFFFF;
}

void Cpu65816::handleBVC(uint32_t data)
{
    if (!getBit(m_Registers.P, kPRegister_V)) {
        m_Registers.PC = data;
    }
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

void Cpu65816::handleCLD(uint32_t data)
{
    m_Registers.P = clearBit(m_Registers.P, kPRegister_D);
}

void Cpu65816::handleCLI(uint32_t data)
{
    m_Registers.P = clearBit(m_Registers.P, kPRegister_I);
}

void Cpu65816::handleCLV(uint32_t data)
{
    m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
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

void Cpu65816::handleCPX(uint32_t data)
{
    uint16_t negativeMask;
    auto indexSize = getBit(m_Registers.P, kPRegister_X);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        result = (m_Registers.X & 0xFF) - m_Membus->readU8(data);
        negativeMask = 0x80;
    } else {
        result  = m_Registers.X - m_Membus->readU16(data);
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

void Cpu65816::handleCPY(uint32_t data)
{
    uint16_t negativeMask;
    auto indexSize = getBit(m_Registers.P, kPRegister_X);
    int32_t result;

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        result = (m_Registers.Y & 0xFF) - m_Membus->readU8(data);
        negativeMask = 0x80;
    } else {
        result  = m_Registers.Y - m_Membus->readU16(data);
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

void Cpu65816::handleDEC_A(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A - 1) & 0xFF);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A--;
        setNZFlags(m_Registers.A, 0x8000);
    }
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

void Cpu65816::handleEOR(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = m_Membus->readU8(data);
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) ^ value);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A ^= m_Membus->readU16(data);
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleEORImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = data;
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) ^ value);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A ^= data;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleINC_A(uint32_t data)
{
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
    m_Registers.PB = data >> 16;
    m_Registers.PC = data & 0xFFFF;
}

void Cpu65816::handleJSR(uint32_t data)
{
    m_Membus->writeU16(m_Registers.S - 1, m_Registers.PC - 1);
    m_Registers.S -= 2;

    m_Registers.PC = data;
}

void Cpu65816::handleJSL(uint32_t data)
{
    m_Membus->writeU8(m_Registers.S, m_Registers.PB);
    m_Registers.S--;

    m_Membus->writeU16(m_Registers.S - 1, (m_Registers.PC & 0xFFFF) - 1);
    m_Registers.S -= 2;

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

void Cpu65816::handleLSR_A(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Registers.A & 0xFF;

        C = v & 1;
        v >>= 1;

        m_Registers.A = (m_Registers.A & 0xFF00) | (v & 0xFF);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        uint32_t v = m_Registers.A;

        C = v & 1;
        v >>= 1;

        m_Registers.A = v & 0xFFFF;
        setNZFlags(m_Registers.A, 0x8000);
    }

    if (C) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleLSR(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Membus->readU8(data);

        C = v & 1;
        v >>= 1;

        m_Membus->writeU8(data, v);
        setNZFlags(v, 0x80);
    } else {
        uint32_t v = m_Membus->readU16(data);

        C = v & 1;
        v >>= 1;

        m_Membus->writeU16(data, v);
        setNZFlags(v, 0x8000);
    }

    if (C) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleMVN(uint32_t data)
{
    uint8_t srcBank = data >> 8;
    m_Registers.DB = data & 0xFF;

    uint32_t srcAddr = (srcBank << 16) | m_Registers.X;
    uint32_t destAddr = (m_Registers.DB << 16) | m_Registers.Y;
    m_Membus->writeU8(destAddr, m_Membus->readU8(srcAddr));

    m_Registers.A--;
    m_Registers.X++;
    m_Registers.Y++;

    if (m_Registers.A == 0xFFFF) {
        // Skip opcode + parameters
        m_Registers.PC += 3;
    }
}

void Cpu65816::handleNOP(uint32_t data)
{
}

void Cpu65816::handleORA(uint32_t data)
{
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

void Cpu65816::handleORAImmediate(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t value = data;
        m_Registers.A = (m_Registers.A & 0xFF00) | ((m_Registers.A & 0xFF) | value);
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A |= data;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handlePEA(uint32_t data)
{
    m_Membus->writeU16(m_Registers.S - 1, data & 0xFFFF);
    m_Registers.S -= 2;
}

void Cpu65816::handlePEI(uint32_t data)
{
    m_Membus->writeU16(m_Registers.S - 1, data & 0xFFFF);
    m_Registers.S -= 2;
}

void Cpu65816::handlePER(uint32_t data)
{
    m_Membus->writeU16(m_Registers.S - 1, data & 0xFFFF);
    m_Registers.S -= 2;
}

void Cpu65816::handlePHA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Membus->writeU8(m_Registers.S, m_Registers.A & 0xFF);
        m_Registers.S--;
    } else {
        m_Membus->writeU16(m_Registers.S - 1, m_Registers.A);
        m_Registers.S -= 2;
    }
}

void Cpu65816::handlePHB(uint32_t data)
{
    m_Membus->writeU8(m_Registers.S, m_Registers.DB);
    m_Registers.S--;
}

void Cpu65816::handlePHD(uint32_t data)
{
    m_Membus->writeU16(m_Registers.S - 1, m_Registers.D);
    m_Registers.S -= 2;
}

void Cpu65816::handlePHK(uint32_t data)
{
    m_Membus->writeU8(m_Registers.S, m_Registers.PB);
    m_Registers.S--;
}

void Cpu65816::handlePHP(uint32_t data)
{
    m_Membus->writeU8(m_Registers.S, m_Registers.P);
    m_Registers.S--;
}

void Cpu65816::handlePHX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Membus->writeU8(m_Registers.S, m_Registers.X);
        m_Registers.S--;
    } else {
        m_Membus->writeU16(m_Registers.S - 1, m_Registers.X);
        m_Registers.S -= 2;
    }
}

void Cpu65816::handlePHY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Membus->writeU8(m_Registers.S, m_Registers.Y);
        m_Registers.S--;
    } else {
        m_Membus->writeU16(m_Registers.S - 1, m_Registers.Y);
        m_Registers.S -= 2;
    }
}

void Cpu65816::handlePLA(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A = (m_Registers.A & 0xFF00) | m_Membus->readU8(m_Registers.S + 1);
        m_Registers.S++;
        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Membus->readU16(m_Registers.S + 1);
        m_Registers.S += 2;
        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handlePLB(uint32_t data)
{
    m_Registers.DB = m_Membus->readU8(m_Registers.S + 1);
    m_Registers.S++;

    setNZFlags(m_Registers.DB, 0x80);
}

void Cpu65816::handlePLD(uint32_t data)
{
    m_Registers.D = m_Membus->readU16(m_Registers.S + 1);
    m_Registers.S += 2;

    setNZFlags(m_Registers.D, 0x80);
}

void Cpu65816::handlePLP(uint32_t data)
{
    m_Registers.P = m_Membus->readU8(m_Registers.S + 1);
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
        m_Registers.X = m_Membus->readU8(m_Registers.S + 1);
        m_Registers.S++;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Membus->readU16(m_Registers.S + 1);
        m_Registers.S += 2;
        setNZFlags(m_Registers.X, 0x8000);
    }
}

void Cpu65816::handlePLY(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.Y = m_Membus->readU8(m_Registers.S + 1);
        m_Registers.S++;
        setNZFlags(m_Registers.Y & 0xFF, 0x80);
    } else {
        m_Registers.Y = m_Membus->readU16(m_Registers.S + 1);
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
        setNZFlags(m_Registers.A, 0x80);
    } else {
        uint32_t v = m_Registers.A;

        v = (v << 1) | C;
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

void Cpu65816::handleROL(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Membus->readU8(data);

        v = (v << 1) | C;
        C = v >> 8;

        v &= 0xFF;
        m_Membus->writeU8(data, v);
        setNZFlags(v, 0x80);
    } else {
        uint32_t v = m_Membus->readU16(data);

        v = (v << 1) | C;
        C = v >> 16;

        v &= 0xFFFF;
        m_Membus->writeU16(data, v);
        setNZFlags(v, 0x8000);
    }

    if (C) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleROR_A(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);
    uint32_t finalC;

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Registers.A & 0xFF;

        finalC = v & 1;
        v = (C << 7) | (v >> 1);

        m_Registers.A = (m_Registers.A & 0xFF00) | (v & 0xFF);
        setNZFlags(m_Registers.A, 0x80);
    } else {
        uint32_t v = m_Registers.A;

        finalC = v & 1;
        v = (C << 15) | (v >> 1);

        m_Registers.A = v & 0xFFFF;
        setNZFlags(m_Registers.A, 0x8000);
    }

    if (finalC) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleROR(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);
    uint32_t C = getBit(m_Registers.P, kPRegister_C);
    uint32_t finalC;

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint16_t v = m_Membus->readU8(data);

        finalC = v & 1;
        v = (C << 7) | (v >> 1);

        m_Membus->writeU8(data, v);
        setNZFlags(v, 0x80);
    } else {
        uint32_t v = m_Membus->readU16(data);

        finalC = v & 1;
        v = (C << 15) | (v >> 1);

        m_Membus->writeU16(data, v);
        setNZFlags(v, 0x8000);
    }

    if (finalC) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleRTI(uint32_t data)
{
    // Restore registers
    uint16_t P = m_Membus->readU8(m_Registers.S + 1);
    m_Registers.S++;

    uint16_t PC = m_Membus->readU16(m_Registers.S + 1);
    m_Registers.S += 2;

    uint16_t PB = m_Membus->readU8(m_Registers.S + 1);
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
    uint16_t PC = m_Membus->readU16(m_Registers.S + 1) + 1;
    m_Registers.S += 2;

    uint16_t PB = m_Membus->readU8(m_Registers.S + 1);
    m_Registers.S++;

    m_Registers.PC = PC;
    m_Registers.PB = PB;
}

void Cpu65816::handleRTS(uint32_t data)
{
    m_Registers.PC = m_Membus->readU16(m_Registers.S + 1) + 1;
    m_Registers.S += 2;
}

void Cpu65816::handleSBCImmediate(uint32_t rawData)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    assert(!getBit(m_Registers.P, kPRegister_D));

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t data = rawData;
        data = ~data;

        int result = (m_Registers.A & 0xFF) + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        if (~((m_Registers.A & 0xFF) ^ data) & ((m_Registers.A & 0xFF) ^ result) & 0x80) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        // C Flag
        if (result > 0xFF) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }

        // Z and N Flag
        setZFlag(result & 0xFF);
        setNFlag(result, 0x80);

        m_Registers.A = (m_Registers.A & 0xFF00) | (result & 0xFF);
    } else {
        uint16_t data = rawData;
        data = ~data;

        int result = m_Registers.A + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        if (~(m_Registers.A ^ data) & (m_Registers.A ^ result) & 0x8000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        // C Flag
        if (result > 0xFFFF) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }

        // Z and N Flag
        setZFlag(result & 0xFFFF);
        setNFlag(result, 0x8000);

        m_Registers.A = result & 0xFFFF;
    }
}

void Cpu65816::handleSBC(uint32_t address)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    assert(!getBit(m_Registers.P, kPRegister_D));

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t data = m_Membus->readU8(address);
        data = ~data;

        int result = (m_Registers.A & 0xFF) + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        if (~((m_Registers.A & 0xFF) ^ data) & ((m_Registers.A & 0xFF) ^ result) & 0x80) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        // C Flag
        if (result > 0xFF) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }

        // Z and N Flag
        setZFlag(result & 0xFF);
        setNFlag(result, 0x80);

        m_Registers.A = (m_Registers.A & 0xFF00) | (result & 0xFF);
    } else {
        uint16_t data = m_Membus->readU16(address);
        data = ~data;

        int result = m_Registers.A + data + getBit(m_Registers.P, kPRegister_C);

        // V flag
        if (~(m_Registers.A ^ data) & (m_Registers.A ^ result) & 0x8000) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_V);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_V);
        }

        // C Flag
        if (result > 0xFFFF) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_C);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
        }

        // Z and N Flag
        setZFlag(result & 0xFFFF);
        setNFlag(result, 0x8000);

        m_Registers.A = result & 0xFFFF;
    }
}

void Cpu65816::handleSEC(uint32_t data)
{
    m_Registers.P = setBit(m_Registers.P, kPRegister_C);
}

void Cpu65816::handleSED(uint32_t data)
{
    m_Registers.P = setBit(m_Registers.P, kPRegister_D);
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

void Cpu65816::handleTDC(uint32_t data)
{
    m_Registers.A = m_Registers.D;

    setNZFlags(m_Registers.A, 0x8000);
}

void Cpu65816::handleTRB(uint32_t address)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t data = m_Membus->readU8(address);

        // Set Z
        if ((data & (m_Registers.A & 0xFF)) == 0) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_Z);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_Z);
        }

        data &= ~(m_Registers.A & 0xFF);
        m_Membus->writeU8(address, data);
    } else {
        uint16_t data = m_Membus->readU16(address);

        // Set Z
        if ((data & m_Registers.A) == 0) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_Z);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_Z);
        }

        data &= ~m_Registers.A;
        m_Membus->writeU16(address, data);
    }
}

void Cpu65816::handleTSB(uint32_t address)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        uint8_t data = m_Membus->readU8(address);

        // Set Z
        if ((data & (m_Registers.A & 0xFF)) == 0) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_Z);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_Z);
        }

        data |= (m_Registers.A & 0xFF);
        m_Membus->writeU8(address, data);
    } else {
        uint16_t data = m_Membus->readU16(address);

        // Set Z
        if ((data & m_Registers.A) == 0) {
            m_Registers.P = setBit(m_Registers.P, kPRegister_Z);
        } else {
            m_Registers.P = clearBit(m_Registers.P, kPRegister_Z);
        }

        data |= m_Registers.A;
        m_Membus->writeU16(address, data);
    }
}

void Cpu65816::handleTSC(uint32_t data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        m_Registers.A &= 0xFF00;
        m_Registers.A |= m_Registers.S & 0xFF;

        setNZFlags(m_Registers.A & 0xFF, 0x80);
    } else {
        m_Registers.A = m_Registers.S;

        setNZFlags(m_Registers.A, 0x8000);
    }
}

void Cpu65816::handleTSX(uint32_t data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        m_Registers.X &= 0xFF00;
        m_Registers.X |= m_Registers.S & 0xFF;
        setNZFlags(m_Registers.X & 0xFF, 0x80);
    } else {
        m_Registers.X = m_Registers.S;
        setNZFlags(m_Registers.X, 0x8000);
    }
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

    setNZFlags(m_Registers.A & 0xFF, 0x80);
}

void Cpu65816::handleXCE(uint32_t data)
{
    uint32_t initialC = getBit(m_Registers.P, kPRegister_C);
    uint32_t initialE = getBit(m_Registers.P, kPRegister_E);

    if (initialC) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_E);
        m_Registers.P = setBit(m_Registers.P, kPRegister_M);

        m_Registers.P = setBit(m_Registers.P, kPRegister_X);
        m_Registers.X &= 0xFF;
        m_Registers.Y &= 0xFF;
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_E);
    }

    if (initialE) {
        m_Registers.P = setBit(m_Registers.P, kPRegister_C);
    } else {
        m_Registers.P = clearBit(m_Registers.P, kPRegister_C);
    }
}

void Cpu65816::handleWAI(uint32_t data)
{
    m_WaitInterrupt = true;
}

void Cpu65816::setNMI()
{
    m_NMI = true;
}


void Cpu65816::handleImplied(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    snprintf(strIntruction, kStrInstructionLen, "%s", opcodeDesc.m_Name);
}

void Cpu65816::handleImmediate(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    *data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 1;
    snprintf(strIntruction, kStrInstructionLen, "%s #$%02X", opcodeDesc.m_Name, *data);
}

void Cpu65816::handleImmediateA(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    auto accumulatorSize = getBit(m_Registers.P, kPRegister_M);

    // 0: 16 bits, 1: 8 bits
    if (accumulatorSize) {
        *data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 1;

        snprintf(strIntruction, kStrInstructionLen, "%s #$%02X", opcodeDesc.m_Name, *data);
    } else {
        *data = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        snprintf(strIntruction, kStrInstructionLen, "%s #$%04X", opcodeDesc.m_Name, *data);
    }
}

void Cpu65816::handleImmediateIndex(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    auto indexSize = getBit(m_Registers.P, kPRegister_X);

    // 0: 16 bits, 1: 8 bits
    if (indexSize) {
        *data = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 1;

        snprintf(strIntruction, kStrInstructionLen, "%s #$%02X", opcodeDesc.m_Name, *data);
    } else {
        *data = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
        m_Registers.PC += 2;

        snprintf(strIntruction, kStrInstructionLen, "%s #$%04X", opcodeDesc.m_Name, *data);
    }
}

void Cpu65816::handleAbsolute(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    *data = (m_Registers.DB << 16) | rawData;

    snprintf(strIntruction, kStrInstructionLen, "%s $%04X [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteJMP(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    *data = (m_Registers.PB << 16) | rawData;

    snprintf(strIntruction, kStrInstructionLen, "%s $%04X [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteJMPIndirectIndexedX(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    uint32_t address = (m_Registers.PB << 16) | rawData;
    address = (m_Registers.PB << 16) | m_Membus->readU16(address + m_Registers.X);

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s ($%04X,X) [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteIndexedX(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    *data = (m_Registers.DB << 16) | rawData;
    *data += m_Registers.X;

    snprintf(strIntruction, kStrInstructionLen, "%s $%04X,X [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteIndexedY(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    *data = (m_Registers.DB << 16) | rawData;
    *data += m_Registers.Y;

    snprintf(strIntruction, kStrInstructionLen, "%s $%04X,Y [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteLong(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    *data = m_Membus->readU24((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 3;

    snprintf(strIntruction, kStrInstructionLen, "%s $%06X ", opcodeDesc.m_Name, *data);
}

void Cpu65816::handleAbsoluteIndirect(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    uint32_t address = (m_Registers.DB << 16) | rawData;
    address = (m_Registers.DB << 16) | m_Membus->readU16(address);

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s [$%04X] [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteIndirectLong(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    uint32_t address = (m_Registers.DB << 16) | rawData;
    address = m_Membus->readU24(address);

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s [$%04X] [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleAbsoluteLongIndexedX(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint32_t rawData = m_Membus->readU24((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 3;

    *data = rawData + m_Registers.X;

    snprintf(strIntruction, kStrInstructionLen, "%s $%06X,X [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDp(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint32_t rawData =  m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    *data = m_Registers.D + rawData;

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndexedX(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint32_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    *data = m_Registers.D + rawData + m_Registers.X;

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X,X [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndexedY(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint32_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    *data = m_Registers.D + rawData + m_Registers.Y;

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X,Y [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndirect(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    uint32_t address = ((m_Registers.DB << 16) | (m_Registers.D + rawData));
    address = (m_Registers.DB << 16) | m_Membus->readU16(address);

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s ($%02X) [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndirectIndexedX(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    uint32_t address = ((m_Registers.DB << 16) | (m_Registers.D + rawData));
    address = ((m_Registers.DB << 16) | m_Membus->readU16(address)) + m_Registers.X;

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s ($%02X),X [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndirectIndexedY(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    uint32_t address = ((m_Registers.DB << 16) | (m_Registers.D + rawData));
    address = ((m_Registers.DB << 16) | m_Membus->readU16(address)) + m_Registers.Y;

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s ($%02X),Y [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndirectLong(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    uint32_t address = m_Registers.D + rawData;
    address = m_Membus->readU24(address);

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s [$%02X] [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleDpIndirectLongIndexedY(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC++;

    uint32_t address = m_Registers.D + rawData;
    address = m_Membus->readU24(address) + m_Registers.Y;

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s [$%02X],Y [%06X] ", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handlePcRelative(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 1;

    *data = m_Registers.PC + static_cast<int8_t>(rawData);
    *data |= m_Registers.PB << 16;

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handlePcRelativeLong(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint16_t rawData = m_Membus->readU16((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 2;

    *data = m_Registers.PC + rawData;
    *data |= m_Registers.PB << 16;

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleStackRelative(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 1;

    *data = m_Registers.S + static_cast<int8_t>(rawData);

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X,S [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleStackRelativeIndirectIndexedY(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    uint8_t rawData = m_Membus->readU8((m_Registers.PB << 16) | m_Registers.PC);
    m_Registers.PC += 1;

    uint32_t address = m_Registers.S + static_cast<int8_t>(rawData);
    address = m_Membus->readU16(address) + m_Registers.Y;

    *data = address;

    snprintf(strIntruction, kStrInstructionLen, "%s ($%02X,S),Y [%06X]", opcodeDesc.m_Name, rawData, *data);
}

void Cpu65816::handleBlockMove(
    const OpcodeDesc& opcodeDesc,
    char strIntruction[kStrInstructionLen],
    uint32_t* data)
{
    // PC isn't changed automatically, skip the opcode
    *data = m_Membus->readU16((m_Registers.PB << 16) | (m_Registers.PC + 1));

    snprintf(strIntruction, kStrInstructionLen, "%s $%02X, $%02X", opcodeDesc.m_Name, *data >> 8, *data & 0xFF);
}
