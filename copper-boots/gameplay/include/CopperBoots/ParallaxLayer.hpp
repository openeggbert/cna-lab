#pragma once

#include <cmath>
#include <cstdint>

namespace CopperBoots
{
    enum class ParallaxGeometry
    {
        BlockSilhouette,
        CloudBand,
    };

    struct RgbColor
    {
        std::uint8_t R;
        std::uint8_t G;
        std::uint8_t B;
    };

    struct ParallaxLayer
    {
        float ScrollFactor;
        float Depth;
        int Spacing;
        int Baseline;
        int MinimumHeight;
        RgbColor Tint;
        ParallaxGeometry Geometry;
        bool Repeating;
        bool Fixed;

        [[nodiscard]] int WrappedOffset(const float cameraX) const noexcept
        {
            if (Fixed || Spacing <= 0)
                return 0;
            int offset = static_cast<int>(std::floor(cameraX * ScrollFactor)) %
                         Spacing;
            if (offset < 0)
                offset += Spacing;
            return offset;
        }
    };
}
