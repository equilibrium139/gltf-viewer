#pragma once

#include <cstdint>
#include <type_traits>

enum class VertexAttribute : std::uint32_t
{
    POSITION = 1 << 0,
    NORMAL = 1 << 1,
    MORPH_TARGET0_POSITION = 1 << 2,
    MORPH_TARGET1_POSITION = 1 << 3,
    MORPH_TARGET0_NORMAL = 1 << 4,
    MORPH_TARGET1_NORMAL = 1 << 5,
};

inline VertexAttribute operator | (VertexAttribute lhs, VertexAttribute rhs)
{
    using T = std::underlying_type_t<VertexAttribute>;
    return (VertexAttribute)((T)lhs | (T)rhs);
}

inline VertexAttribute& operator |= (VertexAttribute& lhs, VertexAttribute rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline VertexAttribute operator & (VertexAttribute lhs, VertexAttribute rhs)
{
    using T = std::underlying_type_t<VertexAttribute>;
    return (VertexAttribute)((T)lhs & (T)rhs);
}

inline VertexAttribute& operator &= (VertexAttribute& lhs, VertexAttribute rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline bool HasFlag(VertexAttribute flags, VertexAttribute flag_to_check)
{
    return (std::underlying_type_t<VertexAttribute>)(flags & flag_to_check) != 0;
}