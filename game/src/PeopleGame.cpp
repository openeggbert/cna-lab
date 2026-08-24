#include "People/PeopleGame.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <ranges>
#include <span>
#include <variant>
#include <vector>

#include "People/Rendering/RenderOrder.hpp"
#include "People/Rendering/WallPresentation.hpp"
#include "People/World/RoomMap.hpp"
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
using People::World::WallEdge;
using People::Rendering::DrawLayer;
using People::Rendering::RenderKey;
using People::Rendering::RenderOrder;
using People::Rendering::WallPresentation;

namespace
{
    constexpr int WallHeight = 72;

    [[nodiscard]] bool WasPressed(
        const KeyboardState& current, const KeyboardState& previous, const Keys key)
    {
        return current.IsKeyDown(key) && previous.IsKeyUp(key);
    }

    [[nodiscard]] std::uint64_t StableWallId(const WallEdge wall)
    {
        std::uint64_t value = 1469598103934665603ULL;
        const auto mix = [&value](const std::uint32_t field) {
            value ^= field;
            value *= 1099511628211ULL;
        };
        mix(static_cast<std::uint32_t>(wall.floor));
        mix(static_cast<std::uint32_t>(wall.y));
        mix(static_cast<std::uint32_t>(wall.x));
        mix(static_cast<std::uint32_t>(wall.axis));
        return value;
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
    InitializeDemoLot();
}

void PeopleGame::InitializeDemoLot()
{
    constexpr int minimumX = 6;
    constexpr int maximumX = 12;
    constexpr int minimumY = 5;
    constexpr int maximumY = 11;
    for (int y = minimumY; y <= maximumY; ++y)
    {
        for (int x = minimumX; x <= maximumX; ++x)
            (void)lot_.SetFloorCovering({x, y, 0}, std::string("people.floor.demo_warmwood"));
    }
    for (int x = minimumX; x <= maximumX; ++x)
    {
        (void)lot_.AddWall({x, minimumY, 0}, People::World::TileEdge::MinY);
        (void)lot_.AddWall({x, maximumY, 0}, People::World::TileEdge::MaxY);
    }
    for (int y = minimumY; y <= maximumY; ++y)
    {
        (void)lot_.AddWall({minimumX, y, 0}, People::World::TileEdge::MinX);
        (void)lot_.AddWall({maximumX, y, 0}, People::World::TileEdge::MaxX);
    }
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

Texture2D PeopleGame::CreateWallTexture(const bool slopesDownRight)
{
    auto& device = getGraphicsDeviceProperty();
    constexpr int width = IsometricProjection::HalfTileWidth + 1;
    constexpr int height = WallHeight + IsometricProjection::HalfTileHeight + 1;
    Texture2D texture(device, width, height);
    std::vector<Color> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
        Color::Transparent);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const double slope = static_cast<double>(x)
                * IsometricProjection::HalfTileHeight
                / IsometricProjection::HalfTileWidth;
            const double top = slopesDownRight
                ? slope : IsometricProjection::HalfTileHeight - slope;
            const double bottom = top + WallHeight;
            if (static_cast<double>(y) < top || static_cast<double>(y) > bottom)
                continue;

            const bool border = static_cast<double>(y) < top + 2.0
                || static_cast<double>(y) > bottom - 2.0
                || x < 2 || x >= width - 2;
            const bool lowerPanel = static_cast<double>(y) > bottom - 18.0;
            const Color color = border
                ? Color(104, 79, 59, 255)
                : (lowerPanel ? Color(178, 151, 111, 255) : Color(226, 211, 181, 255));
            pixels[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = color;
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
    wallDownRightTexture_ = CreateWallTexture(true);
    wallUpRightTexture_ = CreateWallTexture(false);

    const auto& viewport = device.getViewportProperty();
    camera_.origin = {
        static_cast<double>(viewport.getWidthProperty()) * 0.5,
        static_cast<double>(viewport.getHeightProperty()) * 0.10
    };
    camera_.zoom = 0.62;
    camera_.rotation = ViewRotation::North;

    const auto mouse = Mouse::GetState();
    previousWheel_ = mouse.getScrollWheelValueProperty();

    const People::World::RoomMap rooms = People::World::RoomMap::Rebuild(lot_, 0);
    std::cout << "People: CNA SpriteBatch foundation loaded; lot="
              << lot_.Size().width << 'x' << lot_.Size().height << ", tile="
              << IsometricProjection::TileWidth << 'x' << IsometricProjection::TileHeight
              << ", walls=" << lot_.Walls().size()
              << ", enclosed-rooms=" << rooms.EnclosedRoomCount()
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
    using Drawable = std::variant<TileCoordinate, WallEdge>;
    struct WorldDrawItem
    {
        Drawable drawable;
        RenderKey key;
    };

    std::vector<WorldDrawItem> items;
    const People::World::LotSize lotSize = lot_.Size();
    items.reserve(static_cast<std::size_t>(lotSize.width * lotSize.height)
                  + lot_.Walls().size());
    for (int y = 0; y < lotSize.height; ++y)
    {
        for (int x = 0; x < lotSize.width; ++x)
        {
            const TileCoordinate tile{x, y, 0};
            const std::uint64_t stableId = static_cast<std::uint64_t>(
                y * lotSize.width + x + 1);
            items.push_back({
                tile,
                RenderOrder::BuildKey(
                    std::span<const TileCoordinate>(&tile, 1), tile, lotSize,
                    camera_.rotation, DrawLayer::Terrain, 0, stableId)
            });
        }
    }

    for (const WallEdge wall : lot_.Walls())
    {
        const People::Rendering::WallRenderDescriptor descriptor =
            WallPresentation::Describe(lot_, wall, camera_.rotation);

        items.push_back({
            wall,
            RenderOrder::BuildKey(
                descriptor.footprint, descriptor.sortAnchor, lotSize, camera_.rotation,
                descriptor.layer, 0, StableWallId(wall))
        });
    }

    std::ranges::sort(items, [](const WorldDrawItem& left, const WorldDrawItem& right) {
        return left.key < right.key;
    });

    spriteBatch_->Begin(
        SpriteSortMode::Deferred,
        BlendState::AlphaBlend,
        const_cast<SamplerState*>(&SamplerState::PointClamp),
        nullptr, nullptr);

    for (const WorldDrawItem& item : items)
    {
        if (const auto* tile = std::get_if<TileCoordinate>(&item.drawable))
            DrawTile(*tile);
        else
            DrawWall(std::get<WallEdge>(item.drawable));
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

void PeopleGame::DrawTile(const TileCoordinate tile)
{
    const PixelPoint center = IsometricProjection::WorldToScreen(tile, lot_.Size(), camera_);
    const Vector2 topLeft{
        static_cast<float>(center.x - IsometricProjection::HalfTileWidth * camera_.zoom),
        static_cast<float>(center.y - IsometricProjection::HalfTileHeight * camera_.zoom)
    };
    const bool alternate = (tile.x + tile.y) % 2 != 0;
    const People::World::FloorTileState& floor = lot_.FloorAt(tile);
    const Color tint = floor.floorCoveringId.has_value()
        ? (alternate ? Color(181, 134, 86, 255) : Color(199, 153, 99, 255))
        : (floor.terrain == People::World::TerrainKind::Grass
            ? (alternate ? Color(166, 195, 150, 255) : Color(183, 209, 165, 255))
            : (floor.terrain == People::World::TerrainKind::Soil
                ? Color(164, 125, 82, 255)
                : Color(151, 157, 160, 255)));
    spriteBatch_->Draw(
        tileTexture_, topLeft, std::nullopt, tint,
        0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
        SpriteEffects::None, 0.0f);
}

void PeopleGame::DrawWall(const WallEdge wall)
{
    const People::Rendering::WallRenderDescriptor descriptor =
        WallPresentation::Describe(lot_, wall, camera_.rotation);
    PixelPoint left = IsometricProjection::WorldPointToScreen(
        descriptor.endpoints[0], lot_.Size(), camera_);
    PixelPoint right = IsometricProjection::WorldPointToScreen(
        descriptor.endpoints[1], lot_.Size(), camera_);
    if (left.x > right.x)
        std::swap(left, right);
    const bool slopesDownRight = right.y > left.y;
    const Vector2 topLeft{
        static_cast<float>(left.x),
        static_cast<float>(std::min(left.y, right.y) - WallHeight * camera_.zoom)
    };
    const Texture2D& texture = slopesDownRight ? wallDownRightTexture_ : wallUpRightTexture_;
    spriteBatch_->Draw(
        texture, topLeft, std::nullopt, Color::White,
        0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
        SpriteEffects::None, 0.0f);
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
