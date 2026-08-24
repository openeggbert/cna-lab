#include "People/World/IsometricProjection.hpp"

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

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

    void CheckNear(const double actual, const double expected, const std::string& message)
    {
        Check(std::abs(actual - expected) <= 1.0e-9,
              message + " (actual=" + std::to_string(actual)
                  + ", expected=" + std::to_string(expected) + ')');
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

    void TestRotationsRoundTrip()
    {
        const std::vector<LotSize> lots{{1, 1}, {3, 5}, {20, 20}};
        for (const LotSize lot : lots)
        {
            for (int rotationIndex = 0; rotationIndex < 4; ++rotationIndex)
            {
                const auto rotation = static_cast<ViewRotation>(rotationIndex);
                const LotSize viewSize = IsometricProjection::ViewSize(lot, rotation);
                const LotSize expected = rotationIndex % 2 == 0
                    ? lot : LotSize{lot.height, lot.width};
                Check(viewSize == expected, "view dimensions rotate with rectangular lot");

                for (int y = 0; y < lot.height; ++y)
                {
                    for (int x = 0; x < lot.width; ++x)
                    {
                        const TileCoordinate world{x, y, 2};
                        const ViewCoordinate view = IsometricProjection::Rotate(world, lot, rotation);
                        Check(view.x >= 0 && view.y >= 0
                                  && view.x < viewSize.width && view.y < viewSize.height,
                              "rotated coordinate remains in bounded view");
                        Check(IsometricProjection::InverseRotate(view, lot, rotation) == world,
                              "rotation inverse restores world coordinate");
                    }
                }
            }
        }
    }

    void TestKnownProjection()
    {
        const PixelPoint origin = IsometricProjection::Project({0, 0, 0});
        CheckNear(origin.x, 0.0, "origin projects to zero x");
        CheckNear(origin.y, 0.0, "origin projects to zero y");

        const PixelPoint east = IsometricProjection::Project({1, 0, 0});
        CheckNear(east.x, 48.0, "view x advances half tile width");
        CheckNear(east.y, 24.0, "view x advances half tile height");

        const PixelPoint south = IsometricProjection::Project({0, 1, 0});
        CheckNear(south.x, -48.0, "view y retreats half tile width");
        CheckNear(south.y, 24.0, "view y advances half tile height");

        const PixelPoint upper = IsometricProjection::Project({3, 2, 1});
        CheckNear(upper.x, 48.0, "floor does not alter projected x");
        CheckNear(upper.y, 24.0, "floor subtracts explicit elevation");
    }

    void TestScreenRoundTrips()
    {
        const LotSize lot{7, 5};
        for (int rotationIndex = 0; rotationIndex < 4; ++rotationIndex)
        {
            Camera camera{{321.25, 97.5}, 1.375, static_cast<ViewRotation>(rotationIndex)};
            for (int y = 0; y < lot.height; ++y)
            {
                for (int x = 0; x < lot.width; ++x)
                {
                    const TileCoordinate tile{x, y, 0};
                    const PixelPoint screen = IsometricProjection::WorldToScreen(tile, lot, camera);
                    const auto picked = IsometricProjection::ScreenToWorld(screen, lot, camera);
                    Check(picked.has_value() && *picked == tile,
                          "tile center picks the same world tile under every rotation");
                }
            }
        }
    }

    void TestInsideAndOutsidePicking()
    {
        const LotSize lot{2, 2};
        const Camera camera{{100.0, 80.0}, 1.0, ViewRotation::North};
        const auto inside = IsometricProjection::ScreenToWorld({112.0, 84.0}, lot, camera);
        Check(inside.has_value() && *inside == TileCoordinate{0, 0, 0},
              "point inside diamond selects tile");

        const auto sharedEdge = IsometricProjection::ScreenToWorld({124.0, 92.0}, lot, camera);
        Check(sharedEdge.has_value() && *sharedEdge == TileCoordinate{1, 0, 0},
              "shared edge chooses greater view depth");

        Check(!IsometricProjection::ScreenToWorld({-10000.0, -10000.0}, lot, camera).has_value(),
              "point far outside lot is rejected");
    }

    void TestSharedEdgePickingUnderAllRotations()
    {
        const LotSize lot{4, 3};
        for (int rotationIndex = 0; rotationIndex < 4; ++rotationIndex)
        {
            const auto rotation = static_cast<ViewRotation>(rotationIndex);
            const Camera camera{{213.0, 87.0}, 1.25, rotation};
            const LotSize viewSize = IsometricProjection::ViewSize(lot, rotation);
            const ViewCoordinate shallower{0, 0, 0};
            const ViewCoordinate deeper{1, 0, 0};
            Check(viewSize.width >= 2, "edge test view has two horizontal cells");

            const PixelPoint left = IsometricProjection::Project(shallower);
            const PixelPoint right = IsometricProjection::Project(deeper);
            const PixelPoint shared{
                camera.origin.x + (left.x + right.x) * 0.5 * camera.zoom,
                camera.origin.y + (left.y + right.y) * 0.5 * camera.zoom
            };
            const auto picked = IsometricProjection::ScreenToWorld(shared, lot, camera);
            const TileCoordinate expected = IsometricProjection::InverseRotate(deeper, lot, rotation);
            Check(picked.has_value() && *picked == expected,
                  "shared edge chooses greater view depth under every rotation");
        }
    }

    void TestCameraTranslationAndZoom()
    {
        const LotSize lot{20, 20};
        const Camera camera{{640.0, 72.0}, 0.5, ViewRotation::North};
        const PixelPoint screen = IsometricProjection::WorldToScreen({2, 1, 0}, lot, camera);
        CheckNear(screen.x, 664.0, "camera applies zoom and x origin");
        CheckNear(screen.y, 108.0, "camera applies zoom and y origin");

        const PixelPoint focus{719.0, 411.0};
        const Camera zoomed = IsometricProjection::ZoomAt(camera, 1.8, focus, 0.35, 2.0);
        CheckNear((focus.x - zoomed.origin.x) / zoomed.zoom,
                  (focus.x - camera.origin.x) / camera.zoom,
                  "cursor zoom preserves lot-local x below focus");
        CheckNear((focus.y - zoomed.origin.y) / zoomed.zoom,
                  (focus.y - camera.origin.y) / camera.zoom,
                  "cursor zoom preserves lot-local y below focus");
        CheckNear(IsometricProjection::ZoomAt(camera, 99.0, focus, 0.35, 2.0).zoom,
                  2.0, "zoom clamps to upper limit");

        const LotSize rectangular{8, 5};
        const Camera before{{401.0, 73.0}, 0.9, ViewRotation::North};
        const Camera after = IsometricProjection::RotateAroundLotCenter(before, rectangular, 1);
        Check(after.rotation == ViewRotation::East, "camera rotates one discrete direction");

        const auto projectedCentroid = [rectangular](const Camera& value) {
            const TileCoordinate corners[] = {
                {0, 0, 0}, {rectangular.width - 1, 0, 0},
                {0, rectangular.height - 1, 0},
                {rectangular.width - 1, rectangular.height - 1, 0}
            };
            PixelPoint total{};
            for (const TileCoordinate corner : corners)
            {
                const PixelPoint point = IsometricProjection::WorldToScreen(
                    corner, rectangular, value);
                total.x += point.x;
                total.y += point.y;
            }
            return PixelPoint{total.x / 4.0, total.y / 4.0};
        };
        const PixelPoint oldCenter = projectedCentroid(before);
        const PixelPoint newCenter = projectedCentroid(after);
        CheckNear(newCenter.x, oldCenter.x, "rotation preserves rectangular lot center x");
        CheckNear(newCenter.y, oldCenter.y, "rotation preserves rectangular lot center y");
    }

    void TestValidationAndRotationWrapping()
    {
        CheckThrows([] { (void)IsometricProjection::ViewSize({0, 2}, ViewRotation::North); },
                    "zero-sized lot is rejected");
        CheckThrows([] { (void)IsometricProjection::Rotate({2, 0, 0}, {2, 2}, ViewRotation::North); },
                    "out-of-bounds world tile is rejected");
        CheckThrows([] {
            const Camera invalid{{0.0, 0.0}, 0.0, ViewRotation::North};
            (void)IsometricProjection::WorldToScreen({0, 0, 0}, {1, 1}, invalid);
        }, "non-positive zoom is rejected");
        CheckThrows([] {
            const Camera camera{{0.0, 0.0}, 1.0, ViewRotation::North};
            (void)IsometricProjection::ZoomAt(camera, 2.0, {0.0, 0.0}, 3.0, 1.0);
        }, "reversed zoom range is rejected");

        Check(IsometricProjection::RotateBy(ViewRotation::North, -1) == ViewRotation::West,
              "negative rotation wraps west");
        Check(IsometricProjection::RotateBy(ViewRotation::West, 1) == ViewRotation::North,
              "positive rotation wraps north");
        Check(IsometricProjection::RotateBy(ViewRotation::East, 9) == ViewRotation::South,
              "multiple turns wrap to four rotations");
    }
}

int main()
{
    TestRotationsRoundTrip();
    TestKnownProjection();
    TestScreenRoundTrips();
    TestInsideAndOutsidePicking();
    TestSharedEdgePickingUnderAllRotations();
    TestCameraTranslationAndZoom();
    TestValidationAndRotationWrapping();

    if (failures != 0)
    {
        std::cerr << failures << " People core test(s) failed\n";
        return 1;
    }
    std::cout << "All People isometric projection tests passed\n";
    return 0;
}
