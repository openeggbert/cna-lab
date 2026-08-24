#pragma once

#include <optional>

namespace People::World
{
    /** @brief The four clockwise presentation rotations. */
    enum class ViewRotation
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    /** @brief A logical simulation coordinate. */
    struct TileCoordinate
    {
        int x = 0;
        int y = 0;
        int floor = 0;

        bool operator==(const TileCoordinate&) const = default;
    };

    /** @brief A tile coordinate after applying the presentation rotation. */
    struct ViewCoordinate
    {
        int x = 0;
        int y = 0;
        int floor = 0;

        bool operator==(const ViewCoordinate&) const = default;
    };

    /** @brief Dimensions of a bounded lot in logical tiles. */
    struct LotSize
    {
        int width = 0;
        int height = 0;

        bool operator==(const LotSize&) const = default;
    };

    /** @brief A point measured in projected or screen pixels. */
    struct PixelPoint
    {
        double x = 0.0;
        double y = 0.0;

        bool operator==(const PixelPoint&) const = default;
    };

    /** @brief Continuous simulation point, used for edges and sub-tile movement. */
    struct WorldPoint
    {
        double x = 0.0;
        double y = 0.0;
        int floor = 0;
    };

    /** @brief Presentation-only camera state. */
    struct Camera
    {
        PixelPoint origin;
        double zoom = 1.0;
        ViewRotation rotation = ViewRotation::North;
    };

    /**
     * @brief Pure coordinate transforms for People's 96 x 48 isometric lot.
     *
     * All methods are renderer independent. Rotations are bounded by the lot;
     * the simulation coordinate never rotates.
     */
    class IsometricProjection final
    {
    public:
        static constexpr int TileWidth = 96;
        static constexpr int TileHeight = 48;
        static constexpr int HalfTileWidth = TileWidth / 2;
        static constexpr int HalfTileHeight = TileHeight / 2;
        static constexpr int FloorHeight = 96;

        /** @brief Returns lot dimensions after a presentation rotation. */
        [[nodiscard]] static LotSize ViewSize(LotSize lot, ViewRotation rotation);

        /** @brief Rotates a valid simulation tile into bounded view space. */
        [[nodiscard]] static ViewCoordinate Rotate(
            TileCoordinate world, LotSize lot, ViewRotation rotation);

        /** @brief Restores a valid bounded view tile to simulation space. */
        [[nodiscard]] static TileCoordinate InverseRotate(
            ViewCoordinate view, LotSize lot, ViewRotation rotation);

        /** @brief Projects the center of a view-space tile into lot-local pixels. */
        [[nodiscard]] static PixelPoint Project(ViewCoordinate view);

        /** @brief Projects a world tile center through rotation, zoom, and camera origin. */
        [[nodiscard]] static PixelPoint WorldToScreen(
            TileCoordinate world, LotSize lot, const Camera& camera);

        /** @brief Projects a bounded continuous world point through presentation state. */
        [[nodiscard]] static PixelPoint WorldPointToScreen(
            WorldPoint world, LotSize lot, const Camera& camera);

        /**
         * @brief Picks a floor-zero world tile from a screen pixel.
         *
         * Shared edges deterministically select the view-space tile with the
         * greatest depth, then greatest y and x.
         */
        [[nodiscard]] static std::optional<TileCoordinate> ScreenToWorld(
            PixelPoint screen, LotSize lot, const Camera& camera);

        /** @brief Clamps a zoom while preserving the lot-space point below a screen pixel. */
        [[nodiscard]] static Camera ZoomAt(
            Camera camera, double requestedZoom, PixelPoint screenFocus,
            double minimumZoom, double maximumZoom);

        /** @brief Rotates presentation around the projected center of a bounded lot. */
        [[nodiscard]] static Camera RotateAroundLotCenter(
            Camera camera, LotSize lot, int clockwiseQuarterTurns);

        /** @brief Returns the next rotation, wrapping to one of four values. */
        [[nodiscard]] static ViewRotation RotateBy(
            ViewRotation rotation, int clockwiseQuarterTurns);

    private:
        static void ValidateLot(LotSize lot);
    };
}
