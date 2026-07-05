#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

#include "Render/ChunkRenderer.hpp"
#include "Worlds/PlayerController.hpp"
#include "Worlds/World.hpp"

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

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::Texture2D> atlasTexture_;

    Worlds::World world_;
    std::unique_ptr<Worlds::PlayerController> player_;
    std::vector<Render::ChunkRenderer> chunkRenderers_;

    bool leftClickWasDown_ = false;
    bool rightClickWasDown_ = false;
    int smokeFramesLeft_ = 0;
};

}
