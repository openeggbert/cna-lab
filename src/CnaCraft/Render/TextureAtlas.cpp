#include "TextureAtlas.hpp"

#include <cstddef>
#include <vector>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"

using Microsoft::Xna::Framework::Color;
using Microsoft::Xna::Framework::Graphics::GraphicsDevice;
using Microsoft::Xna::Framework::Graphics::Texture2D;

namespace CnaCraft::Render {

namespace {
// One placeholder color per atlas tile index — see BlockType.hpp for which
// tile index each block face uses.
const Color kTileColors[] = {
    Color(90, 170, 60, 255),   // 0: grass top
    Color(120, 90, 55, 255),   // 1: grass side
    Color(105, 75, 45, 255),   // 2: dirt
    Color(130, 130, 130, 255), // 3: stone
    Color(220, 200, 140, 255), // 4: sand
    Color(40, 40, 40, 255),    // 5: bedrock
    Color(178, 79, 57, 255),   // 6: brick
    Color(92, 64, 42, 255),    // 7: wood side (bark)
    Color(196, 164, 116, 255), // 8: wood top/bottom (rings)
    Color(160, 160, 165, 255), // 9: cement
    Color(184, 148, 94, 255),  // 10: plank
    Color(235, 235, 245, 255), // 11: snow top
    Color(210, 220, 230, 255), // 12: snow side
    Color(110, 115, 100, 255), // 13: cobblestone
    Color(190, 190, 180, 255), // 14: light stone
    Color(70, 70, 75, 255),    // 15: dark stone
    Color(140, 200, 220, 130), // 16: glass (partially transparent)
    Color(235, 235, 240, 255), // 17: cloud
};
constexpr int kTileCount = sizeof(kTileColors) / sizeof(kTileColors[0]);
}

Texture2D BuildPlaceholderAtlas(GraphicsDevice& device) {
    const int atlasSize = kAtlasTileSize * kAtlasTilesPerRow;
    std::vector<Color> pixels(static_cast<std::size_t>(atlasSize) * atlasSize, Color(255, 0, 255, 255));

    for (int tile = 0; tile < kTileCount; ++tile) {
        const int tx = (tile % kAtlasTilesPerRow) * kAtlasTileSize;
        const int ty = (tile / kAtlasTilesPerRow) * kAtlasTileSize;
        for (int y = 0; y < kAtlasTileSize; ++y) {
            for (int x = 0; x < kAtlasTileSize; ++x) {
                pixels[static_cast<std::size_t>((ty + y) * atlasSize + (tx + x))] = kTileColors[tile];
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
