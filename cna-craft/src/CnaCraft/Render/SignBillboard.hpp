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
// Each quad uses a single outward-facing winding (updated 2026-07-10) —
// same (0,1,2)+(0,2,3) corner-order convention as ChunkMesher's cube
// faces, which is proven correct against CNA's default rasterizer state
// (every cube face in this project has rendered outward, never culled,
// across many real-build screenshots this whole session). NOT
// independently re-verified for this specific sign-quad case, though --
// a real-build check was attempted but blocked by this session's own
// synthetic-text-input flakiness (Xvfb/SDL stopped delivering
// TextInputEXT characters partway through this session, affecting even
// the already-working `/`-command typing, so no sign with real text could
// be placed to look at). If you touch this again, verify with a real
// screenshot before trusting this comment's confidence — this is a
// same-convention inference, not a confirmed observation. An earlier
// version emitted both windings to sidestep needing this exact
// verification pass (see Render/SkyDome.cpp's note on why that's normally
// needed for non-cube geometry); reverting to that safe fallback is one
// line if this turns out wrong (see git history for the removed indices).
// If correct, this removes the both-windings version's known tradeoff:
// faint mirrored "ghost text" visible from behind at a grazing angle.
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
