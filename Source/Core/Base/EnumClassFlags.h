#pragma once
#include <type_traits>

// 仿 UE Core/Public/Misc/EnumClassFlags.h
// 为 enum class 生成位运算符，并提供 EnumHasAnyFlags / EnumHasAllFlags 等辅助。
// 用法：在 enum class 定义之后写一行  ENUM_CLASS_FLAGS(EMyFlags);
// 之后即可对该枚举用 | & ^ ~ |= &= ^= 以及下面的模板辅助函数。

#define ENUM_CLASS_FLAGS(Enum) \
    inline           Enum& operator|=(Enum& L, Enum R) { return L = (Enum)((std::underlying_type_t<Enum>)L | (std::underlying_type_t<Enum>)R); } \
    inline           Enum& operator&=(Enum& L, Enum R) { return L = (Enum)((std::underlying_type_t<Enum>)L & (std::underlying_type_t<Enum>)R); } \
    inline           Enum& operator^=(Enum& L, Enum R) { return L = (Enum)((std::underlying_type_t<Enum>)L ^ (std::underlying_type_t<Enum>)R); } \
    inline constexpr Enum  operator| (Enum  L, Enum R) { return (Enum)((std::underlying_type_t<Enum>)L | (std::underlying_type_t<Enum>)R); } \
    inline constexpr Enum  operator& (Enum  L, Enum R) { return (Enum)((std::underlying_type_t<Enum>)L & (std::underlying_type_t<Enum>)R); } \
    inline constexpr Enum  operator^ (Enum  L, Enum R) { return (Enum)((std::underlying_type_t<Enum>)L ^ (std::underlying_type_t<Enum>)R); } \
    inline constexpr Enum  operator~ (Enum  E)         { return (Enum)~(std::underlying_type_t<Enum>)E; } \
    inline constexpr bool  operator! (Enum  E)         { return !(std::underlying_type_t<Enum>)E; }

// 是否含任一 Contains 中的位
template <typename Enum>
constexpr bool EnumHasAnyFlags(Enum Flags, Enum Contains)
{
    using T = std::underlying_type_t<Enum>;
    return ((T)Flags & (T)Contains) != 0;
}

// 是否含 Contains 中的全部位
template <typename Enum>
constexpr bool EnumHasAllFlags(Enum Flags, Enum Contains)
{
    using T = std::underlying_type_t<Enum>;
    return ((T)Flags & (T)Contains) == (T)Contains;
}

// 加上 / 去掉某些位
template <typename Enum>
constexpr void EnumAddFlags(Enum& Flags, Enum FlagsToAdd)
{
    using T = std::underlying_type_t<Enum>;
    Flags = (Enum)((T)Flags | (T)FlagsToAdd);
}

template <typename Enum>
constexpr void EnumRemoveFlags(Enum& Flags, Enum FlagsToRemove)
{
    using T = std::underlying_type_t<Enum>;
    Flags = (Enum)((T)Flags & ~(T)FlagsToRemove);
}
