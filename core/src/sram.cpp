#include "msfce/core/log.h"
#include "sram.h"

#define TAG "sram"

namespace msfce::core {

Sram::Sram(size_t size)
    : BufferMemComponent(MemComponentType::sram, size), m_AddressMask(size - 1)
{
}

uint8_t Sram::readU8(uint32_t address)
{
    return BufferMemComponent::readU8(address & m_AddressMask);
}

void Sram::writeU8(uint32_t address, uint8_t value)
{
    BufferMemComponent::writeU8(address & m_AddressMask, value);
}

int Sram::save(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return -errno;
    }

    LOGI(TAG, "Saving to srm %s", path.c_str());
    dumpToFile(f);
    fclose(f);

    return 0;
}

int Sram::load(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return -errno;
    }

    LOGI(TAG, "Loading srm %s", path.c_str());
    loadFromFile(f);
    fclose(f);

    return 0;
}

} // namespace msfce::core
