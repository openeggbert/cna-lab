#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "People/Navigation/AStarPathfinder.hpp"
#include "People/Navigation/StaticNavigationGrid.hpp"
#include "People/Objects/ObjectModel.hpp"
#include "People/Rendering/ObjectPresentation.hpp"
#include "People/Rendering/ResidentPresentation.hpp"
#include "People/Simulation/ResidentModel.hpp"
#include "People/Simulation/MovementExecutor.hpp"
#include "People/World/IsometricProjection.hpp"
#include "People/World/LotGrid.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Input/ButtonState.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"

/** @brief Initial CNA application and 2D isometric lot presentation. */
class PeopleGame final : public Microsoft::Xna::Framework::Game
{
public:
    /** @brief Creates an interactive game, or a bounded smoke run when frames is positive. */
    explicit PeopleGame(
        int smokeFrames = 0,
        bool smokeRotations = false,
        bool smokeWalk = false);

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
    void IssueDemoMove(People::World::TileCoordinate destination);
    void AdvanceSimulationTick();
    void InitializeDemoLot();
    void DrawLot();
    void DrawTile(People::World::TileCoordinate tile);
    void DrawWall(People::World::WallEdge wall);
    void DrawObject(People::Objects::ObjectInstanceId objectId);
    void DrawResident(People::Simulation::ResidentId residentId);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateTileTexture(bool highlight);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateWallTexture(
        bool slopesDownRight);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateDoorTexture(
        bool slopesDownRight, bool open);
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateFurnitureTexture(
        std::string_view definitionId, People::Rendering::SpriteDirection direction);
    /**
     * @param walkPhase -1 draws the idle stance; 0 and 1 draw the walk frames.
     */
    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Texture2D CreateResidentTexture(
        People::Rendering::SpriteDirection direction,
        int walkPhase);

    static constexpr double MinimumZoom = 0.35;
    static constexpr double MaximumZoom = 2.0;
    static constexpr double PanPixelsPerSecond = 420.0;
    static constexpr double SimulationTickSeconds = 1.0 / 20.0;

    Microsoft::Xna::Framework::GraphicsDeviceManager graphics_;
    People::World::LotGrid lot_{20, 20, 1};
    People::Objects::ObjectWorld objects_{lot_};
    People::Simulation::ResidentRegistry residents_{lot_};
    People::Simulation::MovementExecutor movement_{residents_};
    std::unique_ptr<Microsoft::Xna::Framework::Graphics::SpriteBatch> spriteBatch_;
    Microsoft::Xna::Framework::Graphics::Texture2D tileTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D highlightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D wallDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D wallUpRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorClosedDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorClosedUpRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorOpenDownRightTexture_;
    Microsoft::Xna::Framework::Graphics::Texture2D doorOpenUpRightTexture_;
    std::map<std::string, Microsoft::Xna::Framework::Graphics::Texture2D, std::less<>>
        objectTextures_;
    People::Rendering::ResidentIdleSpriteSet demoResidentSprites_;
    People::Rendering::ResidentWalkSpriteSet demoResidentWalkSprites_;
    std::map<std::string, Microsoft::Xna::Framework::Graphics::Texture2D, std::less<>>
        residentTextures_;
    std::optional<People::World::WallEdge> demoDoor_;
    People::World::Camera camera_;
    std::optional<People::World::TileCoordinate> hoveredTile_;
    std::optional<People::Objects::ObjectInstanceId> selectedObject_;
    Microsoft::Xna::Framework::Input::KeyboardState previousKeyboard_;
    Microsoft::Xna::Framework::Input::ButtonState previousLeftButton_ =
        Microsoft::Xna::Framework::Input::ButtonState::Released;
    Microsoft::Xna::Framework::Input::ButtonState previousRightButton_ =
        Microsoft::Xna::Framework::Input::ButtonState::Released;
    int previousWheel_ = 0;
    double simulationAccumulator_ = 0.0;
    People::Simulation::MovementRequestId nextDemoMovementRequest_ = 1;
    int smokeFrames_ = 0;
    int drawnFrames_ = 0;
    bool smokeRotations_ = false;
    /** @brief Bounded developer smoke that routes the demo resident and traces frames. */
    bool smokeWalk_ = false;
    bool smokeWalkStarted_ = false;
};
