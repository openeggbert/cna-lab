#pragma once

#include <memory>
#include <vector>

#include "Microsoft/Xna/Framework/Graphics/IndexBuffer.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexBuffer.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
class BasicEffect;
}

namespace CnaCraft::Render {

// Remote players as oriented textured cubes (MULTIPLAYER_PLAN.md §7,
// plan.md §12.1 item 18 M5) -- ports Craft's make_player/render_players
// (src/cube.c:160-189, src/main.c:1702-1719): each player IS a single
// cube, centered on their (eye) position, yaw/pitch-rotated so the face
// tile looks where they look. Skin comes from the atlas's kPlayerSkinTile
// (a stand-in for Craft's hand-drawn head tiles).
//
// One shared VB/IB re-uploaded per frame for ALL players (36 verts each --
// a handful of players is far smaller than one chunk mesh; same per-frame
// re-upload cost profile as SkyDome). Drawn inside the opaque terrain
// pass's effect state (VertexColorEnabled, unlit, daylight DiffuseColor):
// the baked vertex color of 127 = shade 1.0, so players carry plain
// daylight brightness with no AO -- Craft's own make_player uses flat
// light 0.8 and zero AO, the same "player models aren't terrain" idea.
class PlayerCube {
public:
    struct Instance {
        float x = 0, y = 0, z = 0;  // eye position (network State semantics)
        float yaw = 0, pitch = 0;
    };

    // Rebuilds + re-uploads the shared buffers. Empty list frees them.
    void Update(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                const std::vector<Instance>& players);

    // Draw with the terrain pass's state already applied (see class doc).
    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
              Microsoft::Xna::Framework::Graphics::BasicEffect& effect);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::VertexBuffer> vb_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::IndexBuffer> ib_;
    int primitiveCount_ = 0;
};

}
