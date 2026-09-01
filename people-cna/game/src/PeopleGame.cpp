#include "People/PeopleGame.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

#include "People/Content/DemoFurniture.hpp"
#include "People/Content/DemoResident.hpp"
#include "People/Rendering/ObjectPresentation.hpp"
#include "People/Rendering/ResidentPresentation.hpp"
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
using People::Objects::ObjectInstanceId;
using People::Rendering::DrawLayer;
using People::Rendering::ObjectPresentation;
using People::Rendering::RenderKey;
using People::Rendering::RenderOrder;
using People::Rendering::ResidentPresentation;
using People::Rendering::SpriteDirection;
using People::Rendering::WallPresentation;

namespace
{
    constexpr int WallHeight = 72;
    constexpr int FurnitureSpriteWidth = 128;
    constexpr int FurnitureSpriteHeight = 128;

    struct RasterPoint
    {
        int x = 0;
        int y = 0;
    };

    [[nodiscard]] RasterPoint Add(const RasterPoint left, const RasterPoint right)
    {
        return {left.x + right.x, left.y + right.y};
    }

    [[nodiscard]] RasterPoint Subtract(const RasterPoint left, const RasterPoint right)
    {
        return {left.x - right.x, left.y - right.y};
    }

    [[nodiscard]] RasterPoint Scale(
        const RasterPoint value, const int numerator, const int denominator)
    {
        return {value.x * numerator / denominator, value.y * numerator / denominator};
    }

    class PixelCanvas final
    {
    public:
        PixelCanvas(const int width, const int height)
            : width_(width),
              height_(height),
              pixels_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                      Color::Transparent)
        {
        }

        void Put(const int x, const int y, const Color color)
        {
            if (x < 0 || y < 0 || x >= width_ || y >= height_)
                return;
            pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                    + static_cast<std::size_t>(x)] = color;
        }

        void FillRectangle(
            const int minimumX, const int minimumY,
            const int maximumX, const int maximumY,
            const Color color)
        {
            for (int y = minimumY; y <= maximumY; ++y)
            {
                for (int x = minimumX; x <= maximumX; ++x)
                    Put(x, y, color);
            }
        }

        void FillEllipse(
            const RasterPoint center, const int radiusX, const int radiusY,
            const Color color)
        {
            for (int y = center.y - radiusY; y <= center.y + radiusY; ++y)
            {
                for (int x = center.x - radiusX; x <= center.x + radiusX; ++x)
                {
                    const double normalizedX = static_cast<double>(x - center.x) / radiusX;
                    const double normalizedY = static_cast<double>(y - center.y) / radiusY;
                    if (normalizedX * normalizedX + normalizedY * normalizedY <= 1.0)
                        Put(x, y, color);
                }
            }
        }

        void FillPolygon(const std::span<const RasterPoint> points, const Color color)
        {
            if (points.size() < 3)
                return;
            int minimumY = std::numeric_limits<int>::max();
            int maximumY = std::numeric_limits<int>::min();
            for (const RasterPoint point : points)
            {
                minimumY = std::min(minimumY, point.y);
                maximumY = std::max(maximumY, point.y);
            }

            std::vector<double> intersections;
            intersections.reserve(points.size());
            for (int y = minimumY; y <= maximumY; ++y)
            {
                intersections.clear();
                const double scanY = static_cast<double>(y) + 0.5;
                for (std::size_t index = 0; index < points.size(); ++index)
                {
                    const RasterPoint first = points[index];
                    const RasterPoint second = points[(index + 1) % points.size()];
                    const bool crosses = (static_cast<double>(first.y) <= scanY
                                          && static_cast<double>(second.y) > scanY)
                        || (static_cast<double>(second.y) <= scanY
                            && static_cast<double>(first.y) > scanY);
                    if (!crosses)
                        continue;
                    const double ratio = (scanY - first.y)
                        / static_cast<double>(second.y - first.y);
                    intersections.push_back(
                        static_cast<double>(first.x) + ratio * (second.x - first.x));
                }
                std::ranges::sort(intersections);
                for (std::size_t index = 0; index + 1 < intersections.size(); index += 2)
                {
                    const int firstX = static_cast<int>(std::ceil(intersections[index]));
                    const int lastX = static_cast<int>(std::floor(intersections[index + 1]));
                    for (int x = firstX; x <= lastX; ++x)
                        Put(x, y, color);
                }
            }
        }

        void DrawLine(
            RasterPoint start, const RasterPoint end, const int radius, const Color color)
        {
            const int differenceX = std::abs(end.x - start.x);
            const int stepX = start.x < end.x ? 1 : -1;
            const int differenceY = -std::abs(end.y - start.y);
            const int stepY = start.y < end.y ? 1 : -1;
            int error = differenceX + differenceY;
            while (true)
            {
                FillEllipse(start, radius, radius, color);
                if (start.x == end.x && start.y == end.y)
                    break;
                const int doubledError = error * 2;
                if (doubledError >= differenceY)
                {
                    error += differenceY;
                    start.x += stepX;
                }
                if (doubledError <= differenceX)
                {
                    error += differenceX;
                    start.y += stepY;
                }
            }
        }

        [[nodiscard]] const std::vector<Color>& Pixels() const noexcept
        {
            return pixels_;
        }

    private:
        int width_;
        int height_;
        std::vector<Color> pixels_;
    };

    struct FurnitureBasis
    {
        RasterPoint forward;
        RasterPoint right;
    };

    [[nodiscard]] FurnitureBasis BasisFor(const SpriteDirection direction)
    {
        switch (direction)
        {
            case SpriteDirection::North: return {{36, -18}, {24, 12}};
            case SpriteDirection::East: return {{36, 18}, {-24, 12}};
            case SpriteDirection::South: return {{-36, 18}, {-24, -12}};
            case SpriteDirection::West: return {{-36, -18}, {24, -12}};
        }
        throw std::invalid_argument("sprite direction must be one of four directions");
    }

    [[nodiscard]] std::array<RasterPoint, 4> PlanarQuad(
        const RasterPoint center,
        const RasterPoint forward,
        const RasterPoint right)
    {
        return {{
            Subtract(Subtract(center, forward), right),
            Add(Subtract(center, forward), right),
            Add(Add(center, forward), right),
            Subtract(Add(center, forward), right)
        }};
    }

    [[nodiscard]] std::array<RasterPoint, 4> Raised(
        std::array<RasterPoint, 4> points, const int height)
    {
        for (RasterPoint& point : points)
            point.y -= height;
        return points;
    }

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

