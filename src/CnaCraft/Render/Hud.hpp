#pragma once

#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft::Render {

// Crosshair + hotbar overlay (plan.md §11.7), replacing the console-printf
// stopgap the hotbar/fly-mode features used until now. CNA has no
// content-pipeline SpriteFont available at runtime (see
// house3d_demo.cpp, which resorts to the same approach), so text is drawn
// with an embedded 8x8 bitmap font into a CPU-side pixel buffer, then
// uploaded as a Texture2D and drawn via SpriteBatch — same technique as
// house3d_demo.cpp's controls overlay.
class Hud {
public:
    explicit Hud(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    // Rebuilds the hotbar strip texture. Call only when the displayed state
    // actually changed (selected slot / flying) — this uploads a new texture,
    // it's not meant to run every frame.
    void RebuildHotbar(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const std::string* slotNames, int slotCount, int selectedIndex, bool flying);

    // Sign-typing overlay (CRAFT_PARITY.md §4.3) — call every frame while
    // typing (unlike RebuildHotbar's on-change-only convention) since the
    // in-progress text changes every keystroke; the texture is small and
    // typing is a low-frequency interaction, so per-frame rebuild is cheap
    // enough. `active=false` just hides the overlay in Draw().
    void SetTyping(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, bool active,
                    const std::string& text);

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    Microsoft::Xna::Framework::Graphics::Texture2D crosshairTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D hotbarTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D typingTexture_;
    bool typingVisible_ = false;
};

}
