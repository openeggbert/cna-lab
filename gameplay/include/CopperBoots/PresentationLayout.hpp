#pragma once

#include "CopperBoots/GameSettings.hpp"

namespace CopperBoots
{
    struct PresentationViewport
    {
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;

        bool operator==(const PresentationViewport&) const = default;
    };

    [[nodiscard]] PresentationViewport ComputePresentationViewport(
        int outputWidth, int outputHeight, int logicalWidth,
        int logicalHeight, PresentationStyle style) noexcept;
}
