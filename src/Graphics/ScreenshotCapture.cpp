#include "ScreenshotCapture.hpp"

#include "CNA/Internal/Graphics/ImageLoader.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

#include <exception>
#include <vector>

namespace IronGang
{
    namespace
    {
        namespace Graphics = Microsoft::Xna::Framework::Graphics;
        using Microsoft::Xna::Framework::Color;
    }

    bool CaptureScreenshot(Graphics::GraphicsDevice& device,
                           const std::string& path,
                           ScreenshotSummary& summary,
                           std::string& errorMessage)
    {
        try
        {
            const Graphics::PresentationParameters& presentation =
                device.getPresentationParametersProperty();
            const int width = presentation.getBackBufferWidthProperty();
            const int height = presentation.getBackBufferHeightProperty();
            if (width <= 0 || height <= 0)
            {
                errorMessage = "screenshot: the back buffer has no size yet";
                return false;
            }

            // Color carries a vtable pointer, so its bytes are not R,G,B,A -- unpack rather than
            // memcpy. (CNA's own GetBackBufferData has the same note for the same reason.)
            std::vector<Color> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            device.GetBackBufferData(pixels.data(), static_cast<int>(pixels.size()));

            std::vector<std::uint8_t> rgba(pixels.size() * 4);
            for (std::size_t index = 0; index < pixels.size(); ++index)
            {
                rgba[index * 4 + 0] = pixels[index].getRProperty();
                rgba[index * 4 + 1] = pixels[index].getGProperty();
                rgba[index * 4 + 2] = pixels[index].getBProperty();
                rgba[index * 4 + 3] = pixels[index].getAProperty();
            }

            CNA::Internal::Graphics::ImageLoader::SavePng(rgba.data(), width, height, path);
            summary = SummarizeScreenshot(rgba, width, height);
            return WriteScreenshotSummary(path + ".summary.json", summary, errorMessage);
        }
        catch (const std::exception& exception)
        {
            // A renderer with no ReadBackbuffer override raises rather than returning a code, and
            // the software and EasyGL renderers differ here -- the game must survive either.
            errorMessage = std::string("screenshot: ") + exception.what();
            return false;
        }
    }
}
