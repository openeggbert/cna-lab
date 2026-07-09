#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Render {

// A plain vertex-colored gradient hemisphere replacing the flat
// `device.Clear(...)` sky (CRAFT_PARITY.md §5.3) — the safe, no-asset-
// dependency version of Craft's real sky dome (`src/cube.c` make_sphere,
// textured with textures/sky.png and time-of-day sampling in
// shaders/sky_*.glsl). Full texture-sampling parity would need a new art
// asset and a custom ShaderEffect (only real on EASYGL today per
// missing.md) — a scope decision left `needs_human`. This version needs
// neither: a low-poly hemisphere with per-vertex color interpolated from a
// horizon color (at the rim) to a zenith color (at the pole), rebuilt each
// frame from the same day/night sky colors `CnaCraftGame::Draw` already
// computes for the fallback clear color, drawn with stock `BasicEffect`
// (`VertexColorEnabled`, no texture) — works identically on all three CNA
// backends.
class SkyDome {
public:
    // Rebuilds per-vertex colors for the current frame's horizon/zenith sky
    // tint (cheap — a fixed, small vertex count, no geometry changes).
    void Update(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                const Microsoft::Xna::Framework::Color& horizonColor,
                const Microsoft::Xna::Framework::Color& zenithColor);

    // Draws the dome centered on the camera position (so it always reads as
    // "infinitely far away," never translating with the player) at a fixed
    // large radius comfortably inside the far clip plane.
    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
              const Microsoft::Xna::Framework::Vector3& cameraPosition);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib_;
    int primitiveCount_ = 0;
};

}
