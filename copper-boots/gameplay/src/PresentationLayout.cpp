#include "CopperBoots/PresentationLayout.hpp"

#include <algorithm>
#include <cstdint>

namespace CopperBoots
{
    PresentationViewport ComputePresentationViewport(
        const int outputWidth, const int outputHeight,
        const int logicalWidth, const int logicalHeight,
        const PresentationStyle style) noexcept
    {
        if (outputWidth <= 0 || outputHeight <= 0 ||
            logicalWidth <= 0 || logicalHeight <= 0) {
            return {};
        }

        int width = 0;
        int height = 0;
        if (style == PresentationStyle::IntegerScale &&
            outputWidth >= logicalWidth && outputHeight >= logicalHeight) {
            const int scale = std::max(1,
                std::min(outputWidth / logicalWidth,
                         outputHeight / logicalHeight));
            width = logicalWidth * scale;
            height = logicalHeight * scale;
        }
        else {
            const std::int64_t widthLimitedHeight =
                static_cast<std::int64_t>(outputWidth) * logicalHeight;
            const std::int64_t heightLimitedWidth =
                static_cast<std::int64_t>(outputHeight) * logicalWidth;
            if (widthLimitedHeight <= heightLimitedWidth) {
                width = outputWidth;
                height = std::max(1, static_cast<int>(
                    widthLimitedHeight / logicalWidth));
            }
            else {
                height = outputHeight;
                width = std::max(1, static_cast<int>(
                    heightLimitedWidth / logicalHeight));
            }
        }
        return {(outputWidth - width) / 2, (outputHeight - height) / 2,
                width, height};
    }
}
