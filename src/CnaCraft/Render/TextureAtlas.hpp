#pragma once

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft::Render {

// A square grid of same-size tiles packed into one Texture2D atlas.
constexpr int kAtlasTileSize = 16;
constexpr int kAtlasTilesPerRow = 5;

// M1/early-milestone placeholder: one flat color per tile index (see
// plan.md §5/§9 — a real texture asset replaces this at milestone M6). Built
// entirely in memory, no asset file dependency, so the render pipeline can be
// proven out before any art exists.
Microsoft::Xna::Framework::Graphics::Texture2D BuildPlaceholderAtlas(
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

// Remaps a tile-local (u, v) in [0,1] into tileIndex's rect within the atlas.
void MapAtlasUv(int tileIndex, float localU, float localV, float& outU, float& outV);

}
