#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Render {

// A textured sky sphere (CRAFT_PARITY.md §5.3, plan.md §12.1 item 33) —
// ports Craft's real sky dome (`src/cube.c` make_sphere + textures/sky.png
// + shaders/sky_*.glsl) onto stock BasicEffect. Craft's sky fragment shader
// samples `texture2D(sampler, vec2(timer, t))`: the U coordinate is the
// time of day (one uniform value for the whole dome per frame) and V is the
// vertex's elevation band (`t = 1 - acos(y)/pi`: 0 = nadir, 0.5 = horizon,
// 1 = zenith). BasicEffect has no custom-uniform slot to pass `timer`
// through, so Update() bakes the frame's time of day into every vertex's U
// instead and re-uploads the (small, ~450-vertex) buffer — the same
// per-frame re-upload cost profile the previous vertex-colored gradient
// version of this class already had. Vertices are VertexPositionColorTexture
// with white color: the one proven CNA Texture+VertexColor unlit combo
// (see MeshData.hpp's ChunkMeshData comment), drawn by CnaCraftGame with
// TextureEnabled+VertexColorEnabled, LightingEnabled=false, and
// SamplerState::LinearClamp — matching Craft's own GL_LINEAR +
// GL_CLAMP_TO_EDGE sky sampling exactly.
//
// This replaced an earlier vertex-colored gradient hemisphere (2026-07-10,
// user decision from the parity audit: "Přidat texturu oblohy"): the
// gradient version only approximated day/night tinting and had no
// dawn/dusk colors at all; the texture carries Craft's real sunrise/sunset
// bands. Now a full sphere (not a hemisphere) so looking below the horizon
// while flying shows Craft's real below-horizon sky colors too.
class SkyDome {
public:
    // Re-bakes the current time of day (in [0,1), see
    // Worlds::ComputeTimeOfDay) into every vertex's U coordinate and
    // re-uploads. Geometry/indices are built once, lazily.
    void Update(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, float timeOfDay);

    // Draws the dome centered on the camera position (so it always reads as
    // "infinitely far away," never translating with the player) at a fixed
    // large radius comfortably inside the far clip plane. The caller is
    // responsible for the BasicEffect flip (texture = the sky texture,
    // VertexColorEnabled, lighting/fog off) and the LinearClamp sampler.
    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect,
              const Microsoft::Xna::Framework::Vector3& cameraPosition);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib_;
    int primitiveCount_ = 0;
};

}
