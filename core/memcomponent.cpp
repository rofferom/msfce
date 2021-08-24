#include "memcomponent.h"

MemComponent::MemComponent(MemComponentType type)
    : m_Type(type)
{
}

MemComponentType MemComponent::getType() const
{
    return m_Type;
}

BufferMemComponent::BufferMemComponent(MemComponentType type, size_t size)
    : MemComponent(type),
      m_Size(size)
{
    m_Data.resize(size);
}

BufferMemComponent::BufferMemComponent(MemComponentType type, std::vector<uint8_t>&& data)
    : MemComponent(type),
      m_Data(data),
      m_Size(m_Data.size())
{
}

uint8_t BufferMemComponent::readU8(uint32_t address)
{
    return m_Data[address % m_Size];
}

void BufferMemComponent::writeU8(uint32_t address, uint8_t value)
{
    m_Data[address % m_Size] = value;
}

void BufferMemComponent::dumpToFile(FILE* f)
{
    fwrite(m_Data.data(), m_Data.size(), 1, f);
}

void BufferMemComponent::loadFromFile(FILE* f)
{
    fread(m_Data.data(), m_Data.size(), 1, f);
}
