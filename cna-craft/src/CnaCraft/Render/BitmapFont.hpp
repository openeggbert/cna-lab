#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace CnaCraft::Render {

// Native size (before `scale`) of one glyph, in pixels — both dimensions.
constexpr int kBitmapFontGlyphSize = 8;

// Renders ASCII text into an RGBA pixel buffer at `scale`x the native 8x8
// glyph size (each source pixel becomes a scale-by-scale block). CNA has no
// content-pipeline SpriteFont available at runtime (see
// house3d_demo.cpp, which resorts to the same approach) — this embedded
// font is shared by Render/Hud.cpp (2D hotbar overlay) and
// Render/SignBillboard.cpp (3D sign text), rather than duplicated, since
// both need the same "rasterize ASCII text into an RGBA buffer" primitive.
//
// Returns the x position just past the last drawn glyph, so callers can
// chain multiple colored runs on one line without retracking width
// themselves.
int FontDrawText(std::vector<std::uint8_t>& buf, int bufW, int bufH, int x, int y, const std::string& text,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255, int scale = 1);

}