PeopleGame::PeopleGame(
    const int smokeFrames,
    const bool smokeRotations,
    const bool smokeWalk)
    : graphics_(this),
      smokeFrames_(std::max(0, smokeFrames)),
      smokeRotations_(smokeRotations),
      smokeWalk_(smokeWalk)
{
    graphics_.setPreferredBackBufferWidthProperty(1280);
    graphics_.setPreferredBackBufferHeightProperty(720);
    setIsMouseVisibleProperty(true);
    getWindowProperty().setTitleProperty("People - resident routing milestone");
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
    const TileCoordinate doorTile{9, maximumY, 0};
    (void)lot_.AddDoor(doorTile, People::World::TileEdge::MaxY);
    demoDoor_ = lot_.CanonicalWall(doorTile, People::World::TileEdge::MaxY);
    People::Content::DemoFurniture::Populate(objects_);
    const People::Simulation::ResidentMutationResult resident =
        residents_.Add(People::Content::DemoResident::MaraState());
    if (!resident.IsValid())
        throw std::logic_error("demo resident failed active-lot validation");
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

Texture2D PeopleGame::CreateDoorTexture(
    const bool slopesDownRight, const bool open)
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
            const double vertical = static_cast<double>(y) - top;
            if (vertical < 0.0 || vertical > WallHeight)
                continue;

            const bool doorway = x >= 7 && x <= width - 8 && vertical >= 9.0;
            const bool opening = open && x >= 12 && x <= width - 13 && vertical >= 15.0;
            if (opening)
                continue;

            Color color = Color(226, 211, 181, 255);
            if (doorway)
            {
                const bool frame = x < 11 || x > width - 12 || vertical < 14.0;
                if (frame)
                    color = Color(92, 61, 39, 255);
                else if (open)
                    color = Color(63, 45, 34, 255);
                else
                {
                    const bool panelEdge = x < 14 || x > width - 15
                        || vertical < 19.0 || vertical > WallHeight - 5.0;
                    color = panelEdge
                        ? Color(103, 65, 38, 255)
                        : Color(147, 91, 49, 255);
                    const double knobX = slopesDownRight ? width - 17.0 : 16.0;
                    if (std::abs(static_cast<double>(x) - knobX) < 1.5
                        && std::abs(vertical - 43.0) < 1.5)
                        color = Color(221, 177, 67, 255);
                }
            }
            else if (vertical > WallHeight - 18.0)
                color = Color(178, 151, 111, 255);

            const bool outerBorder = vertical < 2.0 || vertical > WallHeight - 2.0
                || x < 2 || x >= width - 2;
            if (outerBorder)
                color = Color(104, 79, 59, 255);
            pixels[static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)] = color;
        }
    }

    texture.SetData(pixels.data(), static_cast<int>(pixels.size()));
    return texture;
}

