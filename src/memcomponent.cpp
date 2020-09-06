#include "memcomponent.h"

MemComponent::MemComponent(MemComponentType type)
    : m_Type(type)
{
}

MemComponentType MemComponent::getType() const
{
    return m_Type;
}
