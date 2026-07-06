#pragma once

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft::Render {

// A square grid of same-size tiles packed into one Texture2D atlas.
constexpr int kAtlasTileSize = 16;
constexpr int kAtlasTilesPerRow = 5;

// Procedurally generated per-tile textures (plan.md §11.2 "Real texture
// atlas image") — a per-pixel pattern (speckle/mottle, brick mortar grid,
// wood bark/rings, plank seams, snow flecks) layered over each tile's base
// color, in the spirit of Craft's own hand-authored textures.png tiles
// (mottled stone/dirt/sand, brick's mortar grid, wood's bark/ring patterns)
// without needing an actual art asset — built entirely in memory, same as
// the flat-color placeholder this replaced.
Microsoft::Xna::Framework::Graphics::Texture2D BuildProceduralAtlas(
    Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

// Remaps a tile-local (u, v) in [0,1] into tileIndex's rect within the atlas.
void MapAtlasUv(int tileIndex, float localU, float localV, float& outU, float& outV);

}
