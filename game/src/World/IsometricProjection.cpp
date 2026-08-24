#include "People/World/IsometricProjection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace People::World
{
    namespace
    {
        [[nodiscard]] bool IsInside(const TileCoordinate tile, const LotSize lot)
        {
            return tile.x >= 0 && tile.y >= 0 && tile.x < lot.width && tile.y < lot.height;
        }

        [[nodiscard]] bool IsInside(const ViewCoordinate tile, const LotSize lot)
        {
            return tile.x >= 0 && tile.y >= 0 && tile.x < lot.width && tile.y < lot.height;
        }

        [[nodiscard]] PixelPoint ProjectContinuous(
            const double x, const double y, const double floor = 0.0)
        {
            return {
                (x - y) * IsometricProjection::HalfTileWidth,
                (x + y) * IsometricProjection::HalfTileHeight
                    - floor * IsometricProjection::FloorHeight
            };
        }

        [[nodiscard]] PixelPoint RotateContinuous(
            const double x, const double y, const LotSize lot, const ViewRotation rotation)
        {
            switch (rotation)
            {
                case ViewRotation::North: return {x, y};
                case ViewRotation::East: return {static_cast<double>(lot.height - 1) - y, x};
                case ViewRotation::South:
                    return {static_cast<double>(lot.width - 1) - x,
                            static_cast<double>(lot.height - 1) - y};
                case ViewRotation::West:
                    return {y, static_cast<double>(lot.width - 1) - x};
            }
            throw std::invalid_argument("view rotation must be one of four directions");
        }
    }

    void IsometricProjection::ValidateLot(const LotSize lot)
    {
        if (lot.width <= 0 || lot.height <= 0)
            throw std::invalid_argument("lot dimensions must be positive");
    }

    LotSize IsometricProjection::ViewSize(const LotSize lot, const ViewRotation rotation)
    {
        ValidateLot(lot);
        const int quarterTurns = static_cast<int>(rotation);
        if (quarterTurns < 0 || quarterTurns > 3)
            throw std::invalid_argument("view rotation must be one of four directions");
        return quarterTurns % 2 == 0 ? lot : LotSize{lot.height, lot.width};
    }

    ViewCoordinate IsometricProjection::Rotate(
        const TileCoordinate world, const LotSize lot, const ViewRotation rotation)
    {
        ValidateLot(lot);
        if (!IsInside(world, lot))
            throw std::out_of_range("world tile is outside the lot");

        switch (rotation)
        {
            case ViewRotation::North: return {world.x, world.y, world.floor};
            case ViewRotation::East: return {lot.height - 1 - world.y, world.x, world.floor};
            case ViewRotation::South:
                return {lot.width - 1 - world.x, lot.height - 1 - world.y, world.floor};
            case ViewRotation::West: return {world.y, lot.width - 1 - world.x, world.floor};
        }
        throw std::invalid_argument("view rotation must be one of four directions");
    }

    TileCoordinate IsometricProjection::InverseRotate(
        const ViewCoordinate view, const LotSize lot, const ViewRotation rotation)
    {
        ValidateLot(lot);
        const LotSize viewSize = ViewSize(lot, rotation);
        if (!IsInside(view, viewSize))
            throw std::out_of_range("view tile is outside the rotated lot");

        switch (rotation)
        {
            case ViewRotation::North: return {view.x, view.y, view.floor};
            case ViewRotation::East: return {view.y, lot.height - 1 - view.x, view.floor};
            case ViewRotation::South:
                return {lot.width - 1 - view.x, lot.height - 1 - view.y, view.floor};
            case ViewRotation::West: return {lot.width - 1 - view.y, view.x, view.floor};
        }
        throw std::invalid_argument("view rotation must be one of four directions");
    }

    PixelPoint IsometricProjection::Project(const ViewCoordinate view)
    {
        return {
            static_cast<double>(view.x - view.y) * HalfTileWidth,
            static_cast<double>(view.x + view.y) * HalfTileHeight
                - static_cast<double>(view.floor) * FloorHeight
        };
    }

    PixelPoint IsometricProjection::WorldToScreen(
        const TileCoordinate world, const LotSize lot, const Camera& camera)
    {
        if (!std::isfinite(camera.zoom) || camera.zoom <= 0.0)
            throw std::invalid_argument("camera zoom must be finite and positive");

        const PixelPoint projected = Project(Rotate(world, lot, camera.rotation));
        return {
            camera.origin.x + projected.x * camera.zoom,
            camera.origin.y + projected.y * camera.zoom
        };
    }

    std::optional<TileCoordinate> IsometricProjection::ScreenToWorld(
        const PixelPoint screen, const LotSize lot, const Camera& camera)
    {
        ValidateLot(lot);
        if (!std::isfinite(camera.zoom) || camera.zoom <= 0.0)
            throw std::invalid_argument("camera zoom must be finite and positive");

        const double localX = (screen.x - camera.origin.x) / camera.zoom;
        const double localY = (screen.y - camera.origin.y) / camera.zoom;
        const double viewX = 0.5 * (localX / HalfTileWidth + localY / HalfTileHeight);
        const double viewY = 0.5 * (localY / HalfTileHeight - localX / HalfTileWidth);
        const int baseX = static_cast<int>(std::floor(viewX));
        const int baseY = static_cast<int>(std::floor(viewY));
        const LotSize viewSize = ViewSize(lot, camera.rotation);

        std::optional<ViewCoordinate> best;
        int bestDepth = std::numeric_limits<int>::min();
        constexpr double edgeTolerance = 1.0e-9;

        for (int y = baseY - 1; y <= baseY + 2; ++y)
        {
            for (int x = baseX - 1; x <= baseX + 2; ++x)
            {
                const ViewCoordinate candidate{x, y, 0};
                if (!IsInside(candidate, viewSize))
                    continue;

                const PixelPoint center = Project(candidate);
                const double diamondDistance =
                    std::abs(localX - center.x) / HalfTileWidth
                    + std::abs(localY - center.y) / HalfTileHeight;
                if (diamondDistance > 1.0 + edgeTolerance)
                    continue;

                const int depth = x + y;
                const bool winsTie = !best.has_value()
                    || depth > bestDepth
                    || (depth == bestDepth && y > best->y)
                    || (depth == bestDepth && y == best->y && x > best->x);
                if (winsTie)
                {
                    best = candidate;
                    bestDepth = depth;
                }
            }
        }

        if (!best.has_value())
            return std::nullopt;
        return InverseRotate(*best, lot, camera.rotation);
    }

    Camera IsometricProjection::ZoomAt(
        Camera camera, const double requestedZoom, const PixelPoint screenFocus,
        const double minimumZoom, const double maximumZoom)
    {
        if (!std::isfinite(camera.zoom) || camera.zoom <= 0.0
            || !std::isfinite(requestedZoom)
            || !std::isfinite(minimumZoom) || !std::isfinite(maximumZoom)
            || minimumZoom <= 0.0 || minimumZoom > maximumZoom)
            throw std::invalid_argument("camera zoom range must be finite, positive, and ordered");

        const double zoom = std::clamp(requestedZoom, minimumZoom, maximumZoom);
        const PixelPoint focusInLot{
            (screenFocus.x - camera.origin.x) / camera.zoom,
            (screenFocus.y - camera.origin.y) / camera.zoom
        };
        camera.zoom = zoom;
        camera.origin = {
            screenFocus.x - focusInLot.x * camera.zoom,
            screenFocus.y - focusInLot.y * camera.zoom
        };
        return camera;
    }

    Camera IsometricProjection::RotateAroundLotCenter(
        Camera camera, const LotSize lot, const int clockwiseQuarterTurns)
    {
        ValidateLot(lot);
        const double centerX = static_cast<double>(lot.width - 1) * 0.5;
        const double centerY = static_cast<double>(lot.height - 1) * 0.5;
        const PixelPoint oldView = RotateContinuous(centerX, centerY, lot, camera.rotation);
        const PixelPoint oldProjected = ProjectContinuous(oldView.x, oldView.y);
        const PixelPoint screenFocus{
            camera.origin.x + oldProjected.x * camera.zoom,
            camera.origin.y + oldProjected.y * camera.zoom
        };

        camera.rotation = RotateBy(camera.rotation, clockwiseQuarterTurns);
        const PixelPoint newView = RotateContinuous(centerX, centerY, lot, camera.rotation);
        const PixelPoint newProjected = ProjectContinuous(newView.x, newView.y);
        camera.origin = {
            screenFocus.x - newProjected.x * camera.zoom,
            screenFocus.y - newProjected.y * camera.zoom
        };
        return camera;
    }

    ViewRotation IsometricProjection::RotateBy(
        const ViewRotation rotation, const int clockwiseQuarterTurns)
    {
        const int current = static_cast<int>(rotation);
        if (current < 0 || current > 3)
            throw std::invalid_argument("view rotation must be one of four directions");
        const int wrapped = ((current + clockwiseQuarterTurns) % 4 + 4) % 4;
        return static_cast<ViewRotation>(wrapped);
    }
}
