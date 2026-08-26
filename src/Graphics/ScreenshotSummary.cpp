#include "IronGang/Graphics/ScreenshotSummary.hpp"

#include "IronGang/Core/AtomicFile.hpp"

#include <cstdlib>
#include <unordered_set>

namespace IronGang
{
    namespace
    {
        // Anti-aliasing and the HUD's blending leave near-sky pixels that are not "geometry".
        constexpr int kSkyTolerance = 6;

        [[nodiscard]] bool IsSky(std::uint8_t red, std::uint8_t green, std::uint8_t blue) noexcept
        {
            return std::abs(static_cast<int>(red) - static_cast<int>(kSkyClearRed)) <= kSkyTolerance &&
                   std::abs(static_cast<int>(green) - static_cast<int>(kSkyClearGreen)) <= kSkyTolerance &&
                   std::abs(static_cast<int>(blue) - static_cast<int>(kSkyClearBlue)) <= kSkyTolerance;
        }
    }

    ScreenshotSummary SummarizeScreenshot(const std::vector<std::uint8_t>& rgba, int width, int height)
    {
        ScreenshotSummary summary;
        if (width <= 0 || height <= 0)
        {
            return summary;
        }
        const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
        if (rgba.size() != expected)
        {
            return summary;
        }

        summary.width = width;
        summary.height = height;
        summary.pixelCount = expected / 4;

        std::unordered_set<std::uint32_t> colours;
        std::uint64_t redTotal = 0;
        std::uint64_t greenTotal = 0;
        std::uint64_t blueTotal = 0;
        std::uint64_t digest = 1469598103934665603ULL;

        for (std::size_t index = 0; index < expected; index += 4)
        {
            const std::uint8_t red = rgba[index];
            const std::uint8_t green = rgba[index + 1];
            const std::uint8_t blue = rgba[index + 2];

            redTotal += red;
            greenTotal += green;
            blueTotal += blue;
            if (!IsSky(red, green, blue))
            {
                ++summary.nonSkyPixels;
            }
            if (colours.size() < kDistinctColourCap)
            {
                colours.insert((static_cast<std::uint32_t>(red) << 16) |
                               (static_cast<std::uint32_t>(green) << 8) | blue);
            }
            for (std::size_t byte = 0; byte < 4; ++byte)
            {
                digest ^= rgba[index + byte];
                digest *= 1099511628211ULL;
            }
        }

        summary.distinctColours = colours.size();
        const double pixels = static_cast<double>(summary.pixelCount);
        summary.meanRed = static_cast<double>(redTotal) / pixels;
        summary.meanGreen = static_cast<double>(greenTotal) / pixels;
        summary.meanBlue = static_cast<double>(blueTotal) / pixels;
        summary.digest = digest;
        return summary;
    }

    bool ScreenshotLooksRendered(const ScreenshotSummary& summary, std::string& reason)
    {
        if (summary.pixelCount == 0)
        {
            reason = "the capture is empty (no pixels were read back)";
            return false;
        }
        const double nonSky = summary.NonSkyFraction();
        if (nonSky < 0.01)
        {
            reason = "the frame is nothing but sky -- no geometry or HUD was drawn";
            return false;
        }
        if (summary.distinctColours < 8)
        {
            reason = "the frame has fewer than eight distinct colours, which is a flat fill rather "
                     "than a rendered scene";
            return false;
        }
        return true;
    }

    bool WriteScreenshotSummary(const std::string& path,
                                const ScreenshotSummary& summary,
                                std::string& errorMessage)
    {
        std::string text;
        text += "{\n";
        text += "  \"width\": " + std::to_string(summary.width) + ",\n";
        text += "  \"height\": " + std::to_string(summary.height) + ",\n";
        text += "  \"pixelCount\": " + std::to_string(summary.pixelCount) + ",\n";
        text += "  \"nonSkyPixels\": " + std::to_string(summary.nonSkyPixels) + ",\n";
        text += "  \"nonSkyFraction\": " + std::to_string(summary.NonSkyFraction()) + ",\n";
        text += "  \"distinctColours\": " + std::to_string(summary.distinctColours) + ",\n";
        text += "  \"distinctColoursCapped\": " +
                std::string(summary.distinctColours >= kDistinctColourCap ? "true" : "false") + ",\n";
        text += "  \"meanRed\": " + std::to_string(summary.meanRed) + ",\n";
        text += "  \"meanGreen\": " + std::to_string(summary.meanGreen) + ",\n";
        text += "  \"meanBlue\": " + std::to_string(summary.meanBlue) + ",\n";
        text += "  \"digest\": \"" + std::to_string(summary.digest) + "\"\n";
        text += "}\n";
        return WriteTextFileAtomically(path, text, false, errorMessage);
    }
}
