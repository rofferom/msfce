#pragma once

#include <string>
#include "memcomponent.h"

namespace msfce::core {

class Sram : public BufferMemComponent {
public:
    Sram(size_t size);

    uint8_t readU8(uint32_t address) override;
    void writeU8(uint32_t address, uint8_t value) override;

    int save(const std::string& path);
    int load(const std::string& path);

private:
    uint32_t m_AddressMask;
};

} // namespace msfce::core
