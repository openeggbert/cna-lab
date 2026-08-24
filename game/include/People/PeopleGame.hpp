#pragma once

#include <memory>
#include <optional>

#include "People/World/IsometricProjection.hpp"
#include "People/World/LotGrid.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

/** @brief Initial CNA application and 2D isometric lot presentation. */
class PeopleGame final : public Microsoft::Xna::Framework::Game
{
public:
    /** @brief Creates an interactive game, or a bounded smoke run when frames is positive. */
    explicit PeopleGame(int smokeFrames = 0, bool smokeRotations = false);

    GetTypeNameHPP()

protected:
    void LoadContent() override;
    void Update(Microsoft::Xna::Framework::GameTime& gameTime) override;
    void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override;

private:
    void HandleCameraInput(double elapsedSeconds);
    void ChangeRotation(int clockwiseQuarterTurns);
    void ChangeZoom(double newZoom, People::World::PixelPoint screenFocus);
    void RefreshHoveredTile();
    void InitializeDemoLot();
    void DrawLot();
    void DrawTile(People::World::TileCoordinate tile);
    void DrawWall(People::World::WallEdge wall);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateTileTexture(bool highlight);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateWallTexture(
        bool slopesDownRight);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateDoorTexture(
        bool slopesDownRight, bool open);

    static constexpr double MinimumZoom = 0.35;
    static constexpr double MaximumZoom = 2.0;
    static constexpr double PanPixelsPerSecond = 420.0;

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    People::World::LotGrid lot_{20, 20, 1};
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    Microsoft::Xna::Framework::Graphics::Texture2D tileTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D highlightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D wallDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D wallUpRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorClosedDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorClosedUpRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorOpenDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorOpenUpRightTexture_;
    std::optional<People::World::WallEdge> demoDoor_;
    People::World::Camera camera_;
    std::optional<People::World::TileCoordinate> hoveredTile_;
    Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_;
    int previousWheel_ = 0;
    int smokeFrames_ = 0;
    int drawnFrames_ = 0;
    bool smokeRotations_ = false;
};
