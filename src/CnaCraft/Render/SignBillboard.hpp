#pragma once

#include <memory>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Worlds {
struct Sign;
}

namespace CnaCraft::Render {

// Renders placed signs as 3D quads on the block face they're attached to
// (CRAFT_PARITY.md §4.3) — ports Craft's real sign rendering
// (`gen_sign_buffer`/`draw_signs`, src/main.c, src/cube.c
// `make_sign_buffer`), adapted: Craft samples pre-authored glyphs out of
// `textures/sign.png`; this project has no art assets (same reasoning as
// TextureAtlas's procedural tiles), so each sign's text is rasterized with
// the shared embedded bitmap font (Render/BitmapFont.hpp) into its own
// small dynamically-built texture instead.
//
// Each quad is emitted with both triangle windings (visible from either
// side) rather than a single Craft-accurate outward-facing winding, since
// this project's correct winding convention for non-cube geometry has
// turned out to need real-build empirical verification each time (see
// Render/SkyDome.cpp's winding note) — doubling the (very small, since sign
// counts are low) triangle count sidesteps needing another such pass here,
// and "visible from both sides" is a reasonable simplification for a
// decal-like sign anyway.
class SignBillboard {
public:
    // Rebuilds GPU resources for all `signs` — call only when the sign list
    // actually changed (e.g. right after placing/removing a sign), not
    // every frame, same convention as Render::ChunkRenderer::Rebuild.
    void Rebuild(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                 const std::vector<Worlds::Sign>& signs);

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

private:
    struct SignQuad {
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> texture;
    };
    std::vector<SignQuad> quads_;
};

}
