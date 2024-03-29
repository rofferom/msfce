#pragma once

#include <stdlib.h>
#include <stdint.h>

#include "memcomponent.h"

namespace msfce::core {

class Maths : public MemComponent {
public:
    Maths();
    ~Maths() = default;

    uint8_t readU8(uint32_t addr) override;
    void writeU8(uint32_t addr, uint8_t value) override;

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    uint16_t m_Multiplicand = 0;
    uint8_t m_Multiplier = 0;

    uint16_t m_Dividend = 0;
    uint8_t m_Divisor = 0;

    uint16_t m_Quotient = 0;
    uint16_t m_RemainderProduct = 0;
};

} // namespace msfce::core
