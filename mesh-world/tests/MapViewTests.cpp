// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M187-189/M200 (MAP12) — MapView viewport state: zoom in/out, pan, and the
// visible-tile set for a viewport, across several level/viewport combos.

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

#include "MapView.hpp"
#include "Map/PlanetConstants.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

TEST(MapViewTest, SetViewJumpsDirectlyToTheGivenTile) {
    MapView view;
    view.set_view(TileCoord{9, 12, 34});
    EXPECT_EQ(view.level(), 9);
    EXPECT_EQ(view.center(), (TileCoord{9, 12, 34}));
}

TEST(MapViewTest, SetViewThenPanStaysWithinThatLevelsBounds) {
    MapView view;
    view.set_view(TileCoord{3, 4, 4});  // level 3: valid range [0, 8)
    view.pan(100, -100);
    const TileCoord c = view.center();
    EXPECT_EQ(c.level, 3);
    EXPECT_EQ(c.x, 7);
    EXPECT_EQ(c.y, 0);
}

TEST(MapViewTest, StartsAtLevel0OverThePlanetRoot) {
    MapView view;
    EXPECT_EQ(view.level(), 0);
    EXPECT_EQ(view.center(), (TileCoord{0, 0, 0}));
}

TEST(MapViewTest, ZoomInIncreasesLevelAndDescendsToAChild) {
    MapView view;
    view.zoom_in();
    EXPECT_EQ(view.level(), 1);
    EXPECT_EQ(view.center(), (TileCoord{1, 0, 0}));

    view.zoom_in();
    EXPECT_EQ(view.level(), 2);
    EXPECT_EQ(view.center().level, 2);
}

TEST(MapViewTest, ZoomOutAtLevel0IsANoOp) {
    MapView view;
    view.zoom_out();
    EXPECT_EQ(view.level(), 0);
    EXPECT_EQ(view.center(), (TileCoord{0, 0, 0}));
}

TEST(MapViewTest, ZoomInThenZoomOutReturnsToTheParentTile) {
    MapView view;
    view.zoom_in();
    view.pan(0, 0);  // no-op, just confirming pan doesn't interfere
    const TileCoord before_out = view.center();
    view.zoom_out();
    EXPECT_EQ(view.level(), 0);
    EXPECT_EQ(view.center(), before_out.parent());
}

TEST(MapViewTest, ZoomInClampsAtMaxLevel) {
    MapView view;
    for (int i = 0; i < MAX_LEVEL + 5; ++i) view.zoom_in();
    EXPECT_EQ(view.level(), MAX_LEVEL);
}

TEST(MapViewTest, PanAtLevel0ClampsToTheSingleValidTile) {
    MapView view;  // level 0: only (0,0) is valid
    view.pan(5, -3);
    EXPECT_EQ(view.center(), (TileCoord{0, 0, 0}));
}

TEST(MapViewTest, PanMovesCenterWithinBoundsAtADeeperLevel) {
    MapView view;
    view.zoom_in();
    view.zoom_in();
    view.zoom_in();  // level 3: valid range [0, 8)
    view.pan(2, -1);
    const TileCoord c = view.center();
    EXPECT_EQ(c.level, 3);
    EXPECT_GE(c.x, 0);
    EXPECT_LT(c.x, 8);
    EXPECT_GE(c.y, 0);
    EXPECT_LT(c.y, 8);
}

TEST(MapViewTest, PanClampsAtThePlanetEdgeWithoutWrapping) {
    MapView view;
    view.zoom_in();
    view.zoom_in();  // level 2: valid range [0, 4)
    view.pan(-100, 100);
    const TileCoord c = view.center();
    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 3);
}

TEST(MapViewTest, VisibleTilesAtLevel0IsJustTheSingleRootTile) {
    MapView view;
    const auto tiles = view.visible_tiles(5, 5);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles[0], (TileCoord{0, 0, 0}));
}

TEST(MapViewTest, VisibleTilesCoversTheFullViewportAwayFromAnyEdge) {
    MapView view;
    for (int i = 0; i < 5; ++i) view.zoom_in();  // level 5: range [0, 32), plenty of room
    view.pan(16, 16);                            // dead center, far from every edge

    const auto tiles = view.visible_tiles(3, 3);
    EXPECT_EQ(tiles.size(), 9u);

    std::set<std::pair<std::int64_t, std::int64_t>> coords;
    for (const auto& t : tiles) {
        EXPECT_EQ(t.level, 5);
        coords.insert({t.x, t.y});
    }
    EXPECT_EQ(coords.size(), 9u) << "expected 9 distinct tile coordinates, no duplicates";
}