Texture2D PeopleGame::CreateFurnitureTexture(
    const std::string_view definitionId, const SpriteDirection direction)
{
    PixelCanvas canvas(FurnitureSpriteWidth, FurnitureSpriteHeight);
    const FurnitureBasis basis = BasisFor(direction);
    constexpr RasterPoint contact{64, 96};
    const Color shadow = Color::FromNonPremultiplied(27, 38, 43, 82);
    const Color outline(54, 54, 54, 255);

    if (definitionId == People::Content::DemoFurniture::BedId)
    {
        const RasterPoint length = Scale(basis.forward, 5, 8);
        const RasterPoint width = Scale(basis.right, 3, 4);
        const RasterPoint center = Add(contact, length);
        const auto floorShape = PlanarQuad(center, length, width);
        canvas.FillPolygon(floorShape, shadow);

        const auto frame = Raised(floorShape, 8);
        canvas.FillPolygon(frame, Color(119, 74, 48, 255));
        for (std::size_t index = 0; index < frame.size(); ++index)
            canvas.DrawLine(frame[index], frame[(index + 1) % frame.size()], 1, outline);

        auto mattress = Raised(frame, 7);
        for (RasterPoint& point : mattress)
        {
            point = Add(point, Scale(Subtract(center, point), 1, 10));
        }
        canvas.FillPolygon(mattress, Color(203, 112, 92, 255));
        for (std::size_t index = 0; index < mattress.size(); ++index)
            canvas.DrawLine(mattress[index], mattress[(index + 1) % mattress.size()], 1, outline);

        const RasterPoint headCenter = Add(contact, Scale(basis.forward, 5, 4));
        const RasterPoint headSide = Scale(basis.right, 3, 4);
        const RasterPoint headLeft = Subtract(headCenter, headSide);
        const RasterPoint headRight = Add(headCenter, headSide);
        canvas.DrawLine(headLeft, {headLeft.x, headLeft.y - 29}, 2, Color(91, 56, 38, 255));
        canvas.DrawLine(headRight, {headRight.x, headRight.y - 29}, 2, Color(91, 56, 38, 255));
        canvas.DrawLine({headLeft.x, headLeft.y - 26},
                        {headRight.x, headRight.y - 26}, 2, Color(134, 82, 50, 255));

        const RasterPoint pillowCenter = Add(contact, basis.forward);
        auto pillow = Raised(
            PlanarQuad({pillowCenter.x, pillowCenter.y},
                       Scale(basis.forward, 1, 5), Scale(basis.right, 1, 2)),
            18);
        canvas.FillPolygon(pillow, Color(244, 225, 181, 255));
        for (std::size_t index = 0; index < pillow.size(); ++index)
            canvas.DrawLine(pillow[index], pillow[(index + 1) % pillow.size()], 1, outline);
    }
    else if (definitionId == People::Content::DemoFurniture::ChairId)
    {
        canvas.FillEllipse(contact, 19, 8, shadow);
        const RasterPoint seatCenter{contact.x, contact.y - 31};
        const RasterPoint seatForward = Scale(basis.forward, 1, 5);
        const RasterPoint seatRight = Scale(basis.right, 2, 5);
        const auto ground = PlanarQuad(contact, seatForward, seatRight);
        const auto seat = PlanarQuad(seatCenter, seatForward, seatRight);
        for (std::size_t index = 0; index < ground.size(); ++index)
            canvas.DrawLine(ground[index], seat[index], 2, Color(117, 77, 40, 255));
        canvas.FillPolygon(seat, Color(226, 171, 65, 255));
        for (std::size_t index = 0; index < seat.size(); ++index)
            canvas.DrawLine(seat[index], seat[(index + 1) % seat.size()], 1, outline);

        const RasterPoint backCenter = Subtract(seatCenter, seatForward);
        const RasterPoint backSide = seatRight;
        const RasterPoint backLeft = Subtract(backCenter, backSide);
        const RasterPoint backRight = Add(backCenter, backSide);
        canvas.DrawLine(backLeft, {backLeft.x, backLeft.y - 28}, 2,
                        Color(117, 77, 40, 255));
        canvas.DrawLine(backRight, {backRight.x, backRight.y - 28}, 2,
                        Color(117, 77, 40, 255));
        canvas.DrawLine({backLeft.x, backLeft.y - 25},
                        {backRight.x, backRight.y - 25}, 3,
                        Color(226, 171, 65, 255));
    }
    else if (definitionId == People::Content::DemoFurniture::TableId)
    {
        canvas.FillEllipse(contact, 28, 11, shadow);
        const RasterPoint topCenter{contact.x, contact.y - 43};
        const RasterPoint topForward = Scale(basis.forward, 2, 5);
        const RasterPoint topRight = Scale(basis.right, 3, 5);
        const auto ground = PlanarQuad(contact, Scale(topForward, 2, 3),
                                      Scale(topRight, 2, 3));
        const auto top = PlanarQuad(topCenter, topForward, topRight);
        for (std::size_t index = 0; index < ground.size(); ++index)
            canvas.DrawLine(ground[index], top[index], 2, Color(66, 81, 65, 255));
        canvas.FillPolygon(top, Color(94, 139, 96, 255));
        for (std::size_t index = 0; index < top.size(); ++index)
            canvas.DrawLine(top[index], top[(index + 1) % top.size()], 1, outline);
        canvas.DrawLine(Subtract(topCenter, Scale(basis.right, 1, 3)),
                        Add(topCenter, Scale(basis.right, 1, 3)), 1,
                        Color(192, 202, 138, 255));
    }
    else if (definitionId == People::Content::DemoFurniture::RefrigeratorId)
    {
        canvas.FillEllipse(contact, 25, 9, shadow);
        const bool rightLit = direction == SpriteDirection::North
            || direction == SpriteDirection::East;
        const int sideOffset = rightLit ? 9 : -9;
        const std::array<RasterPoint, 4> front{{
            {43, 34}, {84, 34}, {84, 96}, {43, 96}
        }};
        const std::array<RasterPoint, 4> side = rightLit
            ? std::array<RasterPoint, 4>{{
                {84, 34}, {84 + sideOffset, 29}, {84 + sideOffset, 90}, {84, 96}
            }}
            : std::array<RasterPoint, 4>{{
                {43, 34}, {43 + sideOffset, 29}, {43 + sideOffset, 90}, {43, 96}
            }};
        canvas.FillPolygon(front, Color(162, 210, 189, 255));
        canvas.FillPolygon(side, rightLit
            ? Color(126, 177, 162, 255) : Color(105, 156, 145, 255));
        for (std::size_t index = 0; index < front.size(); ++index)
            canvas.DrawLine(front[index], front[(index + 1) % front.size()], 1, outline);
        for (std::size_t index = 0; index < side.size(); ++index)
            canvas.DrawLine(side[index], side[(index + 1) % side.size()], 1, outline);
        canvas.DrawLine({44, 58}, {83, 58}, 1, Color(79, 115, 106, 255));
        const int handleX = direction == SpriteDirection::North
            || direction == SpriteDirection::West ? 50 : 77;
        canvas.DrawLine({handleX, 64}, {handleX, 82}, 1, Color(236, 229, 190, 255));
        const int badgeX = direction == SpriteDirection::North
            || direction == SpriteDirection::East ? 72 : 54;
        canvas.FillRectangle(badgeX - 2, 43, badgeX + 2, 47, Color(229, 171, 65, 255));
    }
    else if (definitionId == People::Content::DemoFurniture::ToiletId)
    {
        canvas.FillEllipse(contact, 21, 8, shadow);
        const RasterPoint back = Subtract(contact, Scale(basis.forward, 2, 5));
        canvas.FillRectangle(back.x - 14, back.y - 55, back.x + 14, back.y - 30,
                             Color(193, 222, 226, 255));
        canvas.DrawLine({back.x - 14, back.y - 55}, {back.x + 14, back.y - 55}, 1,
                        outline);
        const RasterPoint bowl = Add(contact, Scale(basis.forward, 1, 5));
        canvas.FillEllipse({bowl.x, bowl.y - 25}, 21, 12, Color(220, 240, 238, 255));
        canvas.FillEllipse({bowl.x, bowl.y - 26}, 13, 6, Color(95, 151, 159, 255));
        canvas.FillEllipse({bowl.x, bowl.y - 26}, 9, 4, Color(168, 215, 218, 255));
        canvas.FillRectangle(bowl.x - 8, bowl.y - 24, bowl.x + 8, contact.y - 2,
                             Color(193, 222, 226, 255));
        canvas.DrawLine({back.x - 14, back.y - 30}, {back.x + 14, back.y - 30}, 1,
                        outline);
        const int buttonX = back.x + (direction == SpriteDirection::North
            || direction == SpriteDirection::East ? 7 : -7);
        canvas.FillEllipse({buttonX, back.y - 51}, 2, 1, Color(92, 145, 151, 255));
    }
    else
        throw std::invalid_argument("unknown procedural furniture definition");

    Texture2D texture(
        getGraphicsDeviceProperty(), FurnitureSpriteWidth, FurnitureSpriteHeight);
    texture.SetData(canvas.Pixels().data(), static_cast<int>(canvas.Pixels().size()));
    return texture;
}

