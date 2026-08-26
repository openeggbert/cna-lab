#pragma once

#include "IronGang/Graphics/ScreenshotSummary.hpp"

#include <string>

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace IronGang
{
    // plan_30 IG-30-013: reads the finished frame back out of the graphics device and writes it as
    // a PNG, plus a "<path>.summary.json" sidecar of the numbers ScreenshotSummary describes.
    //
    // Lives in the executable, not iron_gang_core, for the same reason PrototypeRenderer does: it
    // is the only part of this feature that needs a GraphicsDevice. The half worth unit-testing --
    // deciding whether a frame looks rendered -- is a pure function in the library.
    //
    // Must be called after the frame's last draw call and before Present(). Returns false with
    // @p errorMessage set; the caller decides whether that is fatal (it is not: a screenshot is a
    // diagnostic, and failing to take one should never take the game down with it).
    [[nodiscard]] bool CaptureScreenshot(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                         const std::string& path,
                                         ScreenshotSummary& summary,
                                         std::string& errorMessage);
}
