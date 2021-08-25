#pragma once

#include <type_traits>

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof((x)[0]))

namespace msfce::core {

template<typename T>
constexpr auto enumToInt(T t)
{
    return static_cast<typename std::underlying_type<T>::type>(t);
}

} // namespace msfce::core
