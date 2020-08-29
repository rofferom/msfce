#pragma once

#include <stdlib.h>

// Interrupt vector (starts at 0xFFE0)
constexpr size_t kRegIV_NMI = 0xFFEA;

// PPU registers
constexpr int kPpuDisplayWidth = 256;
constexpr int kPpuDisplayHeight = 224;

constexpr size_t kRegPpuStart = 0x2100;
constexpr size_t kRegPpuEnd = 0x213F;

constexpr size_t kRegINIDISP = 0x2100;
constexpr size_t kRegSETINI = 0x2133;
constexpr size_t kRegNmitimen = 0x4200;
constexpr size_t kRegVTIMEL = 0x4209;
constexpr size_t kRegVTIMEH = 0x420A;
constexpr size_t kRegRDNMI = 0x4210;
constexpr size_t kRegTIMEUP = 0x4211;

constexpr size_t kRegVMAIN = 0x2115;
constexpr size_t kRegVMADDL = 0x2116;
constexpr size_t kRegVMADDH = 0x2117;
constexpr size_t kRegVMDATAL = 0x2118;
constexpr size_t kRegVMDATAH = 0x2119;
constexpr size_t kRegRDVRAML = 0x2139;
constexpr size_t kRegRDVRAMH = 0x213A;

constexpr size_t kRegOBJSEL = 0x2101;

constexpr size_t kRegOAMADDL = 0x2102;
constexpr size_t kRegOAMADDH = 0x2103;

constexpr size_t kRegOAMDATA = 0x2104;

constexpr size_t kRegBGMODE = 0x2105;

constexpr size_t kRegBG1HOFS = 0x210D;
constexpr size_t kRegBG1VOFS = 0x210E;
constexpr size_t kRegBG2HOFS = 0x210F;
constexpr size_t kRegBG2VOFS = 0x2110;
constexpr size_t kRegBG3HOFS = 0x2111;
constexpr size_t kRegBG3VOFS = 0x2112;
constexpr size_t kRegBG4HOFS = 0x2113;
constexpr size_t kRegBG4VOFS = 0x2114;

constexpr size_t kRegMOSAIC = 0x2106;

constexpr size_t kRegBG1SC = 0x2107;
constexpr size_t kRegBG2SC = 0x2108;
constexpr size_t kRegBG3SC = 0x2109;
constexpr size_t kRegBG4SC = 0x210A;

constexpr size_t kRegBG12NBA = 0x210B;
constexpr size_t kRegBG34NBA = 0x210C;

constexpr size_t kRegWBGLOG = 0x212A;
constexpr size_t kRegWOBJLOG = 0x212B;

constexpr size_t kRegTMW = 0x212E;
constexpr size_t kRegTSW = 0x212F;

constexpr size_t kRegCOLDATA = 0x2132;

constexpr size_t kRegM7SEL = 0x211A;

constexpr size_t kRegCGADD = 0x2121;
constexpr size_t kRegCGDATA = 0x2122;
constexpr size_t kRegRDCGRAM = 0x213B;

constexpr size_t kRegW12SEL = 0x2123;
constexpr size_t kRegW34SEL = 0x2124;
constexpr size_t kRegWOBJSEL = 0x2125;

constexpr size_t kRegTM = 0x212C;
constexpr size_t kRegTS = 0x212D;

constexpr size_t kRegCGWSEL = 0x2130;
constexpr size_t kRegCGADSUB = 0x2131;

// APU registers
constexpr size_t kRegApuStart = 0x2140;
constexpr size_t kRegApuEnd = 0x2143;

constexpr size_t kRegApuPort0 = 0x2140;
constexpr size_t kRegApuPort1 = 0x2141;
constexpr size_t kRegApuPort2 = 0x2142;
constexpr size_t kRegApuPort3 = 0x2143;

// WRAM registers
constexpr size_t kWramBankStart = 0x7E;
constexpr size_t kWramBankEnd = 0x7F;

constexpr size_t kRegisterWMDATA = 0x2180;
constexpr size_t kRegisterWMADDL = 0x2181;
constexpr size_t kRegisterWMADDM = 0x2182;
constexpr size_t kRegisterWMADDH = 0x2183;

// SRAM registers
constexpr size_t kSramBankStart = 0x70;
constexpr size_t kSramBankEnd = 0x7D;

// Joypad registers
constexpr size_t kRegisterJoyWr = 0x4016;

constexpr size_t kRegisterJoyA = 0x4016;
constexpr size_t kRegisterJoyB = 0x4017;

constexpr size_t kRegisterJoyWrio = 0x4201;

constexpr size_t kRegisterJoy1L = 0x4218;
constexpr size_t kRegisterJoy1H = 0x4219;

constexpr size_t kRegisterJoy2L = 0x421A;
constexpr size_t kRegisterJoy2H = 0x421B;

// Maths registers
constexpr size_t kRegisterWRMPYA = 0x4202;
constexpr size_t kRegisterWRMPYB = 0x4203;

constexpr size_t kRegisterWRDIVL = 0x4204;
constexpr size_t kRegisterWRDIVH = 0x4205;
constexpr size_t kRegisterWRDIVB = 0x4206;

constexpr size_t kRegisterRDDIVL = 0x4214;
constexpr size_t kRegisterRDDIVH = 0x4215;

constexpr size_t kRegisterRDMPYL = 0x4216;
constexpr size_t kRegisterRDMPYH = 0x4217;

// DMA/HDMA registers
constexpr size_t kRegisterMDMAEN = 0x420B;
constexpr size_t kRegisterHDMAEN = 0x420C;

constexpr size_t kRegDmaStart = 0x4300;
constexpr size_t kRegDmaEnd = 0x437F;

constexpr size_t kRegDmaP = 0x0;

constexpr size_t kRegDmaBBAD = 0x1;

constexpr size_t kRegDmaA1TL = 0x2;
constexpr size_t kRegDmaA1TH = 0x3;
constexpr size_t kRegDmaA1B = 0x4;

constexpr size_t kRegDmaDASL = 0x5;
constexpr size_t kRegDmaDASH = 0x6;
constexpr size_t kRegDmaDASB = 0x7;

constexpr size_t kRegDmaA2AL = 0x8;
constexpr size_t kRegDmaA2AH = 0x9;

constexpr size_t kRegDmaNTRL = 0xA;

// Misc Registers
constexpr size_t kRegisterHTIMEL = 0x4207;
constexpr size_t kRegisterHTIMEH = 0x4208;
constexpr size_t kRegisterVTIMEL = 0x4209;
constexpr size_t kRegisterVTIMEH = 0x420A;
constexpr size_t kRegisterMemsel = 0x420D;
