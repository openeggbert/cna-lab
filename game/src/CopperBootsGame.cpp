#include "CopperBoots/CopperBootsGame.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <utility>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/TitleContainer.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/TextureFilter.hpp"
#include "Microsoft/Xna/Framework/Input/Buttons.hpp"
#include "Microsoft/Xna/Framework/Input/GamePad.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"
#include "System/IO/StreamReader.hpp"

namespace CopperBoots
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Rectangle;
    using Microsoft::Xna::Framework::Graphics::BlendState;
    using Microsoft::Xna::Framework::Graphics::RenderTarget2D;
    using Microsoft::Xna::Framework::Graphics::SpriteBatch;
    using Microsoft::Xna::Framework::Graphics::SpriteSortMode;
    using Microsoft::Xna::Framework::Graphics::Texture2D;
    using Microsoft::Xna::Framework::Graphics::TextureFilter;
    using Microsoft::Xna::Framework::Input::Buttons;
    using Microsoft::Xna::Framework::Input::GamePad;
    using Microsoft::Xna::Framework::Input::GamePadState;
    using Microsoft::Xna::Framework::Input::Keyboard;
    using Microsoft::Xna::Framework::Input::KeyboardState;
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        [[nodiscard]] int ScreenCoordinate(const float worldCoordinate,
                                           const float cameraCoordinate)
        {
            return static_cast<int>(std::round(worldCoordinate - cameraCoordinate));
        }
    }

    CopperBootsGame::CopperBootsGame(const bool smokeTest)
        : graphics_(this),
          smokeTest_(smokeTest)
    {
        graphics_.setPreferredBackBufferWidthProperty(960);
        graphics_.setPreferredBackBufferHeightProperty(540);
        graphics_.setSynchronizeWithVerticalRetraceProperty(true);
        getWindowProperty().setTitleProperty("Copper Boots - CNA platformer study");
        getWindowProperty().setAllowUserResizingProperty(true);
    }

    const std::string& CopperBootsGame::GetTypeName() const
    {
        static const std::string name = "CopperBoots.CopperBootsGame";
        return name;
    }

    void CopperBootsGame::LoadContent()
    {
        constexpr std::string_view levelPath =
            "Content/Levels/green_ruins.cbl";
        auto levelStream = Microsoft::Xna::Framework::TitleContainer::OpenStream(
            std::string(levelPath));
        System::IO::StreamReader reader(levelStream.get(), true);
        world_.LoadLevel(LevelDefinition::Parse(reader.ReadToEnd(), levelPath));

        auto& device = getGraphicsDeviceProperty();
        spriteBatch_ = std::make_unique<SpriteBatch>(device);
        solidTexture_ = std::make_unique<Texture2D>(device, 1, 1);
        logicalTarget_ = std::make_unique<RenderTarget2D>(
            device, LogicalWidth, LogicalHeight);

        const Color white = Color::White;
        solidTexture_->SetData(&white, 1);
        pointSampler_.setFilterProperty(TextureFilter::Point);

        std::cout << "Copper Boots: renderer "
                  << device.GetGraphicsRendererName()
                  << ", logical surface " << LogicalWidth << 'x' << LogicalHeight
                  << ", level " << levelPath
                  << "\n";
        std::cout.flush();
    }

    PlayerInput CopperBootsGame::ReadPlayerInput(const KeyboardState& keyboard,
                                                 const GamePadState& gamepad)
    {
        InputSnapshot snapshot;
        snapshot.Left = keyboard.IsKeyDown(Keys::A) ||
                        keyboard.IsKeyDown(Keys::Left) ||
                        gamepad.IsButtonDown(Buttons::DPadLeft);
        snapshot.Right = keyboard.IsKeyDown(Keys::D) ||
                         keyboard.IsKeyDown(Keys::Right) ||
                         gamepad.IsButtonDown(Buttons::DPadRight);
        snapshot.Run = keyboard.IsKeyDown(Keys::LeftShift) ||
                       keyboard.IsKeyDown(Keys::RightShift) ||
                       gamepad.IsButtonDown(Buttons::X) ||
                       gamepad.IsButtonDown(Buttons::RightShoulder) ||
                       gamepad.IsButtonDown(Buttons::RightTrigger);
        snapshot.Jump = keyboard.IsKeyDown(Keys::Space) ||
                        gamepad.IsButtonDown(Buttons::A);
        snapshot.Attack = keyboard.IsKeyDown(Keys::LeftControl) ||
                          keyboard.IsKeyDown(Keys::RightControl) ||
                          gamepad.IsButtonDown(Buttons::B);
        snapshot.AimUp = keyboard.IsKeyDown(Keys::Up) ||
                         keyboard.IsKeyDown(Keys::W) ||
                         gamepad.IsButtonDown(Buttons::DPadUp) ||
                         gamepad.IsButtonDown(Buttons::LeftThumbstickUp);
        snapshot.AimDown = keyboard.IsKeyDown(Keys::Down) ||
                           keyboard.IsKeyDown(Keys::S) ||
                           gamepad.IsButtonDown(Buttons::DPadDown) ||
                           gamepad.IsButtonDown(Buttons::LeftThumbstickDown);
        snapshot.Interact = keyboard.IsKeyDown(Keys::Down) ||
                            keyboard.IsKeyDown(Keys::S) ||
                            gamepad.IsButtonDown(Buttons::Y) ||
                            gamepad.IsButtonDown(Buttons::DPadDown);
        snapshot.Pause = keyboard.IsKeyDown(Keys::Escape) ||
                         gamepad.IsButtonDown(Buttons::Start);
        snapshot.AnalogMove = gamepad.getThumbSticksProperty().getLeftProperty().X;
        return inputAdapter_.Sample(snapshot);
    }

    void CopperBootsGame::Update(Microsoft::Xna::Framework::GameTime& gameTime)
    {
        const KeyboardState keyboard = Keyboard::GetState();
        const GamePadState gamepad = GamePad::GetState(
            Microsoft::Xna::Framework::PlayerIndex::One);
        const bool debugToggleDown = keyboard.IsKeyDown(Keys::F1);
        if (debugToggleDown && !debugToggleDown_)
            debugOverlay_ = !debugOverlay_;
        debugToggleDown_ = debugToggleDown;
        const auto updateStarted = debugOverlay_
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
        const auto finishUpdateTiming = [&]() {
            if (!debugOverlay_)
                return;
            updateMilliseconds_ = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - updateStarted).count();
        };
        PlayerInput input = ReadPlayerInput(keyboard, gamepad);
        if (input.PausePressed) {
            paused_ = !paused_;
            inputAdapter_.ConsumeEdges();
            Game::Update(gameTime);
            finishUpdateTiming();
            return;
        }
        if (paused_) {
            if (keyboard.IsKeyDown(Keys::R) || gamepad.IsButtonDown(Buttons::Y)) {
                world_.ResetPlayer();
                paused_ = false;
                inputAdapter_.ConsumeEdges();
            }
            else if (keyboard.IsKeyDown(Keys::Q) ||
                     gamepad.IsButtonDown(Buttons::Back)) {
                Exit();
            }
            Game::Update(gameTime);
            finishUpdateTiming();
            return;
        }
        const double elapsed =
            gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty();
        const int steps = clock_.AddFrameTime(elapsed);

        for (int step = 0; step < steps; ++step) {
            world_.Update(input, static_cast<float>(SimulationClock::TickSeconds));
            clock_.MarkStep();
            input.JumpPressed = false;
            input.AttackPressed = false;
            inputAdapter_.ConsumeEdges();
        }

        Game::Update(gameTime);
        finishUpdateTiming();
    }

    void CopperBootsGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime)
    {
        const auto drawStarted = debugOverlay_
            ? std::chrono::steady_clock::now()
            : std::chrono::steady_clock::time_point{};
        if (debugOverlay_) {
            frameMilliseconds_ = gameTime.getElapsedGameTimeProperty()
                .getTotalMillisecondsProperty();
        }
        auto& device = getGraphicsDeviceProperty();
        device.SetRenderTarget(logicalTarget_.get());
        DrawWorld();

        device.SetRenderTarget(nullptr);
        device.Clear(Color(8, 10, 18));
        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                            &pointSampler_, nullptr, nullptr);
        spriteBatch_->Draw(*logicalTarget_, PresentationRectangle(), Color::White);
        spriteBatch_->End();

        ++drawnFrames_;
        if (smokeTest_ && drawnFrames_ >= 3)
            Exit();

        Game::Draw(gameTime);
        if (debugOverlay_) {
            drawMilliseconds_ = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - drawStarted).count();
        }
    }

    void CopperBootsGame::DrawWorld()
    {
        auto& device = getGraphicsDeviceProperty();
        device.Clear(Color(42, 74, 105));
        spriteDrawCount_ = 0;

        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                            &pointSampler_, nullptr, nullptr);
        const float cameraX = world_.Camera().X();
        const float cameraY = world_.Camera().Y();
        DrawParallax(cameraX);
        DrawTiles(cameraX, cameraY);
        DrawCogs(cameraX, cameraY);
        DrawCrawlers(cameraX, cameraY);
        DrawPlatingPickups(cameraX, cameraY);
        DrawCapacitorPickups(cameraX, cameraY);
        DrawProjectiles(cameraX, cameraY);
        DrawRouteEndpoints(cameraX, cameraY);
        DrawPlayer(cameraX, cameraY);
        DrawHud();
        if (world_.Result().Completed)
            DrawCompletionOverlay();
        worldSpriteDrawCount_ = spriteDrawCount_;
        if (debugOverlay_)
            DrawDebugOverlay(cameraX, cameraY);
        if (world_.RouteTransition().Active)
            DrawRouteTransitionOverlay();
        if (paused_)
            DrawPauseOverlay();
        spriteBatch_->End();
    }

    void CopperBootsGame::DrawParallax(const float cameraX)
    {
        FillRectangle(Rectangle(0, 112, LogicalWidth, 68), Color(57, 99, 112));
        const auto& factors = world_.ParallaxFactors();
        const std::array<ParallaxLayer, 4> layers{
            ParallaxLayer{factors[0], 0.10F, 78, 116, 22,
                          {60, 83, 112}, ParallaxGeometry::BlockSilhouette,
                          true, false},
            ParallaxLayer{factors[0], 0.15F, 110, 33, 7,
                          {181, 211, 196}, ParallaxGeometry::CloudBand,
                          true, false},
            ParallaxLayer{factors[1], 0.25F, 54, 130, 30,
                          {58, 107, 104}, ParallaxGeometry::BlockSilhouette,
                          true, false},
            ParallaxLayer{factors[2], 0.50F, 38, 146, 20,
                          {70, 126, 95}, ParallaxGeometry::BlockSilhouette,
                          true, false},
        };
        for (const ParallaxLayer& layer : layers)
            DrawParallaxLayer(layer, cameraX);
    }

    void CopperBootsGame::DrawParallaxLayer(const ParallaxLayer& layer,
                                             const float cameraX)
    {
        if (layer.Spacing <= 0)
            return;
        const Color tint(layer.Tint.R, layer.Tint.G, layer.Tint.B);
        const int offset = layer.WrappedOffset(cameraX);
        const int startX = layer.Repeating ? -layer.Spacing - offset : -offset;
        const int endX = layer.Repeating
            ? LogicalWidth + layer.Spacing
            : startX + layer.Spacing;
        int worldIndex = layer.Fixed || layer.Spacing <= 0
            ? -1
            : static_cast<int>(std::floor(cameraX * layer.ScrollFactor)) /
                  layer.Spacing - 1;

        for (int x = startX; x < endX; x += layer.Spacing, ++worldIndex) {
            if (layer.Geometry == ParallaxGeometry::CloudBand) {
                FillRectangle(Rectangle(x + 20, layer.Baseline - 7, 42, 7), tint);
                FillRectangle(Rectangle(x + 29, layer.Baseline - 12, 25, 6),
                              Color(205, 225, 207));
                continue;
            }
            const int variation = std::abs(worldIndex * 17) % 22;
            const int height = layer.MinimumHeight + variation;
            FillRectangle(Rectangle(x, layer.Baseline - height,
                                    layer.Spacing - 5, height), tint);
        }
    }

    void CopperBootsGame::DrawTiles(const float cameraX, const float cameraY)
    {
        const TileMap& level = world_.Level();
        const int firstX = std::max(0, static_cast<int>(cameraX) / TileMap::TileSize - 1);
        const int lastX = std::min(level.Width() - 1,
            static_cast<int>(cameraX + LogicalWidth) / TileMap::TileSize + 1);
        const int firstY = std::max(0, static_cast<int>(cameraY) / TileMap::TileSize - 1);
        const int lastY = std::min(level.Height() - 1,
            static_cast<int>(cameraY + LogicalHeight) / TileMap::TileSize + 1);

        for (int y = firstY; y <= lastY; ++y) {
            for (int x = firstX; x <= lastX; ++x) {
                const Tile tile = level.Get(x, y);
                if (tile.Visual == TileVisual::None)
                    continue;

                const int screenX = ScreenCoordinate(
                    static_cast<float>(x * TileMap::TileSize), cameraX);
                const int screenY = ScreenCoordinate(
                    static_cast<float>(y * TileMap::TileSize), cameraY) +
                    world_.BlockVisualOffset(x, y);
                switch (tile.Visual) {
                case TileVisual::Ruin: {
                    const Color body = ((x + y) % 2 == 0)
                        ? Color(118, 82, 53)
                        : Color(104, 71, 49);
                    FillRectangle(Rectangle(screenX, screenY,
                                            TileMap::TileSize, TileMap::TileSize), body);
                    FillRectangle(Rectangle(screenX, screenY,
                                            TileMap::TileSize, 3), Color(166, 142, 69));
                    FillRectangle(Rectangle(screenX + 2, screenY + 6,
                                            4, 3), Color(73, 58, 48));
                    break;
                }
                case TileVisual::Breakable:
                    FillRectangle(Rectangle(screenX, screenY, 16, 16),
                                  Color(160, 90, 48));
                    FillRectangle(Rectangle(screenX + 1, screenY + 1, 14, 3),
                                  Color(221, 150, 65));
                    FillRectangle(Rectangle(screenX + 7, screenY + 4, 2, 12),
                                  Color(91, 55, 43));
                    break;
                case TileVisual::Interactive:
                    FillRectangle(Rectangle(screenX, screenY, 16, 16),
                                  Color(203, 137, 43));
                    FillRectangle(Rectangle(screenX + 2, screenY + 2, 12, 12),
                                  Color(236, 184, 61));
                    FillRectangle(Rectangle(screenX + 6, screenY + 4, 4, 2),
                                  Color(91, 65, 48));
                    FillRectangle(Rectangle(screenX + 8, screenY + 6, 2, 5),
                                  Color(91, 65, 48));
                    break;
                case TileVisual::UsedBlock:
                    FillRectangle(Rectangle(screenX, screenY, 16, 16),
                                  Color(93, 83, 65));
                    FillRectangle(Rectangle(screenX + 2, screenY + 2, 12, 12),
                                  Color(126, 111, 80));
                    break;
                case TileVisual::OneWay:
                    FillRectangle(Rectangle(screenX, screenY, 16, 4),
                                  Color(166, 142, 69));
                    FillRectangle(Rectangle(screenX + 2, screenY + 4, 12, 2),
                                  Color(73, 88, 64));
                    break;
                case TileVisual::Hazard:
                    FillRectangle(Rectangle(screenX, screenY + 12, 16, 4),
                                  Color(107, 46, 54));
                    for (int spike = 0; spike < 4; ++spike)
                        FillRectangle(Rectangle(screenX + spike * 4 + 1,
                                                screenY + 5 + spike % 2,
                                                2, 7 - spike % 2),
                                      Color(226, 100, 70));
                    break;
                case TileVisual::Exit:
                    FillRectangle(Rectangle(screenX + 2, screenY, 12, 16),
                                  Color(40, 71, 73));
                    FillRectangle(Rectangle(screenX + 4, screenY + 2, 8, 12),
                                  Color(95, 192, 158));
                    FillRectangle(Rectangle(screenX + 9, screenY + 8, 2, 2),
                                  Color(235, 189, 67));
                    break;
                case TileVisual::Decoration:
                    FillRectangle(Rectangle(screenX + 3, screenY + 8, 10, 8),
                                  Color(68, 143, 86));
                    FillRectangle(Rectangle(screenX + 7, screenY + 3, 3, 12),
                                  Color(94, 174, 93));
                    break;
                case TileVisual::None:
                    break;
                }
            }
        }
    }

    void CopperBootsGame::DrawPlayer(const float cameraX, const float cameraY)
    {
        const PlayerState& player = world_.Player();
        const int x = ScreenCoordinate(player.X, cameraX);
        const int y = ScreenCoordinate(player.Y, cameraY);
        const PlayerPose pose = SelectPlayerPose(player, world_.TickCount());
        if (pose == PlayerPose::Dead) {
            FillRectangle(Rectangle(x, y + 15, 12, 5), Color(124, 73, 48));
            FillRectangle(Rectangle(x + 2, y + 13, 8, 3), Color(205, 119, 42));
            return;
        }

        Color coat = pose == PlayerPose::RunA || pose == PlayerPose::RunB
            ? Color(232, 154, 48)
            : Color(205, 119, 42);
        if (player.Plated)
            coat = Color(79, 157, 124);
        if (player.PowerTransitionTicks > 0 &&
            (player.PowerTransitionTicks / 2) % 2 == 0)
            coat = Color(222, 225, 180);
        if (pose == PlayerPose::DamageBlink)
            coat = Color(231, 224, 181);

        const Color skin = pose == PlayerPose::DamageBlink
            ? Color(245, 239, 183)
            : Color(217, 189, 143);
        const Color boots(53, 81, 94);
        const auto partX = [&](const int offset, const int width) {
            return player.FacingRight
                ? x + offset
                : x + static_cast<int>(PlayerState::Width) - offset - width;
        };
        const auto part = [&](const int offset, const int offsetY,
                              const int width, const int height,
                              const Color& color) {
            FillRectangle(Rectangle(partX(offset, width), y + offsetY,
                                    width, height), color);
        };

        part(2, 0, 8, 5, skin);
        switch (pose) {
        case PlayerPose::WalkA:
            part(1, 5, 10, 10, coat);
            part(0, 6, 2, 6, coat);
            part(10, 8, 2, 6, coat);
            part(1, 15, 4, 5, boots);
            part(8, 15, 3, 4, boots);
            break;
        case PlayerPose::WalkB:
            part(1, 5, 10, 10, coat);
            part(0, 8, 2, 6, coat);
            part(10, 6, 2, 6, coat);
            part(1, 15, 3, 4, boots);
            part(7, 15, 4, 5, boots);
            break;
        case PlayerPose::RunA:
            part(2, 5, 9, 9, coat);
            part(0, 5, 3, 3, coat);
            part(10, 9, 2, 5, coat);
            part(0, 14, 5, 3, boots);
            part(7, 14, 5, 6, boots);
            break;
        case PlayerPose::RunB:
            part(2, 5, 9, 9, coat);
            part(0, 9, 2, 5, coat);
            part(9, 5, 3, 3, coat);
            part(0, 14, 5, 6, boots);
            part(7, 14, 5, 3, boots);
            break;
        case PlayerPose::Rise:
            part(1, 5, 10, 9, coat);
            part(0, 4, 2, 7, coat);
            part(10, 4, 2, 7, coat);
            part(2, 14, 3, 4, boots);
            part(7, 14, 3, 4, boots);
            break;
        case PlayerPose::Fall:
            part(1, 5, 10, 9, coat);
            part(0, 8, 3, 3, coat);
            part(9, 8, 3, 3, coat);
            part(0, 15, 5, 4, boots);
            part(7, 15, 5, 4, boots);
            break;
        case PlayerPose::DamageBlink:
            part(1, 5, 10, 10, coat);
            part(0, 7, 2, 6, skin);
            part(10, 7, 2, 6, skin);
            part(2, 15, 3, 5, boots);
            part(7, 15, 3, 5, boots);
            break;
        case PlayerPose::Idle:
        case PlayerPose::Dead:
            part(1, 5, 10, 10, coat);
            part(0, 7, 2, 6, coat);
            part(10, 7, 2, 6, coat);
            part(2, 15, 3, 5, boots);
            part(7, 15, 3, 5, boots);
            break;
        }

        part(8, 2, 2, 2, Color(24, 36, 42));
    }

    void CopperBootsGame::DrawCogs(const float cameraX, const float cameraY)
    {
        const int pulse = static_cast<int>((world_.TickCount() / 6U) % 3U);
        for (const CogState& cog : world_.Cogs()) {
            if (cog.Collected)
                continue;
            const int x = ScreenCoordinate(cog.X, cameraX);
            const int y = ScreenCoordinate(cog.Y, cameraY) - pulse;
            if (x < -8 || x > LogicalWidth || y < -8 || y > LogicalHeight)
                continue;

            FillRectangle(Rectangle(x + 2, y, 4, 8), Color(230, 173, 54));
            FillRectangle(Rectangle(x, y + 2, 8, 4), Color(230, 173, 54));
            FillRectangle(Rectangle(x + 3, y + 3, 2, 2), Color(102, 68, 46));
        }
    }

    void CopperBootsGame::DrawCrawlers(const float cameraX, const float cameraY)
    {
        for (const CrawlerState& crawler : world_.Crawlers()) {
            const int x = ScreenCoordinate(crawler.X, cameraX);
            const int y = ScreenCoordinate(crawler.Y, cameraY);
            if (x < -16 || x > LogicalWidth || y < -12 || y > LogicalHeight)
                continue;

            if (crawler.Defeated) {
                FillRectangle(Rectangle(x, y + 9, 14, 3), Color(93, 67, 51));
                continue;
            }
            FillRectangle(Rectangle(x + 1, y + 2, 12, 8),
                          Color(125, 71, 55));
            FillRectangle(Rectangle(x + 3, y, 8, 3), Color(188, 113, 59));
            FillRectangle(Rectangle(x, y + 10, 5, 2), Color(61, 64, 63));
            FillRectangle(Rectangle(x + 9, y + 10, 5, 2), Color(61, 64, 63));
            const int eyeX = crawler.Direction > 0 ? x + 9 : x + 3;
            FillRectangle(Rectangle(eyeX, y + 4, 2, 2), Color(240, 198, 76));
        }
    }

    void CopperBootsGame::DrawPlatingPickups(const float cameraX,
                                             const float cameraY)
    {
        for (const PlatingPickupState& pickup : world_.PlatingPickups()) {
            if (pickup.Collected)
                continue;
            const int x = ScreenCoordinate(pickup.X, cameraX);
            const int y = ScreenCoordinate(pickup.Y, cameraY);
            if (x < -12 || x > LogicalWidth || y < -12 || y > LogicalHeight)
                continue;
            FillRectangle(Rectangle(x + 1, y + 2, 10, 9),
                          Color(67, 137, 112));
            FillRectangle(Rectangle(x + 3, y, 6, 3), Color(112, 188, 143));
            FillRectangle(Rectangle(x + 3, y + 5, 6, 3), Color(191, 124, 56));
            FillRectangle(Rectangle(x, y + 3, 2, 6), Color(48, 87, 82));
            FillRectangle(Rectangle(x + 10, y + 3, 2, 6), Color(48, 87, 82));
        }
    }

    void CopperBootsGame::DrawCapacitorPickups(const float cameraX,
                                               const float cameraY)
    {
        for (const CapacitorPickupState& pickup : world_.CapacitorPickups()) {
            if (pickup.Collected)
                continue;
            const int x = ScreenCoordinate(pickup.X, cameraX);
            const int y = ScreenCoordinate(pickup.Y, cameraY);
            FillRectangle(Rectangle(x + 2, y, 6, 10), Color(99, 87, 177));
            FillRectangle(Rectangle(x, y + 3, 10, 4), Color(158, 117, 204));
            FillRectangle(Rectangle(x + 4, y + 2, 2, 6), Color(226, 213, 122));
        }
    }

    void CopperBootsGame::DrawProjectiles(const float cameraX,
                                          const float cameraY)
    {
        for (const ProjectileState& projectile : world_.Projectiles()) {
            if (!projectile.Active)
                continue;
            const int x = ScreenCoordinate(projectile.X, cameraX);
            const int y = ScreenCoordinate(projectile.Y, cameraY);
            FillRectangle(Rectangle(x, y, 4, 4), Color(229, 198, 74));
            FillRectangle(Rectangle(x + 1, y + 1, 2, 2), Color(245, 239, 183));
        }
    }

    void CopperBootsGame::DrawRouteEndpoints(const float cameraX,
                                             const float cameraY)
    {
        const PlayerState& player = world_.Player();
        const float playerCenter = player.X + PlayerState::Width * 0.5F;
        const float playerFoot = player.Y + PlayerState::Height;
        for (const RouteEndpointDefinition& endpoint : world_.RouteEndpoints()) {
            if (endpoint.Area != world_.CurrentArea())
                continue;
            const int x = ScreenCoordinate(static_cast<float>(
                endpoint.Position.X * TileMap::TileSize), cameraX);
            const int y = ScreenCoordinate(static_cast<float>(
                endpoint.Position.Y * TileMap::TileSize), cameraY);
            if (x < -16 || x > LogicalWidth || y < 0 || y > LogicalHeight + 4)
                continue;
            FillRectangle(Rectangle(x + 1, y - 4, 14, 4), Color(40, 71, 73));
            FillRectangle(Rectangle(x + 3, y - 3, 10, 2),
                          Color(95, 192, 158));
            FillRectangle(Rectangle(x + 7, y - 6, 2, 2),
                          Color(235, 189, 67));
            const float endpointCenter = static_cast<float>(
                endpoint.Position.X * TileMap::TileSize) +
                TileMap::TileSize * 0.5F;
            const float endpointFoot = static_cast<float>(
                endpoint.Position.Y * TileMap::TileSize);
            if (std::abs(playerCenter - endpointCenter) <= 20.0F &&
                std::abs(playerFoot - endpointFoot) <= 1.0F &&
                !world_.RouteTransition().Active) {
                DrawText("DOWN", x, y - 14, Color(235, 189, 67));
            }
        }
    }

    void CopperBootsGame::DrawHud()
    {
        const Color ink(231, 224, 181);
        FillRectangle(Rectangle(0, 0, LogicalWidth, 14), Color(24, 36, 42));
        DrawText(world_.LevelName(), 4, 3, ink);
        DrawText("COG", 112, 3, Color(230, 173, 54));
        DrawNumber(world_.CollectedCogCount(), 2, 128, 3, ink);
        DrawText("L", 146, 3, Color(112, 188, 143));
        DrawNumber(world_.Lives(), 1, 152, 3, ink);
        DrawText("S", 164, 3, Color(95, 192, 158));
        DrawNumber(world_.Score(), 6, 170, 3, ink);

        FillRectangle(Rectangle(300, 3, 7, 7), world_.Player().Plated
            ? Color(79, 157, 124)
            : Color(53, 58, 57));
        FillRectangle(Rectangle(309, 3, 7, 7), world_.Player().ArcCapacitor
            ? Color(158, 117, 204)
            : Color(53, 58, 57));
    }

    void CopperBootsGame::DrawDebugOverlay(const float cameraX,
                                            const float cameraY)
    {
        const TileMap& level = world_.Level();
        const int firstX = std::max(0,
            static_cast<int>(cameraX) / TileMap::TileSize - 1);
        const int lastX = std::min(level.Width() - 1,
            static_cast<int>(cameraX + LogicalWidth) / TileMap::TileSize + 1);
        const int firstY = std::max(0,
            static_cast<int>(cameraY) / TileMap::TileSize - 1);
        const int lastY = std::min(level.Height() - 1,
            static_cast<int>(cameraY + LogicalHeight) / TileMap::TileSize + 1);
        for (int tileY = firstY; tileY <= lastY; ++tileY) {
            for (int tileX = firstX; tileX <= lastX; ++tileX) {
                const TileCollision collision =
                    level.Get(tileX, tileY).Collision;
                if (collision == TileCollision::None)
                    continue;
                Color color(95, 192, 158);
                if (collision == TileCollision::OneWay)
                    color = Color(235, 189, 67);
                else if (collision == TileCollision::Hazard)
                    color = Color(226, 100, 70);
                else if (collision == TileCollision::Exit)
                    color = Color(158, 117, 204);
                OutlineRectangle(Rectangle(
                    ScreenCoordinate(static_cast<float>(
                        tileX * TileMap::TileSize), cameraX),
                    ScreenCoordinate(static_cast<float>(
                        tileY * TileMap::TileSize), cameraY),
                    TileMap::TileSize, TileMap::TileSize), color);
            }
        }

        const PlayerState& player = world_.Player();
        OutlineRectangle(Rectangle(ScreenCoordinate(player.X, cameraX),
                                   ScreenCoordinate(player.Y, cameraY),
                                   static_cast<int>(PlayerState::Width),
                                   static_cast<int>(PlayerState::Height)),
                         Color(245, 239, 183));
        for (const CrawlerState& crawler : world_.Crawlers()) {
            if (crawler.Defeated)
                continue;
            OutlineRectangle(Rectangle(ScreenCoordinate(crawler.X, cameraX),
                                       ScreenCoordinate(crawler.Y, cameraY),
                                       static_cast<int>(CrawlerState::Width),
                                       static_cast<int>(CrawlerState::Height)),
                             Color(229, 96, 122));
        }
        OutlineRectangle(Rectangle(0, 0, LogicalWidth, LogicalHeight),
                         Color(99, 164, 201));

        FillRectangle(Rectangle(2, 16, 156, 43), Color(24, 36, 42));
        DrawText("DEBUG F1 T", 5, 19, Color(235, 189, 67));
        DrawNumber(static_cast<int>(world_.TickCount() % 1'000'000U),
                   6, 49, 19, Color(231, 224, 181));
        const int tileX = static_cast<int>(std::floor(
            (player.X + PlayerState::Width * 0.5F) / TileMap::TileSize));
        const int tileY = static_cast<int>(std::floor(
            (player.Y + PlayerState::Height * 0.5F) / TileMap::TileSize));
        DrawText("TILE", 5, 26, Color(95, 192, 158));
        DrawNumber(tileX, 3, 25, 26, Color(231, 224, 181));
        DrawNumber(tileY, 3, 41, 26, Color(231, 224, 181));
        DrawText("VEL", 61, 26, Color(95, 192, 158));
        DrawSignedNumber(static_cast<int>(std::round(player.VelocityX)),
                         3, 77, 26, Color(231, 224, 181));
        DrawSignedNumber(static_cast<int>(std::round(player.VelocityY)),
                         3, 97, 26, Color(231, 224, 181));

        DrawText("CAM", 5, 33, Color(99, 164, 201));
        DrawNumber(static_cast<int>(cameraX), 4, 21, 33,
                   Color(231, 224, 181));
        DrawNumber(static_cast<int>(cameraY), 3, 41, 33,
                   Color(231, 224, 181));
        DrawNumber(static_cast<int>(cameraX + world_.Camera().ViewportWidth()),
                   4, 57, 33, Color(231, 224, 181));
        DrawNumber(static_cast<int>(cameraY + world_.Camera().ViewportHeight()),
                   3, 77, 33, Color(231, 224, 181));

        DrawText("SPR", 5, 40, Color(229, 96, 122));
        DrawNumber(worldSpriteDrawCount_, 4, 21, 40,
                   Color(231, 224, 181));
        DrawText("MS F", 41, 40, Color(229, 96, 122));
        DrawNumber(static_cast<int>(std::round(frameMilliseconds_)), 3,
                   61, 40, Color(231, 224, 181));
        DrawText("U", 77, 40, Color(229, 96, 122));
        DrawNumber(static_cast<int>(std::round(updateMilliseconds_)), 3,
                   85, 40, Color(231, 224, 181));
        DrawText("D", 101, 40, Color(229, 96, 122));
        DrawNumber(static_cast<int>(std::round(drawMilliseconds_)), 3,
                   109, 40, Color(231, 224, 181));
        DrawText("BOX SOLID ONE HAZ EXIT", 5, 49, Color(231, 224, 181));
    }

    void CopperBootsGame::DrawPauseOverlay()
    {
        FillRectangle(Rectangle(72, 52, 176, 76), Color(24, 36, 42));
        FillRectangle(Rectangle(75, 55, 170, 70), Color(53, 81, 94));
        const Color title(235, 189, 67);
        const Color ink(231, 224, 181);
        DrawText("PAUSED", 148, 64, title);
        DrawText("ESC START RESUME", 116, 82, ink);
        DrawText("R Y RESTART", 132, 94, ink);
        DrawText("Q BACK QUIT", 132, 106, ink);
    }

    void CopperBootsGame::DrawRouteTransitionOverlay()
    {
        const int barHeight = std::clamp(static_cast<int>(std::round(
            world_.RouteFadeAmount() * LogicalHeight * 0.5F)),
            0, LogicalHeight / 2);
        FillRectangle(Rectangle(0, 0, LogicalWidth, barHeight),
                      Color(8, 10, 18));
        FillRectangle(Rectangle(0, LogicalHeight - barHeight,
                                LogicalWidth, barHeight), Color(8, 10, 18));
    }

    void CopperBootsGame::DrawCompletionOverlay()
    {
        const int progress = std::min(world_.CompletionTicks(), 30);
        const int barHeight = progress * 2;
        FillRectangle(Rectangle(0, 0, LogicalWidth, barHeight),
                      Color(24, 36, 42));
        FillRectangle(Rectangle(0, LogicalHeight - barHeight,
                                LogicalWidth, barHeight), Color(24, 36, 42));
        if (world_.CompletionTicks() > 15) {
            DrawText("RELAY COMPLETE", 132, 84, Color(235, 189, 67));
            DrawNumber(world_.Result().Score, 6, 148, 96,
                       Color(231, 224, 181));
        }
    }

    void CopperBootsGame::DrawText(const std::string_view text,
                                   const int x, const int y,
                                   const Color& color)
    {
        int cursor = x;
        for (const char glyph : text) {
            DrawGlyph(glyph, cursor, y, color);
            cursor += 4;
        }
    }

    void CopperBootsGame::DrawNumber(int value, const int digits,
                                     const int x, const int y,
                                     const Color& color)
    {
        value = std::max(value, 0);
        int divisor = 1;
        for (int i = 1; i < digits; ++i)
            divisor *= 10;
        for (int i = 0; i < digits; ++i) {
            const int digit = (value / divisor) % 10;
            DrawGlyph(static_cast<char>('0' + digit), x + i * 4, y, color);
            if (divisor > 1)
                divisor /= 10;
        }
    }

    void CopperBootsGame::DrawSignedNumber(const int value, const int digits,
                                           const int x, const int y,
                                           const Color& color)
    {
        DrawGlyph(value < 0 ? '-' : '+', x, y, color);
        DrawNumber(std::abs(value), digits, x + 4, y, color);
    }

    void CopperBootsGame::DrawGlyph(const char glyph, const int x, const int y,
                                    const Color& color)
    {
        const auto rows = GlyphRows(glyph);
        for (int row = 0; row < 5; ++row) {
            for (int column = 0; column < 3; ++column) {
                if ((rows[static_cast<std::size_t>(row)] &
                     (1U << (2 - column))) != 0)
                    FillRectangle(Rectangle(x + column, y + row, 1, 1), color);
            }
        }
    }

    std::array<std::uint8_t, 5> CopperBootsGame::GlyphRows(char glyph)
    {
        if (glyph >= 'a' && glyph <= 'z')
            glyph = static_cast<char>(glyph - 'a' + 'A');
        switch (glyph) {
        case '0': return {7, 5, 5, 5, 7};
        case '1': return {2, 6, 2, 2, 7};
        case '2': return {7, 1, 7, 4, 7};
        case '3': return {7, 1, 7, 1, 7};
        case '4': return {5, 5, 7, 1, 1};
        case '5': return {7, 4, 7, 1, 7};
        case '6': return {7, 4, 7, 5, 7};
        case '7': return {7, 1, 1, 2, 2};
        case '8': return {7, 5, 7, 5, 7};
        case '9': return {7, 5, 7, 1, 7};
        case 'A': return {2, 5, 7, 5, 5};
        case 'B': return {6, 5, 6, 5, 6};
        case 'C': return {3, 4, 4, 4, 3};
        case 'D': return {6, 5, 5, 5, 6};
        case 'E': return {7, 4, 6, 4, 7};
        case 'F': return {7, 4, 6, 4, 4};
        case 'G': return {3, 4, 5, 5, 3};
        case 'H': return {5, 5, 7, 5, 5};
        case 'I': return {7, 2, 2, 2, 7};
        case 'K': return {5, 5, 6, 5, 5};
        case 'L': return {4, 4, 4, 4, 7};
        case 'M': return {5, 7, 7, 5, 5};
        case 'N': return {5, 7, 7, 5, 5};
        case 'O': return {2, 5, 5, 5, 2};
        case 'P': return {6, 5, 6, 4, 4};
        case 'Q': return {2, 5, 5, 3, 1};
        case 'R': return {6, 5, 6, 5, 5};
        case 'S': return {3, 4, 2, 1, 6};
        case 'T': return {7, 2, 2, 2, 2};
        case 'U': return {5, 5, 5, 5, 7};
        case 'V': return {5, 5, 5, 5, 2};
        case 'W': return {5, 5, 7, 7, 5};
        case 'X': return {5, 5, 2, 5, 5};
        case 'Y': return {5, 5, 2, 2, 2};
        case 'Z': return {7, 1, 2, 4, 7};
        case '+': return {0, 2, 7, 2, 0};
        case '-': return {0, 0, 7, 0, 0};
        default: return {0, 0, 0, 0, 0};
        }
    }

    void CopperBootsGame::FillRectangle(const Rectangle& rectangle,
                                         const Color& color)
    {
        spriteBatch_->Draw(*solidTexture_, rectangle, color);
        ++spriteDrawCount_;
    }

    void CopperBootsGame::OutlineRectangle(const Rectangle& rectangle,
                                            const Color& color)
    {
        if (rectangle.Width <= 0 || rectangle.Height <= 0) {
            return;
        }
        FillRectangle(Rectangle(rectangle.X, rectangle.Y,
                                rectangle.Width, 1), color);
        FillRectangle(Rectangle(rectangle.X,
                                rectangle.Y + rectangle.Height - 1,
                                rectangle.Width, 1), color);
        FillRectangle(Rectangle(rectangle.X, rectangle.Y, 1,
                                rectangle.Height), color);
        FillRectangle(Rectangle(rectangle.X + rectangle.Width - 1,
                                rectangle.Y, 1, rectangle.Height), color);
    }

    Rectangle CopperBootsGame::PresentationRectangle()
    {
        const auto& viewport = getGraphicsDeviceProperty().getViewportProperty();
        const int width = viewport.getWidthProperty();
        const int height = viewport.getHeightProperty();
        const int integerScale = std::max(1,
            std::min(width / LogicalWidth, height / LogicalHeight));

        int destinationWidth = LogicalWidth * integerScale;
        int destinationHeight = LogicalHeight * integerScale;
        if (width < LogicalWidth || height < LogicalHeight) {
            const float scale = std::min(
                static_cast<float>(width) / LogicalWidth,
                static_cast<float>(height) / LogicalHeight);
            destinationWidth = std::max(1,
                static_cast<int>(std::floor(LogicalWidth * scale)));
            destinationHeight = std::max(1,
                static_cast<int>(std::floor(LogicalHeight * scale)));
        }

        return Rectangle((width - destinationWidth) / 2,
                         (height - destinationHeight) / 2,
                         destinationWidth, destinationHeight);
    }
}