TEST(MapViewTest, VisibleTilesClampsNearThePlanetEdgeRatherThanWrapping) {
    MapView view;
    view.zoom_in();
    view.zoom_in();  // level 2: range [0, 4)
    view.pan(-100, -100);  // clamps to (0, 0), the corner

    const auto tiles = view.visible_tiles(5, 5);
    for (const auto& t : tiles) {
        EXPECT_GE(t.x, 0);
        EXPECT_LT(t.x, 4);
        EXPECT_GE(t.y, 0);
        EXPECT_LT(t.y, 4);
    }
    // Centered on the corner (0,0) with a 5x5 viewport (2 tiles of margin
    // each side): only the quadrant x in [0,2], y in [0,2] is in range -> 9.
    EXPECT_EQ(tiles.size(), 9u);
}

TEST(MapViewTest, VisibleTilesReturnsEmptyForNonPositiveDimensions) {
    MapView view;
    EXPECT_TRUE(view.visible_tiles(0, 5).empty());
    EXPECT_TRUE(view.visible_tiles(5, 0).empty());
    EXPECT_TRUE(view.visible_tiles(-1, -1).empty());
}

// M195/M202 — label visibility by zoom level: no village-level clutter at
// continent zoom, matching map.md's "no street names at continent zoom"
// principle applied to the PlaceLabel::kind values Settlements::
// appendLabels() actually produces today (capital/city/town/village).
TEST(MapViewTest, VillageLabelsAreHiddenAtShallowZoomButShownWhenZoomedIn) {
    EXPECT_FALSE(MapView::should_show_label("village", 0));
    EXPECT_FALSE(MapView::should_show_label("village", 9));
    EXPECT_TRUE(MapView::should_show_label("village", 12));
    EXPECT_TRUE(MapView::should_show_label("village", 15));
}

TEST(MapViewTest, CityLabelsShowEarlierThanTownOrVillageLabels) {
    EXPECT_FALSE(MapView::should_show_label("city", 3));
    EXPECT_TRUE(MapView::should_show_label("city", 7));

    EXPECT_FALSE(MapView::should_show_label("town", 7));
    EXPECT_TRUE(MapView::should_show_label("town", 9));
}

TEST(MapViewTest, ContinentLabelsAreAlwaysVisible) {
    EXPECT_TRUE(MapView::should_show_label("continent", 0));
    EXPECT_TRUE(MapView::should_show_label("continent", 18));
}

TEST(MapViewTest, VisibilityIsMonotonicWithZoomForEveryKnownKind) {
    for (const std::string& kind : {std::string("continent"), std::string("country"),
                                     std::string("capital"), std::string("city"),
                                     std::string("town"), std::string("village")}) {
        bool seen_visible = false;
        for (int level = 0; level <= MAX_LEVEL; ++level) {
            const bool visible = MapView::should_show_label(kind, level);
            if (seen_visible) {
                EXPECT_TRUE(visible) << kind << " became hidden again at level " << level
                                     << " after being shown at a shallower zoom";
            }
            seen_visible = seen_visible || visible;
        }
        EXPECT_TRUE(seen_visible) << kind << " is never visible at any level";
    }
}

// M199 — scale_denominator() with tiles_across=1 must reproduce map.md
// §5.4's own worked examples exactly (it derives the same formula from the
// same "~0.2 m-wide map viewport" reference).
TEST(MapViewTest, ScaleDenominatorMatchesMapMdWorkedExamplesAtOneTileAcross) {
    EXPECT_NEAR(MapView::scale_denominator(0, 1), 112925000.0, 1.0);
    EXPECT_NEAR(MapView::scale_denominator(3, 1), 14115625.0, 1.0);
    EXPECT_NEAR(MapView::scale_denominator(5, 1), 3528906.25, 1.0);
}

TEST(MapViewTest, ScaleDenominatorScalesLinearlyWithTilesAcross) {
    const double one_tile     = MapView::scale_denominator(5, 1);
    const double fifteen_tile = MapView::scale_denominator(5, 15);
    EXPECT_NEAR(fifteen_tile, one_tile * 15.0, 1e-6);
}

TEST(MapViewTest, ScaleDenominatorDecreasesWhenZoomingIn) {
    // Deeper level -> smaller tiles -> smaller N (a "closer" 1:N scale),
    // holding tiles_across fixed.
    EXPECT_GT(MapView::scale_denominator(3, 15), MapView::scale_denominator(12, 15));
}

TEST(MapViewTest, ScaleDenominatorIsZeroForDegenerateInputs) {
    EXPECT_EQ(MapView::scale_denominator(5, 0), 0.0);
    EXPECT_EQ(MapView::scale_denominator(5, -1), 0.0);
    EXPECT_EQ(MapView::scale_denominator(5, 1, 0.0), 0.0);
    EXPECT_EQ(MapView::scale_denominator(5, 1, -0.2), 0.0);
}
