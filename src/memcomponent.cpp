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
    : MemComponent(type)
{
    m_Data.resize(size);
}

BufferMemComponent::BufferMemComponent(MemComponentType type, std::vector<uint8_t>&& data)
    : MemComponent(type),
      m_Data(data)
{
}

uint8_t BufferMemComponent::readU8(uint32_t address)
{
    return m_Data[address];
}

void BufferMemComponent::writeU8(uint32_t address, uint8_t value)
{
    m_Data[address] = value;
}
