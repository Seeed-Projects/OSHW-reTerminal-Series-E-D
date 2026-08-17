#pragma once

#include <stdint.h>

enum class DIYTextAlignment : uint8_t
{
    kCenter = 0,
    kLeft = 1,
    kRight = 2,
};

enum class DIYBodyPlacement : uint8_t
{
    kBelowIndex = 0,
    kRightOfIndex = 1,
};
