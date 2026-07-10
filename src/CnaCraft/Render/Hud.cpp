#include "Hud.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include "BitmapFont.hpp"

using Microsoft::Xna::Framework::Color;
using Microsoft::Xna::Framework::Rectangle;
using Microsoft::Xna::Framework::Vector2;
using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
using Microsoft::Xna::Framework::Graphics::SpriteBatch;
using Microsoft::Xna::Framework::Graphics::Texture2D;

namespace CnaCraft::Render {

namespace {

constexpr int kCrosshairSize = 16;

Texture2D BuildCrosshairTexture(GraphicsDevice& device) {
    std::vector<Color> pixels(
        static_cast<std::size_t>(kCrosshairSize) * kCrosshairSize, Color(0, 0, 0, 0));
    for (int i = 0; i < kCrosshairSize; ++i) {
        for (int t = 0; t < 2; ++t) {
            pixels[static_cast<std::size_t>((7 + t) * kCrosshairSize + i)] = Color(255, 255, 255, 230);
            pixels[static_cast<std::size_t>(i * kCrosshairSize + (7 + t))] = Color(255, 255, 255, 230);
        }
    }
    Texture2D texture(device, kCrosshairSize, kCrosshairSize);
    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
    return texture;
}

// Text scale 3x (24px-effective glyphs) — see FontDrawText. Showing all 13
// slot names on one line (the original approach) forced a very wide native
// texture, which then had to be scaled back down to fit the screen width —
// undoing most of the size gain and leaving the text just as small as
// before. Showing only the currently-selected item keeps the content short
// enough to render genuinely large at any resolution; width/height below
// fit the worst case ("[FLYING] #13 Cobblestone") with margin.
constexpr int kHotbarTextScale = 3;
constexpr int kHotbarWidth = 650;
constexpr int kHotbarHeight = 32;

constexpr int kTypingTextScale = 2;
constexpr int kTypingWidth = 650;
constexpr int kTypingHeight = 28;

}

Hud::Hud(GraphicsDevice& device)
    : spriteBatch_(std::make_unique<SpriteBatch>(device)),
      crosshairTexture_(BuildCrosshairTexture(device)),
      hotbarTexture_(device, kHotbarWidth, kHotbarHeight),
      typingTexture_(device, kTypingWidth, kTypingHeight) {
    std::vector<std::uint8_t> blank(static_cast<std::size_t>(kHotbarWidth) * kHotbarHeight * 4, 0);
    hotbarTexture_.SetDataRGBA(blank.data(), kHotbarWidth * kHotbarHeight);
    std::vector<std::uint8_t> typingBlank(static_cast<std::size_t>(kTypingWidth) * kTypingHeight * 4, 0);
    typingTexture_.SetDataRGBA(typingBlank.data(), kTypingWidth * kTypingHeight);
}

void Hud::RebuildHotbar(GraphicsDevice&, const std::string* slotNames, int slotCount,
                        int selectedIndex, bool flying) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(kHotbarWidth) * kHotbarHeight * 4, 0);

    std::string text;
    if (flying) text += "[FLYING] ";
    if (selectedIndex >= 0 && selectedIndex < slotCount) {
        text += "#" + std::to_string(selectedIndex + 1) + "/" + std::to_string(slotCount) + " "
              + slotNames[selectedIndex];
    }
    FontDrawText(px, kHotbarWidth, kHotbarHeight, 2, 4, text, 255, 230, 60, 255, kHotbarTextScale);

    hotbarTexture_.SetDataRGBA(px.data(), kHotbarWidth * kHotbarHeight);
}

void Hud::SetTyping(GraphicsDevice&, bool active, const std::string& text) {
    typingVisible_ = active;
    if (!active) return;

    std::vector<std::uint8_t> px(static_cast<std::size_t>(kTypingWidth) * kTypingHeight * 4, 0);
    // Semi-transparent backing so the text stays legible over any terrain.
    for (int i = 0; i < kTypingWidth * kTypingHeight; ++i) {
        px[static_cast<std::size_t>(i) * 4 + 3] = 160;
    }
    const std::string display = "Sign: " + text + "_";
    FontDrawText(px, kTypingWidth, kTypingHeight, 4, 6, display, 255, 255, 255, 255, kTypingTextScale);
    typingTexture_.SetDataRGBA(px.data(), kTypingWidth * kTypingHeight);
}

void Hud::Draw(GraphicsDevice& device) {
    const auto& vp = device.getViewportProperty();
    const int sw = vp.getWidthProperty();
    const int sh = vp.getHeightProperty();

    device.SetDepthTestEnabled(false);
    spriteBatch_->Begin();

    const int crossSize = std::max(20, static_cast<int>(sh * 0.02f));
    spriteBatch_->Draw(
        crosshairTexture_,
        Rectangle((sw - crossSize) / 2, (sh - crossSize) / 2, crossSize, crossSize),
        crosshairTexture_.getBoundsProperty(),
        Color(255, 255, 255, 255));

    // Sized from a target screen HEIGHT (not a fraction of screen width) so
    // it stays legible regardless of resolution: scaling a very wide, short
    // texture (kHotbarWidth x kHotbarHeight) to fit a fraction of the
    // screen's width barely magnifies its height at all, which is what made
    // the original version too small to read on a real display. Falls back
    // to width-based sizing only if the height-based width would overflow
    // the screen.
    int hotbarDrawHeight = std::max(28, static_cast<int>(sh * 0.045f));
    int hotbarDrawWidth = hotbarDrawHeight * kHotbarWidth / kHotbarHeight;
    const int maxWidth = static_cast<int>(sw * 0.95f);
    if (hotbarDrawWidth > maxWidth) {
        hotbarDrawWidth = maxWidth;
        hotbarDrawHeight = hotbarDrawWidth * kHotbarHeight / kHotbarWidth;
    }
    spriteBatch_->Draw(
        hotbarTexture_,
        Rectangle((sw - hotbarDrawWidth) / 2, sh - hotbarDrawHeight - 12, hotbarDrawWidth, hotbarDrawHeight),
        hotbarTexture_.getBoundsProperty(),
        Color(255, 255, 255, 255));

    if (typingVisible_) {
        int typingDrawHeight = std::max(24, static_cast<int>(sh * 0.04f));
        int typingDrawWidth = typingDrawHeight * kTypingWidth / kTypingHeight;
        const int maxTypingWidth = static_cast<int>(sw * 0.95f);
        if (typingDrawWidth > maxTypingWidth) {
            typingDrawWidth = maxTypingWidth;
            typingDrawHeight = typingDrawWidth * kTypingHeight / kTypingWidth;
        }
        spriteBatch_->Draw(
            typingTexture_,
            Rectangle((sw - typingDrawWidth) / 2, sh - hotbarDrawHeight - typingDrawHeight - 20,
                      typingDrawWidth, typingDrawHeight),
            typingTexture_.getBoundsProperty(),
            Color(255, 255, 255, 255));
    }

    spriteBatch_->End();
    device.SetDepthTestEnabled(true);
}

}
