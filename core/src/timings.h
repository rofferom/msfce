#pragma once

namespace msfce::core {

// CPU cycles to Master clock rate
constexpr int kTimingCpuOneCycle = 6;

// ROM access rate
constexpr int kTimingRomSlowAccess = 8;
constexpr int kTimingRomFastAccess = 6;

// RAM access rate
constexpr int kTimingRamAccess = 8;

// Interrupt Vector
constexpr int kTimingIvAccess = 8;

// I/O ports
constexpr int kTimingIoFastAccess = 6;
constexpr int kTimingIoSlowAccess = 12;

// DMA
constexpr int kTimingDmaStart = 8;
constexpr int kTimingDmaAccess = 8;

// PPU
constexpr int kTimingPpuDot = 4;

} // namespace msfce::core
