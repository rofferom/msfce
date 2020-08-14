#pragma once

#include <stdlib.h>

// Interrupt vector (starts at 0xFFE0)
constexpr size_t kRegIV_NMI = 0xFFEA;

// PPU registers
constexpr size_t kRegPpuStart = 0x2100;
constexpr size_t kRegPpuEnd = 0x213F;

constexpr size_t kRegINIDISP = 0x2100;
constexpr size_t kRegSETINI = 0x2133;
constexpr size_t kRegNmitimen = 0x4200;
constexpr size_t kRegVTIMEL = 0x4209;
constexpr size_t kRegVTIMEH = 0x420A;
constexpr size_t kRegRDNMI = 0x4210;
constexpr size_t kRegTIMEUP = 0x4211;

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

// Misc Registers
constexpr size_t kRegisterHTIMEL = 0x4207;
constexpr size_t kRegisterHTIMEH = 0x4208;
constexpr size_t kRegisterVTIMEL = 0x4209;
constexpr size_t kRegisterVTIMEH = 0x420A;
constexpr size_t kRegisterMemsel = 0x420D;
