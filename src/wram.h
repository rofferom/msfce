#pragma once

#include <memory>

#include "memcomponent.h"

class Wram : public BufferMemComponent {
public:
    Wram();
};

class IndirectWram : public MemComponent {
public:
    IndirectWram(const std::shared_ptr<Wram>& wram);

    uint8_t readU8(uint32_t address) override;
    void writeU8(uint32_t address, uint8_t value) override;

    void dumpToFile(FILE* f);
    void loadFromFile(FILE* f);

private:
    std::shared_ptr<Wram> m_Wram;

    uint32_t m_Address = 0;
};
