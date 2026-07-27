#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteFont.hpp"

#include <vector>

namespace Microsoft::Xna::Framework::Graphics
{
    class GraphicsDevice;
}

namespace IronShadows
{
    // Gate M10 (plan_28-ui-hud-menus-accessibility-and-input-rebinding.md): CNA has no XNB content
    // pipeline for fonts -- SpriteFont's own constructor is exposed precisely so an application can
    // build the glyph atlas itself (see SpriteFont.hpp's own doc comment). Rather than vendoring a
    // new TTF-rasterization dependency or sourcing an external font asset/license, this builds a
    // SpriteFont from the public-domain "font8x8" bitmap font (8x8 monochrome glyphs, originally
    // IBM's public-domain VGA font data; see src/UI/BitmapFont.cpp's own provenance comment and
    // assets/licenses/asset-registry.csv), covering the printable ASCII range U+0020 (space)
    // through U+007E ('~') -- everything Iron Shadows' HUD text actually needs.

    inline constexpr int kFont8x8GlyphCount = 95;
    inline constexpr int kFont8x8GlyphSize = 8;
    inline constexpr int kFont8x8AtlasColumns = 16;
    inline constexpr int kFont8x8AtlasRows = 6;
    inline constexpr int kFont8x8AtlasWidth = kFont8x8AtlasColumns * kFont8x8GlyphSize;
    inline constexpr int kFont8x8AtlasHeight = kFont8x8AtlasRows * kFont8x8GlyphSize;

    // Decodes the embedded font8x8 glyph bitmaps into a row-major RGBA atlas pixel buffer (white
    // where a glyph pixel is set, transparent otherwise) -- pulled out as its own GraphicsDevice-
    // independent function so the bit-unpacking/atlas-layout math (the part most likely to have an
    // off-by-one or bit-order bug) is headlessly unit-testable without needing a real device.
    [[nodiscard]] std::vector<Microsoft::Xna::Framework::Color> BuildFont8x8AtlasPixels();

    [[nodiscard]] Microsoft::Xna::Framework::Graphics::SpriteFont BuildBitmapFont8x8(
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
}
