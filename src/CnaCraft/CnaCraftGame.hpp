#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "Persistence/WorldStore.hpp"
#include "Render/ChunkRenderer.hpp"
#include "Render/Hud.hpp"
#include "Render/SelectionOutline.hpp"
#include "Render/SignBillboard.hpp"
#include "Render/SkyDome.hpp"
#include "Worlds/Hotbar.hpp"
#include "Worlds/PlayerController.hpp"
#include "Worlds/Sign.hpp"
#include "Worlds/World.hpp"

namespace Microsoft::Xna::Framework::Graphics {
class GraphicsDevice;
}

namespace CnaCraft {

// Game subclass wiring the engine-agnostic Worlds/ layer to CNA: input
// (Keyboard/Mouse) -> PlayerController/VoxelRaycast, World -> ChunkRenderer
// per chunk, BasicEffect + a placeholder texture atlas for drawing
// (plan.md §5/§6/§7). Reuses house3d_demo.cpp's Game-subclass shape.
class CnaCraftGame final : public Microsoft::Xna::Framework::Game {
public:
    CnaCraftGame();

    const std::string& GetTypeName() const override;

    // Mirrors house3d_demo.cpp's --smoke flag: quit after N Update() calls, for
    // headless CI verification without a human at the keyboard.
    void SetSmokeFrames(int n) { smokeFramesLeft_ = n; }

protected:
    void Initialize() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    void RebuildDirtyChunks();
    void CaptureScreenshot(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> atlasTexture_;

    Worlds::World world_;
    std::unique_ptr<Worlds::PlayerController> player_;
    std::vector<Render::ChunkRenderer> chunkRenderers_;
    Worlds::Hotbar hotbar_;

    // World persistence (CRAFT_PARITY.md §4.1/§4.2) — loaded once in
    // Initialize() (after World::Generate, before the initial chunk mesh
    // build), saved synchronously right after any player edit in Update().
    std::unique_ptr<Persistence::WorldStore> worldStore_;

    std::unique_ptr<Render::Hud> hud_;
    std::vector<std::string> hotbarSlotNames_;

    // Signs (CRAFT_PARITY.md §4.3, plan.md §12.1 item 16). signStore_ is the
    // engine-agnostic data model, loaded once in Initialize() (after
    // worldStore_->LoadInto). signBillboard_ holds the GPU resources and is
    // rebuilt only when the sign list actually changes (placed/erased),
    // never every frame -- same convention as chunkRenderers_.
    Worlds::SignStore signStore_;
    Render::SignBillboard signBillboard_;
    bool signsNeedRebuild_ = true;

    // Sign text-typing state machine (mirrors Craft's own g->typing /
    // g->typing_buffer in main.c). While isTypingSign_ is true, WASD/look/
    // click input is suspended (matching Craft's handle_movement gating
    // movement polling on !g->typing) but gravity/physics still integrates.
    // Backtick opens typing (edge-triggered), Enter submits (re-raycasts
    // fresh, matching Craft calling hit_test at Enter-time rather than
    // caching the block from when typing started), Escape cancels.
    bool isTypingSign_ = false;
    std::string typingBuffer_;
    bool backtickWasDown_ = false;
    bool backspaceWasDown_ = false;
    bool enterWasDown_ = false;
    bool escapeWasDown_ = false;

    // Visible targeted-block feedback (CRAFT_PARITY.md §2.4) — updated each
    // frame in Update() from the same raycast used for break/place, drawn in
    // Draw() only when hasTargetedBlock_ is true (looking at nothing within
    // reach draws no outline, matching Craft's own hit-test-gated behavior).
    Render::SelectionOutline selectionOutline_;
    Render::SkyDome skyDome_;
    bool hasTargetedBlock_ = false;
    int targetedBlockX_ = 0;
    int targetedBlockY_ = 0;
    int targetedBlockZ_ = 0;

    // Cursor capture (CRAFT_PARITY.md §1.2, user decision 2026-07-10: match
    // Craft's real behavior). Escape releases the cursor rather than
    // quitting; left-click while released re-captures it instead of
    // breaking/placing on that same click -- matches Craft's own
    // exclusive/on_mouse_button model exactly. There is no in-game quit key
    // in real Craft at all; closing the window (Alt+F4 / the X button) is
    // the only way to quit, already handled by CNA's own
    // SDL_EVENT_QUIT -> Game::Exit().
    bool cursorCaptured_ = true;

    bool leftClickWasDown_ = false;
    bool rightClickWasDown_ = false;
    bool middleClickWasDown_ = false;
    bool eKeyWasDown_ = false;
    bool rKeyWasDown_ = false;
    bool tabWasDown_ = false;
    bool scrollWheelInitialized_ = false;
    int previousScrollWheelValue_ = 0;
    bool f12WasDown_ = false;
    bool screenshotPending_ = false;
    int screenshotCounter_ = 0;
    int smokeFramesLeft_ = 0;
};

}
