#include "CopperBoots/CopperBootsGame.hpp"

#include <algorithm>
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
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
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

    PlayerInput CopperBootsGame::ReadPlayerInput(const KeyboardState& keyboard)
    {
        const bool left = keyboard.IsKeyDown(Keys::A) ||
                          keyboard.IsKeyDown(Keys::Left);
        const bool right = keyboard.IsKeyDown(Keys::D) ||
                           keyboard.IsKeyDown(Keys::Right);
        const bool jumpHeld = keyboard.IsKeyDown(Keys::Space);
        const bool jumpWasHeld = previousKeyboard_.IsKeyDown(Keys::Space);
        jumpLatched_ = jumpLatched_ || (jumpHeld && !jumpWasHeld);

        PlayerInput result;
        result.Move = static_cast<float>(static_cast<int>(right) -
                                         static_cast<int>(left));
        result.Run = keyboard.IsKeyDown(Keys::LeftShift) ||
                     keyboard.IsKeyDown(Keys::RightShift);
        result.JumpHeld = jumpHeld;
        result.JumpPressed = jumpLatched_;
        previousKeyboard_ = keyboard;
        return result;
    }

    void CopperBootsGame::Update(Microsoft::Xna::Framework::GameTime& gameTime)
    {
        const KeyboardState keyboard = Keyboard::GetState();
        if (keyboard.IsKeyDown(Keys::Escape)) {
            Exit();
            return;
        }

        PlayerInput input = ReadPlayerInput(keyboard);
        const double elapsed =
            gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty();
        const int steps = clock_.AddFrameTime(elapsed);

        for (int step = 0; step < steps; ++step) {
            world_.Update(input, static_cast<float>(SimulationClock::TickSeconds));
            clock_.MarkStep();
            input.JumpPressed = false;
            jumpLatched_ = false;
        }

        Game::Update(gameTime);
    }

    void CopperBootsGame::Draw(const Microsoft::Xna::Framework::GameTime& gameTime)
    {
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
    }

    void CopperBootsGame::DrawWorld()
    {
        auto& device = getGraphicsDeviceProperty();
        device.Clear(Color(42, 74, 105));

        spriteBatch_->Begin(SpriteSortMode::Deferred, BlendState::Opaque,
                            &pointSampler_, nullptr, nullptr);
        const float cameraX = world_.Camera().X();
        const float cameraY = world_.Camera().Y();
        DrawParallax(cameraX);
        DrawTiles(cameraX, cameraY);
        DrawCogs(cameraX, cameraY);
        DrawPlayer(cameraX, cameraY);
        spriteBatch_->End();
    }

    void CopperBootsGame::DrawParallax(const float cameraX)
    {
        FillRectangle(Rectangle(0, 112, LogicalWidth, 68), Color(57, 99, 112));

        const auto repeatLayer = [this, cameraX](const float factor,
                                                  const int spacing,
                                                  const int baseline,
                                                  const int minimumHeight,
                                                  const Color& color) {
            const int offset = static_cast<int>(std::floor(cameraX * factor)) % spacing;
            for (int x = -spacing - offset; x < LogicalWidth + spacing; x += spacing) {
                const int variation = std::abs((x / spacing) * 17) % 22;
                const int height = minimumHeight + variation;
                FillRectangle(Rectangle(x, baseline - height,
                                        spacing - 5, height), color);
            }
        };

        const auto& factors = world_.ParallaxFactors();
        repeatLayer(factors[0], 78, 116, 22, Color(60, 83, 112));
        repeatLayer(factors[1], 54, 130, 30, Color(58, 107, 104));
        repeatLayer(factors[2], 38, 146, 20, Color(70, 126, 95));

        const int cloudOffset =
            static_cast<int>(std::floor(cameraX * factors[0])) % 110;
        for (int x = 20 - cloudOffset; x < LogicalWidth + 80; x += 110) {
            FillRectangle(Rectangle(x, 26, 42, 7), Color(181, 211, 196));
            FillRectangle(Rectangle(x + 9, 21, 25, 6), Color(205, 225, 207));
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
                    static_cast<float>(y * TileMap::TileSize), cameraY);
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

        const Color coat = player.Motion == PlayerMotion::Running
            ? Color(232, 154, 48)
            : Color(205, 119, 42);
        FillRectangle(Rectangle(x + 2, y, 8, 5), Color(217, 189, 143));
        FillRectangle(Rectangle(x + 1, y + 5, 10, 10), coat);
        FillRectangle(Rectangle(x + 2, y + 15, 3, 5), Color(53, 81, 94));
        FillRectangle(Rectangle(x + 7, y + 15, 3, 5), Color(53, 81, 94));
        const int eyeX = player.FacingRight ? x + 8 : x + 3;
        FillRectangle(Rectangle(eyeX, y + 2, 2, 2), Color(24, 36, 42));
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

    void CopperBootsGame::FillRectangle(const Rectangle& rectangle,
                                         const Color& color)
    {
        spriteBatch_->Draw(*solidTexture_, rectangle, color);
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
