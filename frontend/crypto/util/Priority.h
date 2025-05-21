#pragma once

#include <cstdint>

enum class Priority : uint8_t
{
    High = 0,
    Normal = 1,
    Low = 2,
    Barrier = 3,
};
