#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace IronGang
{
    // The colour Draw() clears the sky to. It lives here, rather than as a literal at the Clear()
    // call, because a screenshot check has to know which pixels mean "nothing was drawn here" --
    // and two copies of that number would drift the first time the sky is retuned.
    inline constexpr std::uint8_t kSkyClearRed = 112;
    inline constexpr std::uint8_t kSkyClearGreen = 145;
    inline constexpr std::uint8_t kSkyClearBlue = 164;

    // plan_30 IG-30-013: what a captured frame contains, reduced to numbers a test or a future
    // session can compare without a display, an image library, or a human eye.
    //
    // This is deliberately NOT a golden-image hash comparison. `--smoke N` is not frame-
    // deterministic (CNA drives Update() from the wall clock, so how far the world has advanced by
    // frame N varies between runs), and a strict hash would fail on every machine for reasons that
    // have nothing to do with a rendering regression. What is stable across runs is *whether the
    // frame looks rendered at all*: the sky is not the only colour, geometry covers a plausible
    // share of the screen, and the HUD's own bright text is present.
    struct ScreenshotSummary
    {
        int width{0};
        int height{0};
        std::size_t pixelCount{0};
        // Pixels differing from the sky clear colour by more than a small tolerance.
        std::size_t nonSkyPixels{0};
        // Distinct RGB triples, counted up to kDistinctColourCap and then saturating -- an exact
        // count over a million pixels costs more than the answer is worth.
        std::size_t distinctColours{0};
        double meanRed{0.0};
        double meanGreen{0.0};
        double meanBlue{0.0};
        // FNV-1a 64 over every RGBA byte. Two runs of the same frame agree only if the frames are
        // byte-identical, so this is useful for "did this change at all", not as a pass criterion.
        std::uint64_t digest{0};

        [[nodiscard]] double NonSkyFraction() const noexcept
        {
            return pixelCount == 0 ? 0.0 : static_cast<double>(nonSkyPixels) / static_cast<double>(pixelCount);
        }
    };

    inline constexpr std::size_t kDistinctColourCap = 4096;

    // @p rgba must hold width * height * 4 bytes in R,G,B,A order. A mismatched size yields a
    // zeroed summary, which ScreenshotLooksRendered() then rejects.
    [[nodiscard]] ScreenshotSummary SummarizeScreenshot(const std::vector<std::uint8_t>& rgba,
                                                        int width,
                                                        int height);

    // The regression predicate. False (with a human-readable @p reason) when the frame is one of
    // the failures a screenshot is worth taking to catch: an empty capture, a frame that is
    // nothing but sky (the renderer drew nothing at all), and a frame of one or two flat colours
    // (a shader or format failure that fills the screen).
    //
    // It deliberately does NOT reject a frame with no sky in it. The first real capture was the
    // intro cutscene's high establishing shot, which looks down at the street and is 99.7% non-sky
    // -- a correct frame this predicate called suspicious. A camera angle is not a rendering
    // fault, and a check that cries wolf on valid content is worse than no check.
    [[nodiscard]] bool ScreenshotLooksRendered(const ScreenshotSummary& summary, std::string& reason);

    // Writes the summary beside the PNG as JSON, so a later run can diff numbers rather than
    // pixels. Hand-written -- the schema is six numbers and pulling a JSON writer into the
    // renderer's dependency set for that would be worse.
    [[nodiscard]] bool WriteScreenshotSummary(const std::string& path,
                                              const ScreenshotSummary& summary,
                                              std::string& errorMessage);
}
