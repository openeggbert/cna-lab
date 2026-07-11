#pragma once

#include <array>
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
    // Craft's own MAX_MESSAGES (config.h) -- public so Hud.cpp's texture-size
    // constant can derive from it without duplicating the literal.
    static constexpr int kMaxMessages = 4;

    explicit Hud(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    // Rebuilds the hotbar strip texture. Call only when the displayed state
    // actually changed (selected slot / flying) — this uploads a new texture,
    // it's not meant to run every frame.
    void RebuildHotbar(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const std::string* slotNames, int slotCount, int selectedIndex, bool flying);

    // Typing overlay (CRAFT_PARITY.md §4.3, plan.md §12.1 item 17) — call
    // every frame while typing (unlike RebuildHotbar's on-change-only
    // convention) since the in-progress text changes every keystroke; the
    // texture is small and typing is a low-frequency interaction, so
    // per-frame rebuild is cheap enough. `active=false` just hides the
    // overlay in Draw(). `label` distinguishes Sign vs Command typing
    // (CnaCraftGame passes "Sign"/"Command") since the raw buffer alone
    // doesn't always make that obvious (a Sign buffer has no visible
    // marker, unlike a Command buffer's leading '/').
    void SetTyping(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, bool active,
                    const std::string& label, const std::string& text);

    // On-screen scrolling message log (Craft's own g->messages/
    // MAX_MESSAGES ring buffer, config.h — plan.md §12.1 item 17), used for
    // command feedback ("Unknown command", "Viewing distance must be
    // between 1 and 24", etc.) the same way Craft's add_message is. Stays
    // visible for its 4 most recent messages regardless of whether typing
    // is currently active, matching Craft exactly (messages aren't tied to
    // the typing box's lifetime).
    void PushMessage(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device, const std::string& text);

    // Crosshair-targeted remote-player nametag (MULTIPLAYER_PLAN.md §7,
    // plan.md §12.1 item 18 M5) -- Craft's SHOW_PLAYER_NAMES draws the
    // aimed-at player's name as 2D HUD text near the crosshair
    // (main.c:2893-2898), NOT as a 3D billboard (real Craft has no
    // in-world nametags -- CRAFT_PARITY.md §4.4). Rebuilds its small
    // texture only when the name actually changes; empty name hides it.
    void SetTargetPlayerName(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                             const std::string& name);

    void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

private:
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    Microsoft::Xna::Framework::Graphics::Texture2D crosshairTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D hotbarTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D typingTexture_;
    bool typingVisible_ = false;

    // Ring buffer matching Craft's own g->messages/message_index exactly,
    // including the "empty slots are just skipped, no separate
    // populated-count needed" simplicity -- messages_ default-constructs to
    // kMaxMessages empty strings, and an empty slot is never rendered.
    std::array<std::string, kMaxMessages> messages_;
    int messageIndex_ = 0;
    Microsoft::Xna::Framework::Graphics::Texture2D messageLogTexture_;

    std::string targetPlayerName_;
    Microsoft::Xna::Framework::Graphics::Texture2D nametagTexture_;
};

}
