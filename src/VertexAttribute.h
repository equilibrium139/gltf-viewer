#pragma once

#include <cstdint>
#include <type_traits>

// TODO: support multiple tex coords?
enum class VertexAttribute : std::uint32_t
{
    POSITION = 1 << 0,
    TEXCOORD = 1 << 1,
    NORMAL = 1 << 2,
    WEIGHTS = 1 << 3,
    JOINTS = 1 << 4,
    MORPH_TARGET0_POSITION = 1 << 5,
    MORPH_TARGET1_POSITION = 1 << 6,
    MORPH_TARGET0_NORMAL = 1 << 7,
    MORPH_TARGET1_NORMAL = 1 << 8,
    TANGENT = 1 << 9,
    MORPH_TARGET0_TANGENT = 1 << 10,
    MORPH_TARGET1_TANGENT = 1 << 11,
    COLOR = 1 << 12,
};

inline constexpr VertexAttribute operator | (VertexAttribute lhs, VertexAttribute rhs)
{
    using T = std::underlying_type_t<VertexAttribute>;
    return (VertexAttribute)((T)lhs | (T)rhs);
}

inline constexpr VertexAttribute& operator |= (VertexAttribute& lhs, VertexAttribute rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline constexpr VertexAttribute operator & (VertexAttribute lhs, VertexAttribute rhs)
{
    using T = std::underlying_type_t<VertexAttribute>;
    return (VertexAttribute)((T)lhs & (T)rhs);
}

inline constexpr VertexAttribute& operator &= (VertexAttribute& lhs, VertexAttribute rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr VertexAttribute operator ~(VertexAttribute attr)
{
    using T = std::underlying_type_t<VertexAttribute>;
    return (VertexAttribute)~(T)(attr);
}

inline constexpr bool HasFlag(VertexAttribute flags, VertexAttribute flag_to_check)
{
    return (std::underlying_type_t<VertexAttribute>)(flags & flag_to_check) != 0;
}