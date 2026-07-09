#include "TextureAtlas.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

using Microsoft::Xna::Framework::Color;
using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
using Microsoft::Xna::Framework::Graphics::Texture2D;

namespace CnaCraft::Render {

namespace {

struct TileColor {
    std::uint8_t r, g, b, a;
};

// One base color per atlas tile index — see BlockType.hpp for which tile
// index each block face uses. Procedural patterns below layer texture over
// these.
const TileColor kTileColors[] = {
    {90, 170, 60, 255},   // 0: grass top
    {120, 90, 55, 255},   // 1: grass side
    {105, 75, 45, 255},   // 2: dirt
    {130, 130, 130, 255}, // 3: stone
    {220, 200, 140, 255}, // 4: sand
    {40, 40, 40, 255},    // 5: bedrock
    {178, 79, 57, 255},   // 6: brick
    {92, 64, 42, 255},    // 7: wood side (bark)
    {196, 164, 116, 255}, // 8: wood top/bottom (rings)
    {160, 160, 165, 255}, // 9: cement
    {184, 148, 94, 255},  // 10: plank
    {235, 235, 245, 255}, // 11: snow top
    {210, 220, 230, 255}, // 12: snow side
    {110, 115, 100, 255}, // 13: cobblestone
    {190, 190, 180, 255}, // 14: light stone
    {70, 70, 75, 255},    // 15: dark stone
    {140, 200, 220, 130}, // 16: glass (partially transparent)
    {235, 235, 240, 255}, // 17: cloud
    {55, 120, 40, 215},   // 18: leaves (mottled green, partially transparent like glass)
    {75, 155, 50, 235},   // 19: tall grass (blade green; per-pixel gaps carve the blade shape)
    {235, 210, 60, 255},  // 20: flower (petal yellow; stem+bloom pattern carves the shape)
};
constexpr int kTileCount = sizeof(kTileColors) / sizeof(kTileColors[0]);

enum class Pattern { Mottle, Brick, WoodBark, WoodRings, Plank, SnowFleck, GrassBlade, Flower };

struct TilePattern {
    Pattern pattern;
    float strength; // only used by Pattern::Mottle
};

// Indexed the same as kTileColors. Most blocks get a generic speckle/mottle
// (varying strength — subtle for smooth materials like cement, punchier for
// coarse ones like cobblestone/bedrock); a handful get a dedicated pattern
// matching Craft's own textures.png look (brick's mortar grid, wood's
// bark/rings, plank seams, snow flecks).
constexpr TilePattern kTilePatterns[] = {
    {Pattern::Mottle, 0.22f},    // 0: grass top (blade speckle)
    {Pattern::Mottle, 0.16f},    // 1: grass side
    {Pattern::Mottle, 0.16f},    // 2: dirt
    {Pattern::Mottle, 0.14f},    // 3: stone
    {Pattern::Mottle, 0.10f},    // 4: sand
    {Pattern::Mottle, 0.22f},    // 5: bedrock
    {Pattern::Brick, 0.0f},      // 6: brick
    {Pattern::WoodBark, 0.0f},   // 7: wood side (bark)
    {Pattern::WoodRings, 0.0f},  // 8: wood top/bottom (rings)
    {Pattern::Mottle, 0.08f},    // 9: cement
    {Pattern::Plank, 0.0f},      // 10: plank
    {Pattern::SnowFleck, 0.0f},  // 11: snow top
    {Pattern::Mottle, 0.08f},    // 12: snow side
    {Pattern::Mottle, 0.20f},    // 13: cobblestone
    {Pattern::Mottle, 0.10f},    // 14: light stone
    {Pattern::Mottle, 0.12f},    // 15: dark stone
    {Pattern::Mottle, 0.06f},    // 16: glass
    {Pattern::Mottle, 0.05f},    // 17: cloud
    {Pattern::Mottle, 0.30f},    // 18: leaves (punchy speckle, like foliage clumps)
    {Pattern::GrassBlade, 0.0f}, // 19: tall grass (vertical blade streaks with cutout gaps)
    {Pattern::Flower, 0.0f},     // 20: flower (stem + bloom cutout shape)
};

// Deterministic per-pixel hash noise (same technique as
// NoiseGenerator::Lattice, just 2D-over-a-tile instead of 2D-over-the-world),
// in [0, 1).
float PixelHash(int tile, int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(tile) * 0x9E3779B1u + 1u;
    h ^= static_cast<std::uint32_t>(x) * 0x85EBCA77u;
    h ^= static_cast<std::uint32_t>(y) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0xCC9E2D51u;
    h ^= h >> 16;
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

std::uint8_t AdjustChannel(std::uint8_t base, float delta) {
    int v = static_cast<int>(base) + static_cast<int>(delta * 255.0f);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return static_cast<std::uint8_t>(v);
}

Color Shade(const TileColor& base, float delta) {
    return Color(AdjustChannel(base.r, delta), AdjustChannel(base.g, delta), AdjustChannel(base.b, delta), base.a);
}

Color MottlePattern(const TileColor& base, int tile, int x, int y, float strength) {
    const float noise = PixelHash(tile, x, y);
    return Shade(base, (noise - 0.5f) * 2.0f * strength);
}

Color BrickPattern(const TileColor& base, int x, int y) {
    // Running-bond brick: mortar rows every 4px, mortar columns every 8px,
    // staggered every other row — same coursing Craft's own brick tile uses.
    constexpr int kRowHeight = 4;
    const int row = y / kRowHeight;
    const int rowOffset = (row % 2) * 4;
    const bool mortarRow = (y % kRowHeight) == 0;
    const bool mortarCol = ((x + rowOffset) % 8) == 0;
    if (mortarRow || mortarCol) return Shade(base, -0.35f);
    return MottlePattern(base, 6, x, y, 0.08f);
}

Color WoodBarkPattern(const TileColor& base, int x, int y) {
    // Vertical bark striations with a slight vertical waver, plus light
    // grain noise so it doesn't look perfectly regular.
    const float stripe = ((x + (y / 4)) % 3 == 0) ? -0.12f : 0.0f;
    const float noise = (PixelHash(7, x, y) - 0.5f) * 0.12f;
    return Shade(base, stripe + noise);
}

Color WoodRingsPattern(const TileColor& base, int x, int y) {
    // Concentric growth rings emanating from the tile center.
    const float center = static_cast<float>(kAtlasTileSize) / 2.0f - 0.5f;
    const float dx = static_cast<float>(x) - center;
    const float dy = static_cast<float>(y) - center;
    const float dist = std::sqrt(dx * dx + dy * dy);
    return Shade(base, std::sin(dist * 1.6f) * 0.14f);
}

Color PlankPattern(const TileColor& base, int x, int y) {
    // Horizontal board seams every few rows, plus subtle grain noise.
    const bool seam = (y % 5) == 4;
    const float noise = (PixelHash(10, x, y) - 0.5f) * 0.10f;
    return Shade(base, (seam ? -0.25f : 0.0f) + noise);
}

Color SnowFleckPattern(const TileColor& base, int x, int y) {
    const float noise = PixelHash(11, x, y);
    return Shade(base, noise > 0.85f ? -0.15f : 0.02f);
}

Color GrassBladePattern(const TileColor& base, int x, int y) {
    // Vertical blade streaks with fully-transparent gaps between them (a
    // per-pixel stand-in for Craft's real hand-drawn tall-grass sprite,
    // textures/texture.png) so the cross-quad billboard (ChunkMesher's
    // EmitPlant) reads as individual blades rather than a solid green card.
    // A slight per-row wobble keeps the blades from looking perfectly
    // straight/mechanical.
    const int wobble = static_cast<int>((PixelHash(19, x, y / 3) - 0.5f) * 3.0f);
    const int bladeX = (x + wobble + kAtlasTileSize) % 4;
    if (bladeX >= 2) return Color(base.r, base.g, base.b, std::uint8_t{0}); // gap between blades
    const float noise = (PixelHash(19, x, y) - 0.5f) * 0.18f;
    return Shade(base, noise);
}

Color FlowerPattern(const TileColor& base, int x, int y) {
    // Simple stem-and-bloom shape on an otherwise fully-transparent
    // background -- a per-pixel stand-in for Craft's real hand-drawn flower
    // sprites (7 distinct color variants; this project has one
    // representative Flower type, see BlockType.hpp). Lower half: a thin
    // green stem. Upper half: a rough circular bloom cluster in the tile's
    // base (petal) color.
    constexpr int kCenter = kAtlasTileSize / 2;
    if (y >= kCenter) {
        const bool onStem = (x == kCenter || x == kCenter - 1);
        if (!onStem) return Color(base.r, base.g, base.b, std::uint8_t{0});
        return Shade(TileColor{70, 140, 40, 255}, (PixelHash(20, x, y) - 0.5f) * 0.10f);
    }
    const int dx = x - kCenter;
    const int dy = y - (kCenter / 2);
    const int distSq = dx * dx + dy * dy;
    constexpr int kBloomRadiusSq = 9;
    if (distSq > kBloomRadiusSq) return Color(base.r, base.g, base.b, std::uint8_t{0});
    return Shade(base, (PixelHash(20, x, y) - 0.5f) * 0.15f);
}

Color PaintPixel(int tile, int x, int y) {
    const TileColor& base = kTileColors[tile];
    const TilePattern& pat = kTilePatterns[tile];
    switch (pat.pattern) {
        case Pattern::Brick:      return BrickPattern(base, x, y);
        case Pattern::WoodBark:   return WoodBarkPattern(base, x, y);
        case Pattern::WoodRings:  return WoodRingsPattern(base, x, y);
        case Pattern::Plank:      return PlankPattern(base, x, y);
        case Pattern::SnowFleck:  return SnowFleckPattern(base, x, y);
        case Pattern::GrassBlade: return GrassBladePattern(base, x, y);
        case Pattern::Flower:     return FlowerPattern(base, x, y);
        case Pattern::Mottle:
        default:
            return MottlePattern(base, tile, x, y, pat.strength);
    }
}

}

Texture2D BuildProceduralAtlas(GraphicsDevice& device) {
    const int atlasSize = kAtlasTileSize * kAtlasTilesPerRow;
    std::vector<Color> pixels(static_cast<std::size_t>(atlasSize) * atlasSize, Color(255, 0, 255, 255));

    for (int tile = 0; tile < kTileCount; ++tile) {
        const int tx = (tile % kAtlasTilesPerRow) * kAtlasTileSize;
        const int ty = (tile / kAtlasTilesPerRow) * kAtlasTileSize;
        for (int y = 0; y < kAtlasTileSize; ++y) {
            for (int x = 0; x < kAtlasTileSize; ++x) {
                pixels[static_cast<std::size_t>((ty + y) * atlasSize + (tx + x))] = PaintPixel(tile, x, y);
            }
        }
    }

    Texture2D texture(device, atlasSize, atlasSize);
    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
    return texture;
}

void MapAtlasUv(int tileIndex, float localU, float localV, float& outU, float& outV) {
    const int atlasSize = kAtlasTileSize * kAtlasTilesPerRow;
    const int tx = (tileIndex % kAtlasTilesPerRow) * kAtlasTileSize;
    const int ty = (tileIndex / kAtlasTilesPerRow) * kAtlasTileSize;
    const float tileUnit = static_cast<float>(kAtlasTileSize) / static_cast<float>(atlasSize);
    outU = (static_cast<float>(tx) / static_cast<float>(atlasSize)) + localU * tileUnit;
    outV = (static_cast<float>(ty) / static_cast<float>(atlasSize)) + localV * tileUnit;
}

}
