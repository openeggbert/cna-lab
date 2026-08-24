#include "People/PeopleGame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <vector>

#include "People/Rendering/RenderOrder.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;
using namespace Microsoft::Xna::Framework::Input;
using People::World::IsometricProjection;
using People::World::PixelPoint;
using People::World::TileCoordinate;
using People::World::ViewRotation;
using People::Rendering::DrawLayer;
using People::Rendering::RenderKey;
using People::Rendering::RenderOrder;

namespace
{
    [[nodiscard]] bool WasPressed(
        const KeyboardState& current, const KeyboardState& previous, const Keys key)
    {
        return current.IsKeyDown(key) && previous.IsKeyUp(key);
    }

}

PeopleGame::PeopleGame(const int smokeFrames, const bool smokeRotations)
    : graphics_(this),
      smokeFrames_(std::max(0, smokeFrames)),
      smokeRotations_(smokeRotations)
{
    graphics_.setPreferredBackBufferWidthProperty(1280);
    graphics_.setPreferredBackBufferHeightProperty(720);
    setIsMouseVisibleProperty(true);
    getWindowProperty().setTitleProperty("People - isometric foundation");
}

Texture2D PeopleGame::CreateTileTexture(const bool highlight)
{
    auto& device = getGraphicsDeviceProperty();
    Texture2D texture(device, IsometricProjection::TileWidth, IsometricProjection::TileHeight);
    std::vector<Color> pixels(
        static_cast<std::size_t>(IsometricProjection::TileWidth)
            * static_cast<std::size_t>(IsometricProjection::TileHeight),
        Color::Transparent);

    for (int y = 0; y < IsometricProjection::TileHeight; ++y)
    {
        for (int x = 0; x < IsometricProjection::TileWidth; ++x)
        {
            const double normalizedX = std::abs(
                (static_cast<double>(x) + 0.5 - IsometricProjection::HalfTileWidth)
                / IsometricProjection::HalfTileWidth);
            const double normalizedY = std::abs(
                (static_cast<double>(y) + 0.5 - IsometricProjection::HalfTileHeight)
                / IsometricProjection::HalfTileHeight);
            const double distance = normalizedX + normalizedY;
            if (distance > 1.0)
                continue;

            Color color = Color::Transparent;
            if (highlight)
            {
                color = distance > 0.87
                    ? Color::FromNonPremultiplied(255, 224, 72, 230)
                    : Color::FromNonPremultiplied(255, 216, 48, 72);
            }
            else
            {
                color = distance > 0.92
                    ? Color(42, 73, 49, 255)
                    : Color(224, 239, 215, 255);
            }

            pixels[static_cast<std::size_t>(y) * IsometricProjection::TileWidth
                   + static_cast<std::size_t>(x)] = color;
        }
    }

    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
    return texture;
}

void PeopleGame::LoadContent()
{
    auto& device = getGraphicsDeviceProperty();
    spriteBatch_ = std::make_unique<SpriteBatch>(device);
    tileTexture_ = CreateTileTexture(false);
    highlightTexture_ = CreateTileTexture(true);

    const auto& viewport = device.getViewportProperty();
    camera_.origin = {
        static_cast<double>(viewport.getWidthProperty()) * 0.5,
        static_cast<double>(viewport.getHeightProperty()) * 0.10
    };
    camera_.zoom = 0.62;
    camera_.rotation = ViewRotation::North;

    const auto mouse = Mouse::GetState();
    previousWheel_ = mouse.getScrollWheelValueProperty();

    std::cout << "People: CNA SpriteBatch foundation loaded; lot="
              << lot_.Size().width << 'x' << lot_.Size().height << ", tile="
              << IsometricProjection::TileWidth << 'x' << IsometricProjection::TileHeight
              << ", runtime world=2D\n";
}

void PeopleGame::ChangeZoom(const double newZoom, const PixelPoint screenFocus)
{
    camera_ = IsometricProjection::ZoomAt(
        camera_, newZoom, screenFocus, MinimumZoom, MaximumZoom);
}

void PeopleGame::ChangeRotation(const int clockwiseQuarterTurns)
{
    camera_ = IsometricProjection::RotateAroundLotCenter(
        camera_, lot_.Size(), clockwiseQuarterTurns);
}

void PeopleGame::HandleCameraInput(const double elapsedSeconds)
{
    const KeyboardState current = Keyboard::GetState();
    if (current.IsKeyDown(Keys::Escape))
    {
        Exit();
        return;
    }

    double panX = 0.0;
    double panY = 0.0;
    if (current.IsKeyDown(Keys::A) || current.IsKeyDown(Keys::Left)) panX += 1.0;
    if (current.IsKeyDown(Keys::D) || current.IsKeyDown(Keys::Right)) panX -= 1.0;
    if (current.IsKeyDown(Keys::W) || current.IsKeyDown(Keys::Up)) panY += 1.0;
    if (current.IsKeyDown(Keys::S) || current.IsKeyDown(Keys::Down)) panY -= 1.0;
    const double magnitude = std::hypot(panX, panY);
    if (magnitude > 0.0)
    {
        camera_.origin.x += panX / magnitude * PanPixelsPerSecond * elapsedSeconds;
        camera_.origin.y += panY / magnitude * PanPixelsPerSecond * elapsedSeconds;
    }

    if (WasPressed(current, previousKeyboard_, Keys::Q)) ChangeRotation(-1);
    if (WasPressed(current, previousKeyboard_, Keys::E)) ChangeRotation(1);

    const auto mouse = Mouse::GetState();
    const PixelPoint mousePoint{
        static_cast<double>(mouse.getXProperty()),
        static_cast<double>(mouse.getYProperty())
    };
    const int wheel = mouse.getScrollWheelValueProperty();
    const int wheelDelta = wheel - previousWheel_;
    if (wheelDelta != 0)
    {
        const double notches = static_cast<double>(wheelDelta) / 120.0;
        ChangeZoom(camera_.zoom * std::pow(1.12, notches), mousePoint);
    }
    if (WasPressed(current, previousKeyboard_, Keys::OemPlus)
        || WasPressed(current, previousKeyboard_, Keys::Add))
        ChangeZoom(camera_.zoom * 1.12, mousePoint);
    if (WasPressed(current, previousKeyboard_, Keys::OemMinus)
        || WasPressed(current, previousKeyboard_, Keys::Subtract))
        ChangeZoom(camera_.zoom / 1.12, mousePoint);

    previousWheel_ = wheel;
    previousKeyboard_ = current;
}