Texture2D PeopleGame::CreateResidentTexture(
    const SpriteDirection direction,
    const int walkPhase)
{
    constexpr int width = 64;
    constexpr int height = 96;
    constexpr RasterPoint footAnchor{32, 88};
    PixelCanvas canvas(width, height);
    const Color outline(48, 48, 52, 255);
    const Color skin(188, 127, 91, 255);
    const Color hair(63, 39, 31, 255);
    const Color shirt(51, 151, 151, 255);
    const Color trousers(65, 73, 112, 255);
    canvas.FillEllipse(footAnchor, 14, 5, Color::FromNonPremultiplied(24, 34, 38, 82));

    int turn = static_cast<int>(direction);
    if (turn < 0 || turn > 3)
        throw std::invalid_argument("resident sprite direction must be one of four directions");
    const int stride = turn % 2 == 0 ? 6 : 4;
    const int lean = turn == static_cast<int>(SpriteDirection::East)
        ? 3 : (turn == static_cast<int>(SpriteDirection::West) ? -3 : 0);

    // Walk frames swing legs and arms in opposition around the shared anchor.
    // The contact foot keeps the anchor row so the resident never floats or
    // sinks when the runtime switches between the idle and walk clips.
    if (walkPhase < -1 || walkPhase >= static_cast<int>(
            People::Rendering::ResidentWalkSpriteSet::FrameCount))
        throw std::invalid_argument("resident walk phase must be idle or an authored frame");
    const int swing = walkPhase < 0 ? 0 : (walkPhase == 0 ? 5 : -5);
    const int leftLift = swing < 0 ? 3 : 0;
    const int rightLift = swing > 0 ? 3 : 0;

    const RasterPoint leftFoot{footAnchor.x - stride + swing, footAnchor.y - leftLift};
    const RasterPoint rightFoot{footAnchor.x + stride - swing, footAnchor.y - 1 - rightLift};
    const RasterPoint leftHip{footAnchor.x - 5 + lean, 64};
    const RasterPoint rightHip{footAnchor.x + 5 + lean, 64};
    canvas.DrawLine(leftHip, leftFoot, 3, trousers);
    canvas.DrawLine(rightHip, rightFoot, 3, Color(54, 61, 96, 255));
    canvas.DrawLine({leftFoot.x - 2, leftFoot.y}, {leftFoot.x + 3, leftFoot.y}, 2, outline);
    canvas.DrawLine({rightFoot.x - 2, rightFoot.y}, {rightFoot.x + 3, rightFoot.y}, 2, outline);

    const std::array<RasterPoint, 4> torso{{
        {20 + lean, 43}, {43 + lean, 43}, {39 + lean, 67}, {24 + lean, 67}
    }};
    canvas.FillPolygon(torso, shirt);
    for (std::size_t index = 0; index < torso.size(); ++index)
        canvas.DrawLine(torso[index], torso[(index + 1) % torso.size()], 1, outline);
    canvas.DrawLine({21 + lean, 48}, {16 + lean - swing, 64 - (swing < 0 ? -swing : swing) / 2}, 3, skin);
    canvas.DrawLine({42 + lean, 48}, {47 + lean + swing, 63 - (swing < 0 ? -swing : swing) / 2}, 3, skin);

    const RasterPoint head{32 + lean, 32};
    canvas.FillEllipse(head, 11, 13, skin);
    canvas.FillEllipse({head.x, head.y - 7}, 12, 8, hair);
    if (direction == SpriteDirection::North)
    {
        canvas.FillRectangle(head.x - 11, head.y - 4, head.x + 11, head.y + 4, hair);
    }
    else if (direction == SpriteDirection::South)
    {
        canvas.FillEllipse({head.x - 4, head.y}, 1, 1, outline);
        canvas.FillEllipse({head.x + 4, head.y}, 1, 1, outline);
        canvas.DrawLine({head.x - 3, head.y + 6}, {head.x + 3, head.y + 6}, 1,
                        Color(121, 66, 61, 255));
    }
    else
    {
        const int faceSide = direction == SpriteDirection::East ? 5 : -5;
        canvas.FillEllipse({head.x + faceSide, head.y}, 1, 1, outline);
        canvas.DrawLine({head.x + faceSide, head.y + 5},
                        {head.x + faceSide + (faceSide > 0 ? 2 : -2), head.y + 5},
                        1, Color(121, 66, 61, 255));
    }

    Texture2D texture(getGraphicsDeviceProperty(), width, height);
    texture.SetData(canvas.Pixels().data(), static_cast<int>(canvas.Pixels().size()));
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
    doorClosedDownRightTexture_ = CreateDoorTexture(true, false);
    doorClosedUpRightTexture_ = CreateDoorTexture(false, false);
    doorOpenDownRightTexture_ = CreateDoorTexture(true, true);
    doorOpenUpRightTexture_ = CreateDoorTexture(false, true);
    for (const auto& [id, instance] : objects_.Instances())
    {
        (void)id;
        const People::Objects::ObjectDefinition* definition =
            objects_.Catalog().Find(instance.definitionId);
        if (definition == nullptr)
            throw std::logic_error("demo object definition disappeared before content load");
        const auto state = definition->visual.states.find(definition->visual.defaultState);
        if (state == definition->visual.states.end())
            throw std::logic_error("demo object has no default visual state");
        for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
        {
            const auto direction = static_cast<SpriteDirection>(directionIndex);
            const People::Objects::ObjectSpriteReference& sprite =
                state->second.directions[static_cast<std::size_t>(directionIndex)];
            objectTextures_.emplace(
                sprite.assetId, CreateFurnitureTexture(definition->id, direction));
        }
    }
    demoResidentSprites_ = People::Content::DemoResident::MaraIdleSprites();
    demoResidentWalkSprites_ = People::Content::DemoResident::MaraWalkSprites();
    for (int directionIndex = 0; directionIndex < 4; ++directionIndex)
    {
        const auto direction = static_cast<SpriteDirection>(directionIndex);
        const auto index = static_cast<std::size_t>(directionIndex);
        residentTextures_.emplace(
            demoResidentSprites_.directions[index].assetId,
            CreateResidentTexture(direction, -1));
        for (std::size_t frame = 0;
             frame < People::Rendering::ResidentWalkSpriteSet::FrameCount; ++frame)
        {
            residentTextures_.emplace(
                demoResidentWalkSprites_.frames[frame][index].assetId,
                CreateResidentTexture(direction, static_cast<int>(frame)));
        }
    }

    const auto& viewport = device.getViewportProperty();
    camera_.origin = {
        static_cast<double>(viewport.getWidthProperty()) * 0.5,
        static_cast<double>(viewport.getHeightProperty()) * 0.10
    };
    camera_.zoom = 0.62;
    camera_.rotation = ViewRotation::North;

    const auto mouse = Mouse::GetState();
    previousWheel_ = mouse.getScrollWheelValueProperty();
    previousLeftButton_ = mouse.getLeftButtonProperty();
    previousRightButton_ = mouse.getRightButtonProperty();

    const People::World::RoomMap rooms = People::World::RoomMap::Rebuild(lot_, 0);
    std::cout << "People: CNA SpriteBatch foundation loaded; lot="
              << lot_.Size().width << 'x' << lot_.Size().height << ", tile="
              << IsometricProjection::TileWidth << 'x' << IsometricProjection::TileHeight
              << ", walls=" << lot_.Walls().size()
              << ", doors=" << lot_.Doors().size()
              << ", objects=" << objects_.Instances().size()
              << ", residents=" << residents_.Residents().size()
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
    if (demoDoor_.has_value() && WasPressed(current, previousKeyboard_, Keys::F))
        (void)lot_.SetDoorOpen(*demoDoor_, !lot_.IsDoorOpen(*demoDoor_));

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

void PeopleGame::IssueDemoMove(const TileCoordinate destination)
{
    const People::Simulation::ResidentState* resident = residents_.Find(
        People::Content::DemoResident::MaraId);
    if (resident == nullptr)
        return;
    if (resident->movementRequest.has_value())
    {
        std::cout << "People: move command ignored; resident is already moving\n";
        return;
    }

    const People::Navigation::StaticNavigationGrid grid =
        People::Navigation::StaticNavigationGrid::Build(lot_, objects_, destination.floor);
    const People::Navigation::PathResult path = People::Navigation::AStarPathfinder::FindPath(
        grid, resident->tile, destination);
    if (!path.Succeeded())
    {
        std::cout << "People: move command rejected; path-failure="
                  << static_cast<int>(path.failure) << '\n';
        return;
    }

    const People::Simulation::MovementRequestId requestId = nextDemoMovementRequest_++;
    const People::Simulation::MovementStartResult started = movement_.Begin(
        requestId, resident->id, path.tiles, grid);
    if (!started.IsValid())
    {
        std::cout << "People: move command rejected; movement-failure="
                  << static_cast<int>(started.failure) << '\n';
        return;
    }
    std::cout << "People: move command resident=" << resident->id
              << "; destination=" << destination.x << ',' << destination.y
              << "; path-tiles=" << path.tiles.size()
              << (started.completedImmediately ? "; already-there=yes" : "") << '\n';
}

void PeopleGame::AdvanceSimulationTick()
{
    const People::Simulation::ResidentState* resident = residents_.Find(
        People::Content::DemoResident::MaraId);
    if (resident == nullptr || !resident->movementRequest.has_value())
        return;

    const People::Navigation::StaticNavigationGrid grid =
        People::Navigation::StaticNavigationGrid::Build(lot_, objects_, resident->tile.floor);
    const People::Simulation::MovementTickResult result = movement_.Advance(
        *resident->movementRequest, grid);
    if (result.status == People::Simulation::MovementTickStatus::Failed)
    {
        std::cout << "People: movement failed; reason="
                  << static_cast<int>(result.failure) << '\n';
    }
    else if (result.status == People::Simulation::MovementTickStatus::Replanned)
    {
        std::cout << "People: movement replanned around changed obstruction\n";
    }
    else if (result.status == People::Simulation::MovementTickStatus::Completed)
    {
        std::cout << "People: movement completed at "
                  << result.tile.x << ',' << result.tile.y << '\n';
    }
}

void PeopleGame::Update(GameTime& gameTime)
{
    const double elapsedSeconds = std::min(
        0.1,
        gameTime.getElapsedGameTimeProperty().getTotalMillisecondsProperty() / 1000.0);
    HandleCameraInput(elapsedSeconds);
    RefreshHoveredTile();
    const MouseState mouse = Mouse::GetState();
    if (mouse.getLeftButtonProperty() == ButtonState::Pressed
        && previousLeftButton_ == ButtonState::Released)
    {
        selectedObject_ = hoveredTile_.has_value()
            ? objects_.OccupiedBy(*hoveredTile_) : std::nullopt;
    }
    if (mouse.getRightButtonProperty() == ButtonState::Pressed
        && previousRightButton_ == ButtonState::Released
        && hoveredTile_.has_value())
    {
        IssueDemoMove(*hoveredTile_);
    }
    previousLeftButton_ = mouse.getLeftButtonProperty();
    previousRightButton_ = mouse.getRightButtonProperty();

    simulationAccumulator_ += elapsedSeconds;
    while (simulationAccumulator_ >= SimulationTickSeconds)
    {
        AdvanceSimulationTick();
        simulationAccumulator_ -= SimulationTickSeconds;
    }
}

void PeopleGame::DrawLot()
{
    struct ObjectDrawable
    {
        ObjectInstanceId id = 0;
    };
    struct ResidentDrawable
    {
        People::Simulation::ResidentId id = 0;
    };
    using Drawable = std::variant<
        TileCoordinate, WallEdge, ObjectDrawable, ResidentDrawable>;
    struct WorldDrawItem
    {
        Drawable drawable;
        RenderKey key;
    };

    std::vector<WorldDrawItem> items;
    const People::World::LotSize lotSize = lot_.Size();
    items.reserve(static_cast<std::size_t>(lotSize.width * lotSize.height)
                  + lot_.Walls().size() + objects_.Instances().size()
                  + residents_.Residents().size());
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

    for (const auto& [id, instance] : objects_.Instances())
    {
        const std::vector<TileCoordinate> footprint = objects_.FootprintCells(instance);
        items.push_back({
            ObjectDrawable{id},
            RenderOrder::BuildKey(
                footprint, instance.anchor, lotSize, camera_.rotation,
                DrawLayer::WorldEntity, 0, id)
        });
    }

    for (const auto& [id, resident] : residents_.Residents())
    {
        const std::array<TileCoordinate, 1> footprint{{resident.tile}};
        items.push_back({
            ResidentDrawable{id},
            RenderOrder::BuildKey(
                footprint, resident.tile, lotSize, camera_.rotation,
                DrawLayer::WorldEntity, 1, (1ULL << 63U) | id)
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
        else if (const auto* wall = std::get_if<WallEdge>(&item.drawable))
            DrawWall(*wall);
        else if (const auto* object = std::get_if<ObjectDrawable>(&item.drawable))
            DrawObject(object->id);
        else
            DrawResident(std::get<ResidentDrawable>(item.drawable).id);
    }

    if (selectedObject_.has_value())
    {
        const People::Objects::ObjectInstance* selected = objects_.Find(*selectedObject_);
        if (selected != nullptr)
        {
            for (const TileCoordinate tile : objects_.FootprintCells(*selected))
            {
                const PixelPoint center = IsometricProjection::WorldToScreen(
                    tile, lotSize, camera_);
                const Vector2 topLeft{
                    static_cast<float>(
                        center.x - IsometricProjection::HalfTileWidth * camera_.zoom),
                    static_cast<float>(
                        center.y - IsometricProjection::HalfTileHeight * camera_.zoom)
                };
                spriteBatch_->Draw(
                    highlightTexture_, topLeft, std::nullopt,
                    Color::FromNonPremultiplied(105, 225, 255, 210),
                    0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
                    SpriteEffects::None, 0.0f);
            }
        }
        else
            selectedObject_.reset();
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

void PeopleGame::DrawObject(const ObjectInstanceId objectId)
{
    const People::Objects::ObjectInstance* instance = objects_.Find(objectId);
    if (instance == nullptr)
        throw std::logic_error("render queue refers to a missing object instance");
    const People::Objects::ObjectDefinition* definition =
        objects_.Catalog().Find(instance->definitionId);
    if (definition == nullptr)
        throw std::logic_error("object instance refers to a missing definition");
    const People::Rendering::ObjectSpriteSelection selection =
        ObjectPresentation::SelectDefaultSprite(
            *definition, instance->rotation, camera_.rotation);
    if (selection.reference == nullptr)
        throw std::logic_error("object sprite selection returned no metadata");
    const auto texture = objectTextures_.find(selection.reference->assetId);
    if (texture == objectTextures_.end())
        throw std::logic_error("object sprite metadata has no generated texture");

    const PixelPoint contact = IsometricProjection::WorldToScreen(
        instance->anchor, lot_.Size(), camera_);
    const Vector2 topLeft{
        static_cast<float>(
            contact.x - selection.reference->anchorX * camera_.zoom),
        static_cast<float>(
            contact.y - selection.reference->anchorY * camera_.zoom)
    };
    spriteBatch_->Draw(
        texture->second, topLeft, std::nullopt, Color::White,
        0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
        SpriteEffects::None, 0.0f);
}

void PeopleGame::DrawResident(const People::Simulation::ResidentId residentId)
{
    const People::Simulation::ResidentState* resident = residents_.Find(residentId);
    if (resident == nullptr)
        throw std::logic_error("render queue refers to a missing resident");
    // Presentation only reads inspectable movement progress; selecting a frame
    // can never advance, complete, or cancel the route it is drawing.
    const std::optional<People::Simulation::ResidentMovementProgress> progress =
        movement_.ProgressFor(residentId);
    const bool walking = progress.has_value() && progress->moving;
    const People::Rendering::ResidentSpriteReference& sprite = walking
        ? ResidentPresentation::SelectWalkSprite(
            demoResidentWalkSprites_, resident->facing, camera_.rotation,
            progress->travelledUnits)
        : ResidentPresentation::SelectIdleSprite(
            demoResidentSprites_, resident->facing, camera_.rotation);
    const auto texture = residentTextures_.find(sprite.assetId);
    if (texture == residentTextures_.end())
        throw std::logic_error("resident sprite metadata has no generated texture");
    if (smokeWalk_)
    {
        std::cout << "People: walk trace frame=" << drawnFrames_
                  << "; moving=" << (walking ? "yes" : "no")
                  << "; travelled=" << (progress.has_value() ? progress->travelledUnits : 0)
                  << "; sprite=" << sprite.assetId << '\n';
    }

    const std::optional<People::World::WorldPoint> worldPosition =
        movement_.PositionFor(residentId);
    if (!worldPosition.has_value())
        throw std::logic_error("resident has no valid movement presentation position");
    const PixelPoint contact = IsometricProjection::WorldPointToScreen(
        *worldPosition, lot_.Size(), camera_);
    const Vector2 topLeft{
        static_cast<float>(contact.x - sprite.footAnchorX * camera_.zoom),
        static_cast<float>(contact.y - sprite.footAnchorY * camera_.zoom)
    };
    spriteBatch_->Draw(
        texture->second, topLeft, std::nullopt, Color::White,
        0.0f, Vector2::Zero, static_cast<float>(camera_.zoom),
        SpriteEffects::None, 0.0f);
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
    const bool hasDoor = lot_.HasDoor(wall);
    const bool doorOpen = hasDoor && lot_.IsDoorOpen(wall);
    const Texture2D& texture = hasDoor
        ? (doorOpen
            ? (slopesDownRight ? doorOpenDownRightTexture_ : doorOpenUpRightTexture_)
            : (slopesDownRight ? doorClosedDownRightTexture_ : doorClosedUpRightTexture_))
        : (slopesDownRight ? wallDownRightTexture_ : wallUpRightTexture_);
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
    if (smokeWalk_ && !smokeWalkStarted_)
    {
        smokeWalkStarted_ = true;
        IssueDemoMove({12, 5, 0});
    }
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
        std::cout << "; selected-object=";
        if (selectedObject_.has_value())
            std::cout << *selectedObject_;
        else
            std::cout << "none";
        std::cout << "\n";
        Exit();
    }
}

GetTypeNameCPP(PeopleGame, "PeopleGame")
