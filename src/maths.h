#pragma once

#include <stdlib.h>
#include <stdint.h>

class Maths {
public:
    Maths() = default;
    ~Maths() = default;

    uint8_t readU8(size_t addr);
    uint16_t readU16(size_t addr);

    void writeU8(size_t addr, uint8_t value);
    void writeU16(size_t addr, uint16_t value);

private:
    uint16_t m_Multiplicand = 0;
    uint8_t m_Multiplier = 0;

    uint16_t m_Dividend = 0;
    uint8_t m_Divisor = 0;

    uint16_t m_Quotient = 0;
    uint16_t m_RemainderProduct = 0;
};