void PeopleGame::RefreshHoveredTile()
{
    const auto mouse = Mouse::GetState();
    hoveredTile_ = IsometricProjection::ScreenToWorld(
        {static_cast<double>(mouse.getXProperty()), static_cast<double>(mouse.getYProperty())},
        lot_.Size(), camera_);
}

void PeopleGame::Update(GameTime& gameTime)
{
    const double elapsedSeconds = std::min(
        0.1,
        gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty() / 1000.0);
    HandleCameraInput(elapsedSeconds);
    RefreshHoveredTile();
}

void PeopleGame::DrawLot()
{
    struct TileDrawItem
    {
        TileCoordinate tile;
        RenderKey key;
    };

    std::vector<TileDrawItem> tiles;
    const People::World::LotSize lotSize = lot_.Size();
    tiles.reserve(static_cast<std::size_t>(lotSize.width * lotSize.height));
    for (int y = 0; y < lotSize.height; ++y)
    {
        for (int x = 0; x < lotSize.width; ++x)
        {
            const TileCoordinate tile{x, y, 0};
            const std::uint64_t stableId = static_cast<std::uint64_t>(
                y * lotSize.width + x + 1);
            tiles.push_back({
                tile,
                RenderOrder::BuildKey(
                    std::span<const TileCoordinate>(&tile, 1), tile, lotSize,
                    camera_.rotation, DrawLayer::Terrain, 0, stableId)
            });
        }
    }

    std::ranges::sort(tiles, [](const TileDrawItem& left, const TileDrawItem& right) {
        return left.key < right.key;
    });

    spriteBatch_->Begin(
        SpriteSortMode::Deferred,
        BlendState::AlphaBlend,
        const_cast<SamplerState*>(&SamplerState::PointClamp),
        nullptr, nullptr);

    for (const TileDrawItem& item : tiles)
    {
        const TileCoordinate tile = item.tile;
        const PixelPoint center = IsometricProjection::WorldToScreen(tile, lotSize, camera_);
        const Vector2 topLeft{
            static_cast<float>(center.x - IsometricProjection::HalfTileWidth * camera_.zoom),
            static_cast<float>(center.y - IsometricProjection::HalfTileHeight * camera_.zoom)
        };
        const bool alternate = (tile.x + tile.y) % 2 != 0;
        const People::World::TerrainKind terrain = lot_.FloorAt(tile).terrain;
        const Color tint = terrain == People::World::TerrainKind::Grass
            ? (alternate ? Color(166, 195, 150, 255) : Color(183, 209, 165, 255))
            : (terrain == People::World::TerrainKind::Soil
                ? Color(164, 125, 82, 255)
                : Color(151, 157, 160, 255));
        spriteBatch_->Draw(
            tileTexture_, topLeft, std::nullopt, tint,
            0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
            SpriteEffects::None, 0.0f);
    }

    if (hoveredTile_.has_value())
    {
        const PixelPoint center = IsometricProjection::WorldToScreen(
            *hoveredTile_, lotSize, camera_);
        const Vector2 topLeft{
            static_cast<float>(center.x - IsometricProjection::HalfTileWidth * camera_.zoom),
            static_cast<float>(center.y - IsometricProjection::HalfTileHeight * camera_.zoom)
        };
        spriteBatch_->Draw(
            highlightTexture_, topLeft, std::nullopt, Color::White,
            0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
            SpriteEffects::None, 0.0f);
    }

    spriteBatch_->End();
}

void PeopleGame::Draw(const GameTime& gameTime)
{
    (void)gameTime;
    getGraphicsDeviceProperty().Clear(Color(36, 58, 70, 255));
    DrawLot();

    ++drawnFrames_;
    if (smokeRotations_ && drawnFrames_ < smokeFrames_)
        ChangeRotation(1);
    if (smokeFrames_ > 0 && drawnFrames_ >= smokeFrames_)
    {
        std::cout << "People: smoke test rendered " << drawnFrames_ << " frames; rotation="
                  << static_cast<int>(camera_.rotation)
                  << "; rotation-cycle=" << (smokeRotations_ ? "yes" : "no")
                  << "; zoom=" << camera_.zoom
                  << "; origin=" << camera_.origin.x << ',' << camera_.origin.y
                  << "; hovered=";
        if (hoveredTile_.has_value())
            std::cout << hoveredTile_->x << ',' << hoveredTile_->y;
        else
            std::cout << "outside";
        std::cout << "\n";
        Exit();
    }
}

GetTypeNameCPP(PeopleGame, "PeopleGame")
