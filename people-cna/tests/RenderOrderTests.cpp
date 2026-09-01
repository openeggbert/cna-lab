#include "People/Rendering/RenderOrder.hpp"
#include "People/Rendering/WallPresentation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace People::Rendering;
using namespace People::World;

namespace
{
    int failures = 0;

    void Check(const bool condition, const std::string& message)
    {
        if (condition)
            return;
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }

    void CheckThrows(const std::function<void()>& operation, const std::string& message)
    {
        try
        {
            operation();
            Check(false, message);
        }
        catch (const std::exception&)
        {
        }
    }

    void TestLexicographicFields()
    {
        const RenderKey base{0, 4, DrawLayer::WorldEntity, 2, 3, 0, 10};
        Check(RenderKey{-1, 99, DrawLayer::Overlay, 99, 99, 99, 99} < base,
              "floor is the primary field");
        Check(RenderKey{0, 3, DrawLayer::Overlay, 99, 99, 99, 99} < base,
              "footprint depth precedes draw layer");
        Check(RenderKey{0, 4, DrawLayer::WallBack, 99, 99, 99, 99} < base,
              "draw layer precedes anchor");
        Check(RenderKey{0, 4, DrawLayer::WorldEntity, 1, 99, 99, 99} < base,
              "anchor y precedes anchor x");
        Check(RenderKey{0, 4, DrawLayer::WorldEntity, 2, 2, 99, 99} < base,
              "anchor x precedes local offset");
        Check(RenderKey{0, 4, DrawLayer::WorldEntity, 2, 3, -1, 99} < base,
              "local offset precedes stable ID");
        Check(RenderKey{0, 4, DrawLayer::WorldEntity, 2, 3, 0, 9} < base,
              "stable ID is the final tie breaker");
    }

    void TestFootprintDepthAndAnchorForAllRotations()
    {
        const LotSize lot{6, 4};
        const std::array<TileCoordinate, 3> footprint{{
            {1, 1, 0}, {2, 1, 0}, {2, 2, 0}
        }};
        const TileCoordinate anchor{1, 1, 0};

        for (int index = 0; index < 4; ++index)
        {
            const auto rotation = static_cast<ViewRotation>(index);
            int expectedDepth = 0;
            bool first = true;
            for (const TileCoordinate tile : footprint)
            {
                const ViewCoordinate view = IsometricProjection::Rotate(tile, lot, rotation);
                if (first || view.x + view.y > expectedDepth)
                    expectedDepth = view.x + view.y;
                first = false;
            }
            const ViewCoordinate viewAnchor = IsometricProjection::Rotate(anchor, lot, rotation);
            const RenderKey key = RenderOrder::BuildKey(
                footprint, anchor, lot, rotation, DrawLayer::WorldEntity, 7, 42);
            Check(key.floor == 0, "key preserves logical floor");
            Check(key.footprintDepth == expectedDepth,
                  "key uses farthest footprint depth in every rotation");
            Check(key.anchorY == viewAnchor.y && key.anchorX == viewAnchor.x,
                  "key stores rotated authored anchor");
            Check(key.localOffset == 7 && key.stableId == 42,
                  "key preserves segment offset and stable ID");
        }
    }

    void TestInsertionOrderIndependence()
    {
        const LotSize lot{7, 5};
        const std::array<TileCoordinate, 6> tiles{{
            {0, 0, 0}, {4, 2, 0}, {1, 3, 0},
            {6, 4, 0}, {3, 1, 0}, {2, 2, 0}
        }};
        std::vector<RenderKey> canonical;
        for (std::size_t index = 0; index < tiles.size(); ++index)
        {
            canonical.push_back(RenderOrder::BuildKey(
                std::span<const TileCoordinate>(&tiles[index], 1), tiles[index], lot,
                ViewRotation::West, DrawLayer::Terrain, 0,
                static_cast<std::uint64_t>(index + 1)));
        }
        std::ranges::sort(canonical);

        for (unsigned seed = 0; seed < 20; ++seed)
        {
            std::vector<RenderKey> shuffled = canonical;
            std::mt19937 engine(seed);
            std::ranges::shuffle(shuffled, engine);
            std::ranges::sort(shuffled);
            Check(shuffled == canonical, "sorting result does not depend on insertion order");
        }
    }

