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
#include "Render/SkyDome.hpp"
#include "Worlds/Hotbar.hpp"
#include "Worlds/PlayerController.hpp"
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
