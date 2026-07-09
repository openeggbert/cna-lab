#pragma once

#include <memory>

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Render {

// Renders a wireframe box around a targeted block cell (CRAFT_PARITY.md
// §2.4) — ports Craft's render_wireframe/make_cube_wireframe visual
// feedback for which block will be broken/placed (src/main.c:1734-1752,
// src/cube.c:191-214). Unlike ambient occlusion/sky-dome/fog, this needs no
// custom shader: a plain LineList primitive drawn with stock BasicEffect
// works identically on all three CNA backends (EASYGL/VULKAN/BGFX), so it
// isn't subject to the shader-backend limitations in missing.md.
class SelectionOutline {
public:
    // Rebuilds the 12-edge wireframe box around block cell (bx, by, bz).
    // Slightly oversized (Craft's own n=0.53 half-size vs. the block's true
    // half-size of 0.5) so the outline doesn't z-fight with the block's own
    // faces.
    void Update(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, int bx, int by, int bz);

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib_;
};

}