    void TestValidation()
    {
        const LotSize lot{3, 3};
        const TileCoordinate anchor{1, 1, 0};
        CheckThrows([&] {
            (void)RenderOrder::BuildKey({}, anchor, lot, ViewRotation::North,
                                        DrawLayer::Terrain, 0, 1);
        }, "empty footprint is rejected");

        const std::array<TileCoordinate, 2> mixedFloors{{{1, 1, 0}, {1, 2, 1}}};
        CheckThrows([&] {
            (void)RenderOrder::BuildKey(mixedFloors, anchor, lot, ViewRotation::North,
                                        DrawLayer::WorldEntity, 0, 1);
        }, "mixed-floor footprint is rejected");

        const std::array<TileCoordinate, 1> outside{{{3, 0, 0}}};
        CheckThrows([&] {
            (void)RenderOrder::BuildKey(outside, anchor, lot, ViewRotation::North,
                                        DrawLayer::Terrain, 0, 1);
        }, "out-of-bounds footprint is rejected");
    }

    void TestWallPresentationInAllRotations()
    {
        LotGrid lot(5, 3);
        for (int x = 0; x < lot.Size().width; ++x)
        {
            (void)lot.AddWall({x, 0, 0}, TileEdge::MinY);
            (void)lot.AddWall({x, lot.Size().height - 1, 0}, TileEdge::MaxY);
        }
        for (int y = 0; y < lot.Size().height; ++y)
        {
            (void)lot.AddWall({0, y, 0}, TileEdge::MinX);
            (void)lot.AddWall({lot.Size().width - 1, y, 0}, TileEdge::MaxX);
        }

        for (int rotationIndex = 0; rotationIndex < 4; ++rotationIndex)
        {
            const auto rotation = static_cast<ViewRotation>(rotationIndex);
            int backCount = 0;
            int frontCount = 0;
            for (const WallEdge wall : lot.Walls())
            {
                const WallRenderDescriptor descriptor = WallPresentation::Describe(
                    lot, wall, rotation);
                Check(descriptor.footprint.size() == 1,
                      "perimeter wall has one in-lot footprint tile");
                if (descriptor.layer == DrawLayer::WallBack) ++backCount;
                if (descriptor.layer == DrawLayer::WallFront) ++frontCount;

                const Camera camera{{0.0, 0.0}, 1.0, rotation};
                const PixelPoint first = IsometricProjection::WorldPointToScreen(
                    descriptor.endpoints[0], lot.Size(), camera);
                const PixelPoint second = IsometricProjection::WorldPointToScreen(
                    descriptor.endpoints[1], lot.Size(), camera);
                Check(std::abs(std::abs(second.x - first.x)
                               - IsometricProjection::HalfTileWidth) <= 1.0e-9,
                      "wall segment keeps projected half-tile width");
                Check(std::abs(std::abs(second.y - first.y)
                               - IsometricProjection::HalfTileHeight) <= 1.0e-9,
                      "wall segment keeps projected half-tile height");
            }
            Check(backCount == lot.Size().width + lot.Size().height,
                  "two camera-away perimeter sides use back layer");
            Check(frontCount == lot.Size().width + lot.Size().height,
                  "two camera-facing perimeter sides use front layer");
        }

        (void)lot.AddWall({1, 1, 0}, TileEdge::MaxX);
        const WallRenderDescriptor interior = WallPresentation::Describe(
            lot, lot.CanonicalWall({1, 1, 0}, TileEdge::MaxX), ViewRotation::North);
        Check(interior.footprint.size() == 2, "interior wall spans two adjacent tiles");
        Check(interior.layer == DrawLayer::WallFront,
              "initial interior-wall policy uses front layer pending segmentation");
    }
}

int main()
{
    TestLexicographicFields();
    TestFootprintDepthAndAnchorForAllRotations();
    TestInsertionOrderIndependence();
    TestValidation();
    TestWallPresentationInAllRotations();

    if (failures != 0)
    {
        std::cerr << failures << " People render-order test(s) failed\n";
        return 1;
    }
    std::cout << "All People render-order tests passed\n";
    return 0;
}
