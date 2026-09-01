// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M211 — automated test for the MeshWorldPlanet CLI driver (MAP13). Calls the
// logic in PlanetMapLogic.hpp directly (no subprocess): plan.md's literal ask
// is "generates a full level-0 map without crashing"; this also covers the
// hand-off demo producing MC3Validator-valid XML.

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "BuiltinMaterials.hpp"
#include "MC3Validator.hpp"
#include "Map/MapPipeline.hpp"
#include "Map/MapTileStore.hpp"
#include "ModelPlacementStore.hpp"
#include "PlanetMapLogic.hpp"
#include "PlanetWorld.hpp"
#include "RegionShard.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

std::string tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_planetmap_test_" + suffix)).string();
}

} // namespace

class PlanetMapLogicTest : public ::testing::Test {
protected:
    void SetUp() override { MeshWorld::register_builtin_materials(); }
};

// --- parse_planet_map_args ---------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsToLevel0RootWithNoFlags) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.dir, "/some/dir");
    EXPECT_EQ(args.tile, (TileCoord{0, 0, 0}));
}

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsLevelAndTile) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--level", "3", "--tile", "2,5"};
    const auto [args, ok] = parse_planet_map_args(6, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.tile, (TileCoord{3, 2, 5}));
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsMissingDir) {
    const char* argv[] = {"MeshWorldPlanet"};
    const auto [args, ok] = parse_planet_map_args(1, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsMalformedTile) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--tile", "3"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsOutOfRangeLevel) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--level", "99", "--tile", "0,0"};
    const auto [args, ok] = parse_planet_map_args(6, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsTileOutOfRangeForLevel) {
    // Level 1 has a 2x2 grid (valid indices 0..1); (5,5) is out of range.
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--level", "1", "--tile", "5,5"};
    const auto [args, ok] = parse_planet_map_args(6, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsUnknownFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--bogus"};
    const auto [args, ok] = parse_planet_map_args(3, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsStatsFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--level", "3", "--tile", "2,5", "--stats"};
    const auto [args, ok] = parse_planet_map_args(7, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.stats);
    EXPECT_EQ(args.tile, (TileCoord{3, 2, 5}));
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsStatsToFalse) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_FALSE(args.stats);
}

// --- M207: feature_histogram ---------------------------------------------

TEST_F(PlanetMapLogicTest, FeatureHistogramCountsEachTypeSeparately) {
    std::vector<MapFeature> features;
    features.push_back({FeatureType::River, "R1", {}, {}});
    features.push_back({FeatureType::River, "R2", {}, {}});
    features.push_back({FeatureType::City, "C1", {}, {}});
    features.push_back({FeatureType::Continent, "Cont1", {}, {}});

    const auto hist = feature_histogram(features);
    EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::River)], 2);
    EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::City)], 1);
    EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::Continent)], 1);
    EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::Town)], 0);
}

TEST_F(PlanetMapLogicTest, FeatureHistogramEmptyForNoFeatures) {
    const auto hist = feature_histogram({});
    for (int c : hist) EXPECT_EQ(c, 0);
}

// --- M206: --ascii -----------------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsAsciiFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--ascii"};
    const auto [args, ok] = parse_planet_map_args(3, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.ascii);
}

TEST_F(PlanetMapLogicTest, ZoneAsciiCharMatchesPrintMapLetterScheme) {
    EXPECT_EQ(zone_ascii_char(0), 'C');   // city
    EXPECT_EQ(zone_ascii_char(4), 'O');   // ocean
    // M235 (MAP16, 2026-07-10): the enum grew 12->52 and empty moved from
    // ordinal 11 to 51 (it must stay last, see ZoneType.hpp's own comment);
    // 11 is now savanna, the first of the 40 new values.
    EXPECT_EQ(zone_ascii_char(11), 'A');  // savanna
    EXPECT_EQ(zone_ascii_char(51), '.');  // empty
    EXPECT_EQ(zone_ascii_char(99), '?');  // out of range
}

// --- M209: --names -------------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsNamesFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--names"};
    const auto [args, ok] = parse_planet_map_args(3, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.names);
}

TEST_F(PlanetMapLogicTest, CollectNamesGathersNamedFeaturesAndLabelsSkippingUnnamed) {
    MapTilePayload payload;
    payload.features.push_back({FeatureType::Continent, "Eurasia", {}, {}});
    payload.features.push_back({FeatureType::River, "", {}, {}});  // unnamed, skipped
    payload.features.push_back({FeatureType::Border, "Vecvicvia", {}, {}});
    payload.labels.push_back({"Springfield", {0.0, 0.0}, "capital"});
    payload.labels.push_back({"", {1.0, 1.0}, "city"});  // unnamed, skipped

    const auto names = collect_names(payload);
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0].kind, "continent");
    EXPECT_EQ(names[0].name, "Eurasia");
    EXPECT_EQ(names[1].kind, "border");
    EXPECT_EQ(names[1].name, "Vecvicvia");
    EXPECT_EQ(names[2].kind, "capital");
    EXPECT_EQ(names[2].name, "Springfield");
}

TEST_F(PlanetMapLogicTest, CollectNamesEmptyForPayloadWithNoNames) {
    const MapTilePayload payload;
    EXPECT_TRUE(collect_names(payload).empty());
}

TEST_F(PlanetMapLogicTest, RenderAsciiBiomeMapHasOneLinePerRowAndCorrectWidth) {
    Map::BiomeGrid biome;
    biome.w = 3;
    biome.h = 2;
    // M235 (MAP16, 2026-07-10): empty is ordinal 51 now (grown 12->52,
    // empty must stay last) -- see ZoneAsciiCharMatchesPrintMapLetterScheme's
    // own comment above.
    biome.data = {0, 4, 51, 4, 4, 0};  // city ocean empty / ocean ocean city

    const std::string rendered = render_ascii_biome_map(biome);
    std::istringstream iss(rendered);
    std::string        line1, line2;
    std::getline(iss, line1);
    std::getline(iss, line2);

    EXPECT_EQ(line1, "C O .");
    EXPECT_EQ(line2, "O O C");
}

// --- M205/M213: --png ---------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsPngFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png", "out.png"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.png_path, "out.png");
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsPngPathToEmpty) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.png_path.empty());
}

TEST_F(PlanetMapLogicTest, ZoneRgbColorOutOfRangeIsMagenta) {
    EXPECT_EQ(zone_rgb_color(99), (std::array<std::uint8_t, 3>{255, 0, 255}));
}

TEST_F(PlanetMapLogicTest, LabelRgbColorUnknownKindIsNeutralGray) {
    EXPECT_EQ(label_rgb_color("some_future_kind"), (std::array<std::uint8_t, 3>{90, 90, 90}));
}

TEST_F(PlanetMapLogicTest, RenderPngBiomeImageHasUpscaledDimensions) {
    MapTilePayload payload;
    payload.biome.w = 4;
    payload.biome.h = 3;
    payload.biome.data.assign(12, static_cast<std::uint8_t>(4));  // all ocean

    const RgbImage image = render_png_biome_image(payload, /*upscale=*/5);
    EXPECT_EQ(image.w, 20);
    EXPECT_EQ(image.h, 15);
    ASSERT_EQ(image.pixels.size(), static_cast<std::size_t>(20 * 15 * 3));
}

// M344 (MAP22) -- render_png_biome_image() now bilinearly blends between
// neighboring cells' colors (in cell-CENTER space) instead of flat-filling
// each cell's own upscale×upscale block, so a pixel deep inside a cell
// (far from any shared edge) still resolves to that cell's pure color, but
// pixels straddling a boundary between two different biomes are a real
// blend of both -- replacing this test's pre-M344 assumption (every pixel
// in a cell's block is that cell's exact flat color, no exceptions) with
// the actual invariant that DOES still hold post-M344: cell-center purity,
// not blockwide uniformity. With upscale=4 over a 2-cell-wide grid, hand
// worked-out fractional cell-center math places columns 0-1 fully inside
// cell 0's own influence (pure ocean), columns 6-7 fully inside cell 1's
// (pure forest), and columns 2-5 straddling the shared boundary (blended).
TEST_F(PlanetMapLogicTest, RenderPngBiomeImageCellCentersArePureAndBoundariesBlend) {
    MapTilePayload payload;
    payload.biome.w = 2;
    payload.biome.h = 1;
    payload.biome.data = {4, 3};  // ocean, forest
    const auto ocean  = zone_rgb_color(4);
    const auto forest = zone_rgb_color(3);

    const RgbImage image = render_png_biome_image(payload, /*upscale=*/4);
    ASSERT_EQ(image.w, 8);
    ASSERT_EQ(image.h, 4);

    const auto pixel_at = [&](int x, int y) {
        const std::size_t idx = (static_cast<std::size_t>(y) * 8 + static_cast<std::size_t>(x)) * 3;
        return std::array<std::uint8_t, 3>{image.pixels[idx], image.pixels[idx + 1], image.pixels[idx + 2]};
    };

    for (int y = 0; y < 4; ++y) {
        EXPECT_EQ(pixel_at(0, y), ocean)  << "y=" << y;
        EXPECT_EQ(pixel_at(1, y), ocean)  << "y=" << y;
        EXPECT_EQ(pixel_at(6, y), forest) << "y=" << y;
        EXPECT_EQ(pixel_at(7, y), forest) << "y=" << y;

        for (int x = 2; x <= 5; ++x) {
            const auto p = pixel_at(x, y);
            EXPECT_NE(p, ocean)  << "boundary pixel x=" << x << " should be blended, not pure ocean";
            EXPECT_NE(p, forest) << "boundary pixel x=" << x << " should be blended, not pure forest";
        }
    }
}

// A spatially uniform single-biome grid must render exactly as before
// M344 -- bilinear blending between 4 identical-color corners is a no-op,
// so this is a real non-regression guarantee for the overwhelmingly common
// case (most of any generated map is one biome extending over many cells),
// not just an artifact of the blending formula happening to work out.
TEST_F(PlanetMapLogicTest, RenderPngBiomeImageUniformBiomeIsUnaffectedByBlending) {
    MapTilePayload payload;
    payload.biome.w = 5;
    payload.biome.h = 5;
    payload.biome.data.assign(25, static_cast<std::uint8_t>(3));  // all forest

    const RgbImage image  = render_png_biome_image(payload, /*upscale=*/4);
    const auto     forest = zone_rgb_color(3);
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        EXPECT_EQ(image.pixels[i + 0], forest[0]);
        EXPECT_EQ(image.pixels[i + 1], forest[1]);
        EXPECT_EQ(image.pixels[i + 2], forest[2]);
    }
}

TEST_F(PlanetMapLogicTest, RenderPngBiomeImageBlendingIsDeterministic) {
    MapTilePayload payload;
    payload.biome.w = 3;
    payload.biome.h = 3;
    payload.biome.data = {4, 4, 3, 4, 3, 3, 3, 3, 5};

    const RgbImage a = render_png_biome_image(payload, /*upscale=*/6);
    const RgbImage b = render_png_biome_image(payload, /*upscale=*/6);
    EXPECT_EQ(a.pixels, b.pixels);
}

TEST_F(PlanetMapLogicTest, RenderPngBiomeImageDrawsFeatureOverlayDistinctFromBiomeFill) {
    // Whole tile is one biome color; a river feature crosses it corner to
    // corner in world space (payload.tile defaults to level 0 -- the whole
    // planet -- so world_bounds() spans the full planet extent).
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(3));  // all forest

    const auto b = payload.tile.world_bounds();
    payload.features.push_back({FeatureType::River, "Testfluss",
                                 {{b.min_x, b.min_z}, {b.max_x, b.max_z}}, {}});

    const RgbImage image        = render_png_biome_image(payload, /*upscale=*/8);
    const auto     river_color  = feature_rgb_color(FeatureType::River);
    const auto     forest_color = zone_rgb_color(3);

    int river_pixels = 0;
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        if (image.pixels[i] == river_color[0] && image.pixels[i + 1] == river_color[1] &&
            image.pixels[i + 2] == river_color[2])
            ++river_pixels;
    }
    EXPECT_GT(river_pixels, 0);
    EXPECT_NE(river_color, forest_color);
}

// M-fix -- Lake.points is a scattered "every wet cell" cloud (Hydrology::
// fill_basin()'s pop order / city.lua's/metro.lua's near-sea-level scan),
// not an ordered shoreline ring. Before this fix, Lake was drawn through the
// same connect-the-dots polyline path as every other feature, so two
// far-apart wet cells got joined by a straight line straight across the
// tile. Two points at opposite corners must now render ONLY as their own
// filled cells -- the tile center, which the old diagonal line would have
// crossed, must stay pure background.
TEST_F(PlanetMapLogicTest, RenderPngBiomeImageFillsLakeCellsInsteadOfConnectingThemWithALine) {
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(4));  // all ocean (background)

    const auto b = payload.tile.world_bounds();
    const std::array<double, 2> near_corner{b.min_x + 0.02 * (b.max_x - b.min_x),
                                             b.min_z + 0.02 * (b.max_z - b.min_z)};
    const std::array<double, 2> far_corner{b.min_x + 0.98 * (b.max_x - b.min_x),
                                            b.min_z + 0.98 * (b.max_z - b.min_z)};
    payload.features.push_back({FeatureType::Lake, "TestLake", {near_corner, far_corner}, {}});

    const RgbImage image      = render_png_biome_image(payload, /*upscale=*/8);
    const auto     lake_color = feature_rgb_color(FeatureType::Lake);
    const auto     ocean_color = zone_rgb_color(4);

    const int cx = image.w / 2;
    const int cy = image.h / 2;
    const std::size_t center_idx = (static_cast<std::size_t>(cy) * static_cast<std::size_t>(image.w) +
                                     static_cast<std::size_t>(cx)) * 3;
    EXPECT_EQ(image.pixels[center_idx + 0], ocean_color[0]);
    EXPECT_EQ(image.pixels[center_idx + 1], ocean_color[1]);
    EXPECT_EQ(image.pixels[center_idx + 2], ocean_color[2]);

    int lake_pixels = 0;
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        if (image.pixels[i] == lake_color[0] && image.pixels[i + 1] == lake_color[1] &&
            image.pixels[i + 2] == lake_color[2])
            ++lake_pixels;
    }
    EXPECT_GT(lake_pixels, 0) << "each lake point should still paint its own filled cell";
}

// M-fix -- Country::border (Countries::trace_owner_border()) IS a real
// ordered polygon ring, unlike Lake above, so it's a genuine earcut use
// case: a pixel deep inside the ring should read as a blend of the border
// color and the background (a visible tint), while a pixel clearly outside
// the ring must stay the pure, unblended background color.
TEST_F(PlanetMapLogicTest, RenderPngBiomeImageTintsBorderPolygonInterior) {
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(4));  // all ocean (background)

    const auto b  = payload.tile.world_bounds();
    const auto bw = b.max_x - b.min_x;
    const auto bh = b.max_z - b.min_z;
    // A square ring covering the middle third of the tile in both axes.
    const std::array<double, 2> p0{b.min_x + bw / 3.0, b.min_z + bh / 3.0};
    const std::array<double, 2> p1{b.min_x + 2.0 * bw / 3.0, b.min_z + bh / 3.0};
    const std::array<double, 2> p2{b.min_x + 2.0 * bw / 3.0, b.min_z + 2.0 * bh / 3.0};
    const std::array<double, 2> p3{b.min_x + bw / 3.0, b.min_z + 2.0 * bh / 3.0};
    payload.features.push_back({FeatureType::Border, "Testonia", {p0, p1, p2, p3}, {}});

    const RgbImage image        = render_png_biome_image(payload, /*upscale=*/8);
    const auto     border_color = feature_rgb_color(FeatureType::Border);
    const auto     ocean_color  = zone_rgb_color(4);

    const auto pixel_at = [&](int x, int y) {
        const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.w) +
                                  static_cast<std::size_t>(x)) * 3;
        return std::array<std::uint8_t, 3>{image.pixels[idx], image.pixels[idx + 1], image.pixels[idx + 2]};
    };

    const auto inside  = pixel_at(image.w / 2, image.h / 2);
    const auto outside = pixel_at(2, 2);

    EXPECT_EQ(outside, ocean_color) << "well outside the ring must stay pure background";
    EXPECT_NE(inside, ocean_color) << "well inside the ring must be tinted, not pure background";
    EXPECT_NE(inside, border_color) << "a tint blends toward the border color, it isn't opaque";
}

TEST_F(PlanetMapLogicTest, RenderPngBiomeImageDrawsLabelMarker) {
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(4));  // all ocean

    const auto b = payload.tile.world_bounds();
    const std::array<double, 2> center{(b.min_x + b.max_x) / 2.0, (b.min_z + b.max_z) / 2.0};
    payload.labels.push_back({"Capitalia", center, "capital"});

    const RgbImage image        = render_png_biome_image(payload, /*upscale=*/8);
    const auto     capital_color = label_rgb_color("capital");

    int marker_pixels = 0;
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        if (image.pixels[i] == capital_color[0] && image.pixels[i + 1] == capital_color[1] &&
            image.pixels[i + 2] == capital_color[2])
            ++marker_pixels;
    }
    EXPECT_GT(marker_pixels, 0);
}

// M346 (MAP22) -- label declutter: a lower-priority label whose marker
// would overlap a higher-priority one (label_priority(), matching
// label_rgb_color()'s own kind ordering) must be dropped, not drawn on top
// of / stacked with it. Same exact world position guarantees pixel
// collision regardless of upscale.
TEST_F(PlanetMapLogicTest, RenderPngBiomeImageDropsLowerPriorityLabelWhenItWouldOverlapAHigherPriorityOne) {
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(4));  // all ocean

    const auto b = payload.tile.world_bounds();
    const std::array<double, 2> center{(b.min_x + b.max_x) / 2.0, (b.min_z + b.max_z) / 2.0};
    payload.labels.push_back({"Villagetown", center, "village"});  // added first, but lower priority
    payload.labels.push_back({"Capitalia", center, "capital"});

    const RgbImage image         = render_png_biome_image(payload, /*upscale=*/8);
    const auto     capital_color = label_rgb_color("capital");
    const auto     village_color = label_rgb_color("village");

    bool saw_capital = false, saw_village = false;
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        if (image.pixels[i] == capital_color[0] && image.pixels[i + 1] == capital_color[1] &&
            image.pixels[i + 2] == capital_color[2])
            saw_capital = true;
        if (image.pixels[i] == village_color[0] && image.pixels[i + 1] == village_color[1] &&
            image.pixels[i + 2] == village_color[2])
            saw_village = true;
    }
    EXPECT_TRUE(saw_capital) << "the higher-priority label must still be drawn";
    EXPECT_FALSE(saw_village) << "the lower-priority label colliding with it must be dropped";
}

TEST_F(PlanetMapLogicTest, RenderPngBiomeImageKeepsBothLabelsWhenFarApart) {
    MapTilePayload payload;
    payload.biome.w = 16;
    payload.biome.h = 16;
    payload.biome.data.assign(256, static_cast<std::uint8_t>(4));

    const auto   b  = payload.tile.world_bounds();
    const double x0 = b.min_x + 0.1 * (b.max_x - b.min_x);
    const double z0 = b.min_z + 0.1 * (b.max_z - b.min_z);
    const double x1 = b.min_x + 0.9 * (b.max_x - b.min_x);
    const double z1 = b.min_z + 0.9 * (b.max_z - b.min_z);
    payload.labels.push_back({"Capitalia", {x0, z0}, "capital"});
    payload.labels.push_back({"Villagetown", {x1, z1}, "village"});

    const RgbImage image         = render_png_biome_image(payload, /*upscale=*/8);
    const auto     capital_color = label_rgb_color("capital");
    const auto     village_color = label_rgb_color("village");

    bool saw_capital = false, saw_village = false;
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        if (image.pixels[i] == capital_color[0] && image.pixels[i + 1] == capital_color[1] &&
            image.pixels[i + 2] == capital_color[2])
            saw_capital = true;
        if (image.pixels[i] == village_color[0] && image.pixels[i + 1] == village_color[1] &&
            image.pixels[i + 2] == village_color[2])
            saw_village = true;
    }
    EXPECT_TRUE(saw_capital);
    EXPECT_TRUE(saw_village) << "far-apart labels must not be decluttered against each other";
}

TEST_F(PlanetMapLogicTest, WritePngFileRoundTripsAValidPngSignature) {
    RgbImage image;
    image.w = 3;
    image.h = 2;
    image.pixels.assign(static_cast<std::size_t>(image.w * image.h * 3), std::uint8_t{128});

    const std::string path = tmp_dir("png_write") + ".png";
    ASSERT_TRUE(write_png_file(path, image));

    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    unsigned char sig[8] = {};
    file.read(reinterpret_cast<char*>(sig), 8);
    static constexpr unsigned char kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    EXPECT_TRUE(std::equal(std::begin(sig), std::end(sig), std::begin(kPngSignature)));
}

TEST_F(PlanetMapLogicTest, WritePngFileFailsForUnwritablePath) {
    RgbImage image;
    image.w = 1;
    image.h = 1;
    image.pixels.assign(3, std::uint8_t{0});
    EXPECT_FALSE(write_png_file("/no/such/directory/out.png", image));
}

// --- M279: --legend --------------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsLegendFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--legend", "/tmp/legend.png"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.legend_path, "/tmp/legend.png");
}

TEST_F(PlanetMapLogicTest, RenderZoneLegendImageHasOneSwatchPerZoneType) {
    const RgbImage image = render_zone_legend_image(/*swatch_px=*/10, /*gap_px=*/2, /*cols=*/13);
    // 52 entries / 13 cols = exactly 4 rows.
    EXPECT_EQ(image.w, 13 * 10 + 14 * 2);
    EXPECT_EQ(image.h, 4 * 10 + 5 * 2);
    ASSERT_EQ(image.pixels.size(), static_cast<std::size_t>(image.w) * static_cast<std::size_t>(image.h) * 3);
}

TEST_F(PlanetMapLogicTest, RenderZoneLegendImageSwatchesMatchZoneRgbColorByOrdinal) {
    const RgbImage image = render_zone_legend_image(/*swatch_px=*/10, /*gap_px=*/2, /*cols=*/13);
    for (int ordinal = 0; ordinal < static_cast<int>(ZONE_NAMES.size()); ++ordinal) {
        const int col = ordinal % 13;
        const int row = ordinal / 13;
        // Sample the swatch's center pixel.
        const int x = 2 + col * 12 + 5;
        const int y = 2 + row * 12 + 5;
        const std::size_t idx = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.w) +
                                  static_cast<std::size_t>(x)) * 3;
        const auto expected = zone_rgb_color(ordinal);
        EXPECT_EQ(image.pixels[idx + 0], expected[0]) << "ordinal " << ordinal;
        EXPECT_EQ(image.pixels[idx + 1], expected[1]) << "ordinal " << ordinal;
        EXPECT_EQ(image.pixels[idx + 2], expected[2]) << "ordinal " << ordinal;
    }
}

TEST_F(PlanetMapLogicTest, WritePngFileFailsForEmptyImage) {
    const RgbImage image;  // w=h=0
    EXPECT_FALSE(write_png_file(tmp_dir("png_empty") + ".png", image));
}

// --- MAP15 M229: --png-region -------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsPngRegionFlagWithDefaultRegionTiles) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-region", "out.png"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.png_region_path, "out.png");
    EXPECT_EQ(args.region_tiles, 2);
}

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsRegionTilesFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-region", "out.png",
                           "--region-tiles", "4"};
    const auto [args, ok] = parse_planet_map_args(6, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.region_tiles, 4);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsRegionTilesBelowOneWhenPngRegionRequested) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-region", "out.png",
                           "--region-tiles", "0"};
    const auto [args, ok] = parse_planet_map_args(6, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, RenderPngRegionImageStitchesNxNGridOfTiles) {
    auto        world = PlanetWorld::create_new(tmp_dir("region_stitch"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    // Level 3 has an 8x8 tile grid -- (1,1) with n=3 stays fully in range.
    const RgbImage image = render_png_region_image(pipeline, TileCoord{3, 1, 1}, /*n=*/3,
                                                     /*upscale=*/2);

    const int expected_tile_dim = 64 * 2;  // GRID_SIZE (PlanetGenerator.cpp) * upscale
    EXPECT_EQ(image.w, expected_tile_dim * 3);
    EXPECT_EQ(image.h, expected_tile_dim * 3);
    ASSERT_EQ(image.pixels.size(), static_cast<std::size_t>(image.w) * static_cast<std::size_t>(image.h) * 3);
}

TEST_F(PlanetMapLogicTest, RenderPngRegionImageLeavesOutOfRangeTilesBlack) {
    auto        world = PlanetWorld::create_new(tmp_dir("region_edge"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    // Level 3's valid index range is [0,8); starting at the last tile with
    // n=2 means 3 of the 4 requested tiles fall outside the planet.
    const RgbImage image = render_png_region_image(pipeline, TileCoord{3, 7, 7}, /*n=*/2,
                                                     /*upscale=*/2);

    const int tile_dim = 64 * 2;
    ASSERT_EQ(image.w, tile_dim * 2);
    ASSERT_EQ(image.h, tile_dim * 2);

    // Bottom-right cell (tile (8,8)) is entirely out of range -> left black.
    const std::size_t idx = (static_cast<std::size_t>(image.h - 1) * static_cast<std::size_t>(image.w) +
                              static_cast<std::size_t>(image.w - 1)) * 3;
    EXPECT_EQ(image.pixels[idx + 0], 0);
    EXPECT_EQ(image.pixels[idx + 1], 0);
    EXPECT_EQ(image.pixels[idx + 2], 0);
}

TEST_F(PlanetMapLogicTest, RenderPngRegionImageEmptyWhenEveryTileOutOfRange) {
    auto        world = PlanetWorld::create_new(tmp_dir("region_all_oob"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const RgbImage image = render_png_region_image(pipeline, TileCoord{3, 99, 99}, /*n=*/2);
    EXPECT_EQ(image.w, 0);
    EXPECT_EQ(image.h, 0);
    EXPECT_TRUE(image.pixels.empty());
}

// --- MAP15 M230: --geojson -----------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsGeojsonFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--geojson", "out.geojson"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.geojson_path, "out.geojson");
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsGeojsonPathToEmpty) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.geojson_path.empty());
}

TEST_F(PlanetMapLogicTest, RenderGeojsonIsAFeatureCollection) {
    const MapTilePayload payload;
    const auto            j = nlohmann::json::parse(render_geojson(payload));
    EXPECT_EQ(j.at("type").get<std::string>(), "FeatureCollection");
    EXPECT_TRUE(j.at("features").empty());
}

TEST_F(PlanetMapLogicTest, RenderGeojsonSinglePointFeatureBecomesPointGeometry) {
    MapTilePayload payload;
    payload.features.push_back({FeatureType::Continent, "Eurasia", {{100.0, 200.0}}, {}});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    ASSERT_EQ(j.at("features").size(), 1u);
    const auto& f = j.at("features")[0];
    EXPECT_EQ(f.at("geometry").at("type").get<std::string>(), "Point");
    EXPECT_EQ(f.at("geometry").at("coordinates")[0].get<double>(), 100.0);
    EXPECT_EQ(f.at("geometry").at("coordinates")[1].get<double>(), 200.0);
    EXPECT_EQ(f.at("properties").at("kind").get<std::string>(), "continent");
    EXPECT_EQ(f.at("properties").at("name").get<std::string>(), "Eurasia");
}

TEST_F(PlanetMapLogicTest, RenderGeojsonOpenPathBecomesLineString) {
    MapTilePayload payload;
    payload.features.push_back(
        {FeatureType::River, "Skarnfoss", {{0.0, 0.0}, {10.0, 10.0}, {20.0, 5.0}}, {}});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    ASSERT_EQ(j.at("features").size(), 1u);
    EXPECT_EQ(j.at("features")[0].at("geometry").at("type").get<std::string>(), "LineString");
    EXPECT_EQ(j.at("features")[0].at("geometry").at("coordinates").size(), 3u);
}

TEST_F(PlanetMapLogicTest, RenderGeojsonClosedLoopBecomesPolygon) {
    MapTilePayload payload;
    // Countries::grow()-style closed loop: first point repeated as the last.
    payload.features.push_back({FeatureType::Border, "Vecvicvia",
                                 {{0.0, 0.0}, {10.0, 0.0}, {10.0, 10.0}, {0.0, 0.0}}, {}});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    ASSERT_EQ(j.at("features").size(), 1u);
    const auto& geometry = j.at("features")[0].at("geometry");
    EXPECT_EQ(geometry.at("type").get<std::string>(), "Polygon");
    ASSERT_EQ(geometry.at("coordinates").size(), 1u);       // one ring
    EXPECT_EQ(geometry.at("coordinates")[0].size(), 4u);    // all 4 points kept
}

TEST_F(PlanetMapLogicTest, RenderGeojsonSkipsFeaturesWithNoPoints) {
    MapTilePayload payload;
    payload.features.push_back({FeatureType::Other, "NoGeometry", {}, {}});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    EXPECT_TRUE(j.at("features").empty());
}

TEST_F(PlanetMapLogicTest, RenderGeojsonIncludesLabelsAsPointFeatures) {
    MapTilePayload payload;
    payload.labels.push_back({"Springfield", {1.0, 2.0}, "capital"});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    ASSERT_EQ(j.at("features").size(), 1u);
    const auto& f = j.at("features")[0];
    EXPECT_EQ(f.at("geometry").at("type").get<std::string>(), "Point");
    EXPECT_EQ(f.at("properties").at("kind").get<std::string>(), "capital");
    EXPECT_EQ(f.at("properties").at("name").get<std::string>(), "Springfield");
}

TEST_F(PlanetMapLogicTest, RenderGeojsonFeatureCountMatchesNonEmptyFeaturesPlusLabels) {
    // MAP15 M233: output feature count must match the source payload --
    // every geometric feature (points.size() > 0) plus every label, no
    // more, no less. Mixes River/Border/skipped-empty features with labels.
    MapTilePayload payload;
    payload.features.push_back({FeatureType::River, "R1", {{0.0, 0.0}, {1.0, 1.0}}, {}});
    payload.features.push_back({FeatureType::Border, "B1", {{0.0, 0.0}}, {}});
    payload.features.push_back({FeatureType::Other, "NoGeometry", {}, {}});  // skipped
    payload.labels.push_back({"L1", {0.0, 0.0}, "city"});
    payload.labels.push_back({"L2", {1.0, 1.0}, "town"});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    const std::size_t non_empty_features =
        static_cast<std::size_t>(std::count_if(payload.features.begin(), payload.features.end(),
                                                 [](const MapFeature& f) { return !f.points.empty(); }));
    EXPECT_EQ(j.at("features").size(), non_empty_features + payload.labels.size());
    EXPECT_EQ(j.at("features").size(), 4u);
}

// M346 (MAP22) -- render_geojson() dedupes a true exact duplicate (same
// name+kind+position -- e.g. two independent label sources both appending
// the same settlement) but keeps every genuinely distinct label,
// regardless of how close together they are: a vector consumer decides its
// own declutter, unlike the fixed-resolution PNG render above.
TEST_F(PlanetMapLogicTest, RenderGeojsonDedupesExactDuplicateLabels) {
    MapTilePayload payload;
    payload.labels.push_back({"Springfield", {1.0, 2.0}, "capital"});
    payload.labels.push_back({"Springfield", {1.0, 2.0}, "capital"});  // exact duplicate

    const auto j = nlohmann::json::parse(render_geojson(payload));
    EXPECT_EQ(j.at("features").size(), 1u);
}

TEST_F(PlanetMapLogicTest, RenderGeojsonKeepsLabelsAtDifferentPositionsEvenWithSameNameAndKind) {
    MapTilePayload payload;
    payload.labels.push_back({"Springfield", {1.0, 2.0}, "city"});
    payload.labels.push_back({"Springfield", {3.0, 4.0}, "city"});  // same name/kind, different position

    const auto j = nlohmann::json::parse(render_geojson(payload));
    EXPECT_EQ(j.at("features").size(), 2u);
}

TEST_F(PlanetMapLogicTest, RenderGeojsonLabelFeaturesIncludeLabelPriority) {
    MapTilePayload payload;
    payload.labels.push_back({"Capitalia", {0.0, 0.0}, "capital"});
    payload.labels.push_back({"Riverside", {1.0, 1.0}, "river"});

    const auto j = nlohmann::json::parse(render_geojson(payload));
    ASSERT_EQ(j.at("features").size(), 2u);
    EXPECT_LT(j.at("features")[0].at("properties").at("labelPriority").get<int>(),
              j.at("features")[1].at("properties").at("labelPriority").get<int>())
        << "capital must rank ahead of river";
}

TEST_F(PlanetMapLogicTest, WriteTextFileRoundTrips) {
    const std::string path = tmp_dir("text_write") + ".txt";
    ASSERT_TRUE(write_text_file(path, "hello world"));

    std::ifstream file(path);
    std::stringstream ss;
    ss << file.rdbuf();
    EXPECT_EQ(ss.str(), "hello world");
}

TEST_F(PlanetMapLogicTest, WriteTextFileFailsForUnwritablePath) {
    EXPECT_FALSE(write_text_file("/no/such/directory/out.geojson", "x"));
}

// --- MAP15 M232: --png-mode hillshade ---------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsPngModeHillshade) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-mode", "hillshade"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.png_hillshade);
}

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsPngModeBiomeExplicitly) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-mode", "biome"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_FALSE(args.png_hillshade);
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsPngModeToBiome) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_FALSE(args.png_hillshade);
}

TEST_F(PlanetMapLogicTest, ParseArgsRejectsUnknownPngMode) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--png-mode", "bogus"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    (void)args;
    EXPECT_FALSE(ok);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageHasUpscaledDimensions) {
    MapTilePayload payload;
    payload.elevation.w = 4;
    payload.elevation.h = 3;
    payload.elevation.data.assign(12, 100.0f);

    const RgbImage image = render_png_elevation_image(payload, /*upscale=*/5);
    EXPECT_EQ(image.w, 20);
    EXPECT_EQ(image.h, 15);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageIsGrayscale) {
    MapTilePayload payload;
    payload.elevation.w = 3;
    payload.elevation.h = 3;
    payload.elevation.data = {0, 100, 200, 300, 400, 500, 600, 700, 800};

    const RgbImage image = render_png_elevation_image(payload, /*upscale=*/2);
    for (std::size_t i = 0; i < image.pixels.size(); i += 3) {
        EXPECT_EQ(image.pixels[i], image.pixels[i + 1]);
        EXPECT_EQ(image.pixels[i], image.pixels[i + 2]);
    }
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageFlatGridIsUniformMidGray) {
    MapTilePayload payload;
    payload.elevation.w = 4;
    payload.elevation.h = 4;
    payload.elevation.data.assign(16, 500.0f);  // perfectly flat -> zero slope everywhere

    const RgbImage image = render_png_elevation_image(payload, /*upscale=*/2);
    const std::uint8_t first = image.pixels[0];
    for (std::size_t i = 0; i < image.pixels.size(); ++i) EXPECT_EQ(image.pixels[i], first);
    // Flat terrain's normal is straight up (0,0,1); brightness = light_z ~= 0.577.
    EXPECT_NEAR(first, static_cast<int>(0.5773503f * 255.0f), 1);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageSlopeTowardLightIsBrighterThanAwayFromIt) {
    // Two opposite constant slopes; compare each grid's *center* cell (a
    // genuine central difference, not an edge-clamped one -- a uniform
    // ramp's edge cells clamp to the same one-sided slope as the interior,
    // so edge pixels can't distinguish slope direction the way the center
    // pixel can). The light is fixed at azimuth upper-left (-x,-y): a slope
    // rising left-to-right tilts its normal toward -x (facing the light) --
    // brighter; a slope falling left-to-right tilts toward +x (facing away)
    // -- darker.
    MapTilePayload rising;
    rising.elevation.w = 3;
    rising.elevation.h = 1;
    rising.elevation.data = {0.0f, 1000.0f, 2000.0f};

    MapTilePayload falling;
    falling.elevation.w = 3;
    falling.elevation.h = 1;
    falling.elevation.data = {2000.0f, 1000.0f, 0.0f};

    const RgbImage rising_image  = render_png_elevation_image(rising, /*upscale=*/1);
    const RgbImage falling_image = render_png_elevation_image(falling, /*upscale=*/1);
    const std::uint8_t rising_center_gray  = rising_image.pixels[1 * 3];
    const std::uint8_t falling_center_gray = falling_image.pixels[1 * 3];
    EXPECT_GT(rising_center_gray, falling_center_gray);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageEmptyGridProducesEmptyImage) {
    const MapTilePayload payload;  // elevation.w == elevation.h == 0
    const RgbImage        image = render_png_elevation_image(payload);
    EXPECT_EQ(image.w, 0);
    EXPECT_EQ(image.h, 0);
}

// --- M343 (MAP22): neighbor-aware hillshade, fixing the tile-boundary
// brightness seam a one-sided clamped slope estimate produced -----------

// A continuous 6-cell ramp split into two 3-cell tiles. Rendering the west
// tile's own rightmost column WITH the east tile passed as its neighbor
// must reproduce the exact same brightness a single unsplit 6-cell render
// gives at that same world position -- proving the real neighbor value is
// actually used for the slope estimate, not just accepted and ignored.
TEST_F(PlanetMapLogicTest, RenderPngElevationImageUsesRealNeighborForEdgeSlopeWhenProvided) {
    MapTilePayload combined;
    combined.elevation.w = 6;
    combined.elevation.h = 1;
    combined.elevation.data = {0, 1000, 2000, 3000, 4000, 5000};
    const RgbImage combined_image = render_png_elevation_image(combined, /*upscale=*/1);
    const std::uint8_t expected_gray = combined_image.pixels[2 * 3];  // seam-adjacent cell

    MapTilePayload west;
    west.elevation.w = 3;
    west.elevation.h = 1;
    west.elevation.data = {0, 1000, 2000};

    Map::FieldGrid east;
    east.w = 3;
    east.h = 1;
    east.data = {3000, 4000, 5000};

    const RgbImage with_neighbor =
        render_png_elevation_image(west, /*upscale=*/1, nullptr, &east, nullptr, nullptr);
    EXPECT_EQ(with_neighbor.pixels[2 * 3], expected_gray);
}

// Without a neighbor, the pre-M343 clamped estimate must NOT accidentally
// match the true continuous-ramp value -- otherwise the test above would
// be vacuous (both branches producing the same answer regardless of
// whether the neighbor is actually consulted).
TEST_F(PlanetMapLogicTest, RenderPngElevationImageWithoutNeighborDiffersFromTheTrueContinuousSlope) {
    MapTilePayload west;
    west.elevation.w = 3;
    west.elevation.h = 1;
    west.elevation.data = {0, 1000, 2000};
    const RgbImage no_neighbor = render_png_elevation_image(west, /*upscale=*/1);

    MapTilePayload combined;
    combined.elevation.w = 6;
    combined.elevation.h = 1;
    combined.elevation.data = {0, 1000, 2000, 3000, 4000, 5000};
    const RgbImage combined_image = render_png_elevation_image(combined, /*upscale=*/1);

    EXPECT_NE(no_neighbor.pixels[2 * 3], combined_image.pixels[2 * 3]);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageMismatchedNeighborSizeIsIgnored) {
    MapTilePayload west;
    west.elevation.w = 3;
    west.elevation.h = 1;
    west.elevation.data = {0, 1000, 2000};

    Map::FieldGrid wrong_size_east;
    wrong_size_east.w = 5;  // mismatched width -- must be ignored, not misread
    wrong_size_east.h = 1;
    wrong_size_east.data = {9, 9, 9, 9, 9};

    const RgbImage with_bad_neighbor =
        render_png_elevation_image(west, /*upscale=*/1, nullptr, &wrong_size_east, nullptr, nullptr);
    const RgbImage without_neighbor = render_png_elevation_image(west, /*upscale=*/1);
    EXPECT_EQ(with_bad_neighbor.pixels, without_neighbor.pixels);
}

TEST_F(PlanetMapLogicTest, RenderPngElevationImageEmptyNeighborIsIgnored) {
    MapTilePayload west;
    west.elevation.w = 3;
    west.elevation.h = 1;
    west.elevation.data = {0, 1000, 2000};

    Map::FieldGrid empty_east;  // default-constructed: empty
    const RgbImage with_empty_neighbor =
        render_png_elevation_image(west, /*upscale=*/1, nullptr, &empty_east, nullptr, nullptr);
    const RgbImage without_neighbor = render_png_elevation_image(west, /*upscale=*/1);
    EXPECT_EQ(with_empty_neighbor.pixels, without_neighbor.pixels);
}

// North uses the same logic as East (see the West/East pair above) --
// covers the other axis so a transposed dzdx/dzdy mix-up would be caught.
TEST_F(PlanetMapLogicTest, RenderPngElevationImageUsesRealNorthNeighborToo) {
    MapTilePayload combined;
    combined.elevation.w = 1;
    combined.elevation.h = 6;
    combined.elevation.data = {0, 1000, 2000, 3000, 4000, 5000};
    const RgbImage combined_image = render_png_elevation_image(combined, /*upscale=*/1);
    const std::uint8_t expected_gray = combined_image.pixels[2 * 3];

    MapTilePayload north;
    north.elevation.w = 1;
    north.elevation.h = 3;
    north.elevation.data = {0, 1000, 2000};

    Map::FieldGrid south;
    south.w = 1;
    south.h = 3;
    south.data = {3000, 4000, 5000};

    const RgbImage with_neighbor =
        render_png_elevation_image(north, /*upscale=*/1, nullptr, nullptr, &south, nullptr);
    EXPECT_EQ(with_neighbor.pixels[2 * 3], expected_gray);
}

// Interior pixels (far from any edge) must be completely unaffected by
// whether a neighbor is passed -- this fix is edge-only.
TEST_F(PlanetMapLogicTest, RenderPngElevationImageInteriorPixelsUnaffectedByNeighbors) {
    MapTilePayload payload;
    payload.elevation.w = 5;
    payload.elevation.h = 5;
    payload.elevation.data = {
        0, 100, 200, 300, 400,
        500, 600, 700, 800, 900,
        1000, 1100, 1200, 1300, 1400,
        1500, 1600, 1700, 1800, 1900,
        2000, 2100, 2200, 2300, 2400,
    };
    Map::FieldGrid neighbor;
    neighbor.w = 5;
    neighbor.h = 5;
    neighbor.data.assign(25, 9999.0f);

    const RgbImage without = render_png_elevation_image(payload, /*upscale=*/1);
    const RgbImage with    = render_png_elevation_image(payload, /*upscale=*/1,
                                                       &neighbor, &neighbor, &neighbor, &neighbor);
    // Center pixel (gx=2, gy=2): index (2*5+2)*3.
    const std::size_t center_idx = (2 * 5 + 2) * 3;
    EXPECT_EQ(without.pixels[center_idx], with.pixels[center_idx]);
}

// --- MAP15 M231: --mbtiles -------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsMbtilesFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--mbtiles", "out.mbtiles"};
    const auto [args, ok] = parse_planet_map_args(4, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_EQ(args.mbtiles_path, "out.mbtiles");
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsMbtilesPathToEmpty) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.mbtiles_path.empty());
}

TEST_F(PlanetMapLogicTest, ExportMbtilesReturnsZeroForALevelWithNoGeneratedTiles) {
    const std::string world_dir = tmp_dir("mbtiles_empty");
    const std::string out_path  = world_dir + "_out.mbtiles";
    EXPECT_EQ(export_mbtiles(world_dir, /*level=*/4, out_path), 0);
}

TEST_F(PlanetMapLogicTest, ExportMbtilesPackagesEveryGeneratedTileAtThatLevel) {
    const std::string world_dir = tmp_dir("mbtiles_packages");
    auto        world    = PlanetWorld::create_new(world_dir);
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    // Generate (and thus persist) 3 sibling tiles at level 4.
    pipeline.get(TileCoord{4, 0, 0});
    pipeline.get(TileCoord{4, 1, 0});
    pipeline.get(TileCoord{4, 0, 1});

    const std::string out_path = world_dir + "_out.mbtiles";
    const int         packaged = export_mbtiles(world_dir, /*level=*/4, out_path);
    EXPECT_EQ(packaged, 3);

    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(out_path.c_str(), &db), SQLITE_OK);

    sqlite3_stmt* meta_stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT value FROM metadata WHERE name = 'format';", -1, &meta_stmt, nullptr);
    ASSERT_EQ(sqlite3_step(meta_stmt), SQLITE_ROW);
    EXPECT_STREQ(reinterpret_cast<const char*>(sqlite3_column_text(meta_stmt, 0)), "png");
    sqlite3_finalize(meta_stmt);

    sqlite3_stmt* count_stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM tiles;", -1, &count_stmt, nullptr);
    ASSERT_EQ(sqlite3_step(count_stmt), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(count_stmt, 0), 3);
    sqlite3_finalize(count_stmt);

    // Level 4 spans [0,16); tile (0,0)'s TMS row is 16-1-0 = 15.
    sqlite3_stmt* row_stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT tile_row, length(tile_data) FROM tiles WHERE tile_column = 0 AND tile_row = 15;",
                        -1, &row_stmt, nullptr);
    ASSERT_EQ(sqlite3_step(row_stmt), SQLITE_ROW);
    EXPECT_GT(sqlite3_column_int(row_stmt, 1), 0);  // non-empty PNG blob
    sqlite3_finalize(row_stmt);

    sqlite3_close(db);
}

TEST_F(PlanetMapLogicTest, ExportMbtilesReturnsNegativeOneForUnwritablePath) {
    const std::string world_dir = tmp_dir("mbtiles_unwritable");
    EXPECT_EQ(export_mbtiles(world_dir, /*level=*/4, "/no/such/directory/out.mbtiles"), -1);
}

// --- MAP13 M210: --validate ---------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsValidateFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--validate"};
    const auto [args, ok] = parse_planet_map_args(3, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.validate);
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsValidateToFalse) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_FALSE(args.validate);
}

// A world directory with no map_level*.db files yet must not be treated as
// an error, and must not create one as a side effect (see
// discover_persisted_levels()'s doc comment in PlanetMapLogic.cpp).
TEST_F(PlanetMapLogicTest, ValidateWorldOnDirWithNoPersistedTilesIsOkAndCreatesNothing) {
    const std::string world_dir = tmp_dir("validate_empty");
    std::filesystem::remove_all(world_dir);

    const WorldValidationResult result = validate_world(world_dir);
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.tiles_checked, 0);
    EXPECT_TRUE(result.issues.empty());
    EXPECT_FALSE(std::filesystem::exists(world_dir))
        << "validate_world() must not create map_level*.db files for levels that were never generated";
}

TEST_F(PlanetMapLogicTest, ValidateWorldChecksEveryPersistedTileAcrossLevels) {
    const std::string world_dir = tmp_dir("validate_multi_level");
    auto        world    = PlanetWorld::create_new(world_dir);
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    pipeline.get(TileCoord{0, 0, 0});
    pipeline.get(TileCoord{4, 0, 0});
    pipeline.get(TileCoord{4, 1, 0});
    pipeline.get(TileCoord{7, 2, 3});

    const WorldValidationResult result = validate_world(world_dir);
    EXPECT_TRUE(result.ok());
    // Level 4's two tiles generate their level-0/etc. ancestors along the
    // way (M105/M106), and level 7's tile generates its own ancestor chain
    // too, so the true count is >= the 4 explicitly requested tiles.
    EXPECT_GE(result.tiles_checked, 4);
}

TEST_F(PlanetMapLogicTest, ValidateWorldFlagsACorruptedPersistedTile) {
    const std::string world_dir = tmp_dir("validate_corrupt");
    auto        world    = PlanetWorld::create_new(world_dir);
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const TileCoord tile = TileCoord{4, 0, 0};
    MapTilePayload   payload = pipeline.get(tile);
    payload.biome.data[0] = 255;  // out of ZoneType range -- MapValidator must catch this
    world.tile_store(tile.level).store(tile, payload);

    const WorldValidationResult result = validate_world(world_dir);
    EXPECT_FALSE(result.ok());
    ASSERT_EQ(result.issues.size(), 1u);
    EXPECT_EQ(result.issues.front().tile, tile);
    EXPECT_FALSE(result.issues.front().errors.empty());
}

// --- MAP14 M223: --db-sizes ---------------------------------------------------

TEST_F(PlanetMapLogicTest, ParseArgsAcceptsDbSizesFlag) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir", "--db-sizes"};
    const auto [args, ok] = parse_planet_map_args(3, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_TRUE(args.db_sizes);
}

TEST_F(PlanetMapLogicTest, ParseArgsDefaultsDbSizesToFalse) {
    const char* argv[] = {"MeshWorldPlanet", "/some/dir"};
    const auto [args, ok] = parse_planet_map_args(2, const_cast<char**>(argv));
    ASSERT_TRUE(ok);
    EXPECT_FALSE(args.db_sizes);
}

TEST_F(PlanetMapLogicTest, AuditDbSizesOnDirWithNothingGeneratedIsEmpty) {
    const std::string world_dir = tmp_dir("db_sizes_empty");
    std::filesystem::remove_all(world_dir);

    const DbSizeReport report = audit_db_sizes(world_dir);
    EXPECT_TRUE(report.map_level_dbs.empty());
    EXPECT_TRUE(report.region_dbs.empty());
    EXPECT_EQ(report.total_bytes, 0u);
}

TEST_F(PlanetMapLogicTest, AuditDbSizesReportsEveryPersistedMapLevel) {
    const std::string world_dir = tmp_dir("db_sizes_levels");
    auto        world    = PlanetWorld::create_new(world_dir);
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    pipeline.get(TileCoord{0, 0, 0});
    pipeline.get(TileCoord{4, 0, 0});
    pipeline.get(TileCoord{7, 2, 3});

    const DbSizeReport report = audit_db_sizes(world_dir);
    // Levels 0, 4, and 7's own ancestor chain (1, 2, 3, 5, 6) are all
    // persisted along the way -- 8 distinct levels total (0-7).
    ASSERT_EQ(report.map_level_dbs.size(), 8u);
    std::uint64_t summed = 0;
    for (const DbSizeEntry& e : report.map_level_dbs) {
        EXPECT_GT(e.size_bytes, 0u) << e.label;
        EXPECT_TRUE(e.label.rfind("map_level", 0) == 0) << e.label;
        summed += e.size_bytes;
    }
    EXPECT_TRUE(report.region_dbs.empty());
    EXPECT_EQ(report.total_bytes, summed);
}

TEST_F(PlanetMapLogicTest, AuditDbSizesReportsRegionShards) {
    const std::string world_dir = tmp_dir("db_sizes_regions");
    std::filesystem::remove_all(world_dir);
    std::filesystem::create_directories(world_dir);

    const RegionId region{2, -1};
    {
        ModelPlacementStore store(world_dir, region);
        ModelPlacement       p;
        p.definition_id = "tree_oak";
        p.pos_x = 130.0;
        p.pos_y = 10.0;
        p.pos_z = 5.0;
        store.insert_batch(ChunkCoord{2, 1}, {p});
    }

    const DbSizeReport report = audit_db_sizes(world_dir);
    EXPECT_TRUE(report.map_level_dbs.empty());
    ASSERT_EQ(report.region_dbs.size(), 1u);
    EXPECT_EQ(report.region_dbs.front().label, "models/2_-1.db");
    EXPECT_GT(report.region_dbs.front().size_bytes, 0u);
    EXPECT_EQ(report.total_bytes, report.region_dbs.front().size_bytes);
}

// --- M211: full level-0 map generation without crashing ----------------------

TEST_F(PlanetMapLogicTest, SummarizeLevel0MapDoesNotCrashAndCountsAllCells) {
    auto world = PlanetWorld::create_new(tmp_dir("level0"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const MapTilePayload payload = pipeline.get(TileCoord{0, 0, 0});
    const TileSummary    summary = summarize_tile(payload);

    const int total_cells = payload.elevation.w * payload.elevation.h;
    EXPECT_EQ(summary.land_cells + summary.ocean_cells, total_cells);
    EXPECT_LE(summary.elevation_min, summary.elevation_max);

    const auto hist = biome_histogram(payload.biome);
    int        hist_total = 0;
    for (int c : hist) hist_total += c;
    EXPECT_EQ(hist_total, total_cells);
}

TEST_F(PlanetMapLogicTest, SummarizeDeepTileDoesNotCrash) {
    auto world = PlanetWorld::create_new(tmp_dir("deep"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    // Exercises the full recursive-ancestor-generation path (M105/M106), not
    // just a level-0 payload.
    const MapTilePayload payload = pipeline.get(TileCoord{10, 123, 456});
    const TileSummary    summary = summarize_tile(payload);
    EXPECT_EQ(summary.land_cells + summary.ocean_cells, payload.elevation.w * payload.elevation.h);
}

// --- MAP13 M212: --stats numbers stay within sane bounds ---------------------
//
// M211 only asserts "doesn't crash"; this asserts the actual numbers
// print_feature_stats() would show are plausible, not just present. World
// entropy is time-seeded (never a fixed seed, see NEXT.md §1), so this
// samples several freshly generated level-0 maps rather than trusting one.

TEST_F(PlanetMapLogicTest, FeatureStatsForFreshLevel0MapsStayWithinSaneBounds) {
    for (int sample = 0; sample < 8; ++sample) {
        SCOPED_TRACE("sample " + std::to_string(sample));

        auto world = PlanetWorld::create_new(tmp_dir("stats_bounds_" + std::to_string(sample)));
        MapPipeline pipeline(world, planet_params_from_config(world.config()));

        const MapTilePayload payload = pipeline.get(TileCoord{0, 0, 0});
        const TileSummary    summary = summarize_tile(payload);
        const auto           hist    = feature_histogram(payload.features);
        const int            total   = summary.land_cells + summary.ocean_cells;
        const double         land_ratio = total > 0 ? 100.0 * summary.land_cells / total : 0.0;

        // PlanetGenerator.cpp's own M054 comment: a 0.35 target coverage,
        // with overlap + coastline noise reducing actual land to roughly
        // 20-35% -- margin on both sides here so this doesn't flake on a
        // legitimately unlucky-but-valid draw, while still catching a
        // generator regression that produces an all-land or all-ocean world.
        EXPECT_GT(land_ratio, 5.0);
        EXPECT_LT(land_ratio, 60.0);

        // PlanetGenerator::continent_count() (and planet.lua's port of it)
        // draws entropy % (continents_max - continents_min + 1) +
        // continents_min, so the default WorldConfig (5..12) bounds it
        // exactly -- not just "greater than zero".
        const int continents = hist[static_cast<std::size_t>(FeatureType::Continent)];
        EXPECT_GE(continents, world.config().continents_min);
        EXPECT_LE(continents, world.config().continents_max);

        // Rivers/mountain ranges are terrain-driven (can legitimately be 0
        // on a dry or flat draw) but must never be negative or implausibly
        // large for a single 64x64 level-0 tile.
        EXPECT_GE(hist[static_cast<std::size_t>(FeatureType::River)], 0);
        EXPECT_LE(hist[static_cast<std::size_t>(FeatureType::River)], 200);
        EXPECT_GE(hist[static_cast<std::size_t>(FeatureType::MountainRange)], 0);
        EXPECT_LE(hist[static_cast<std::size_t>(FeatureType::MountainRange)], 20);

        // Settlements/borders/roads are only ever wired in at other levels
        // (MAP9's Countries::grow()/Settlements at level 4, ChildGenerator
        // elsewhere) -- level 0's PlanetGenerator never adds them.
        EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::City)], 0);
        EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::Town)], 0);
        EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::Border)], 0);
        EXPECT_EQ(hist[static_cast<std::size_t>(FeatureType::Road)], 0);
    }
}

// --- Hand-off demo -----------------------------------------------------------

TEST_F(PlanetMapLogicTest, RunHandoffProducesValidatorCleanMc3) {
    auto world = PlanetWorld::create_new(tmp_dir("handoff"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const HandoffResult result = run_handoff(TileCoord{5, 3, 2}, pipeline);

    EXPECT_TRUE(result.ctx.map_context.available);
    EXPECT_FALSE(result.mc3_xml.empty());

    MC3Validator validator;
    const auto   vr = validator.validate(result.mc3_xml, result.ctx.chunk_size_m);
    EXPECT_TRUE(vr.ok) << (vr.errors.empty() ? "" : vr.errors.front());
}

TEST_F(PlanetMapLogicTest, RunHandoffDeterministicForSameTile) {
    auto world = PlanetWorld::create_new(tmp_dir("handoff_repeat"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const HandoffResult a = run_handoff(TileCoord{4, 1, 1}, pipeline);
    const HandoffResult b = run_handoff(TileCoord{4, 1, 1}, pipeline);

    EXPECT_EQ(a.chunk, b.chunk);
    EXPECT_EQ(a.ctx.zone, b.ctx.zone);
    EXPECT_FLOAT_EQ(a.ctx.map_context.elevation_m, b.ctx.map_context.elevation_m);
    EXPECT_EQ(a.mc3_xml, b.mc3_xml);
}

// --- MAP14 M215: level-0 generation wall-clock budget -------------------------
//
// plan.md's MAP0 green-light gate (already passed 2026-07-01, see plan.md's
// "GREEN-LIGHT GATE" block) sets the project's own rough performance bar: a
// *full planet-to-chunk descent* should be "not painfully slow ... e.g. < a
// few seconds". Generating a single level-0 map is a small fraction of that
// descent, so this uses a much tighter budget -- still with wide margin over
// what's actually observed on this dev machine (single-digit milliseconds),
// so it flags a real regression without flaking on a slower CI machine.
TEST_F(PlanetMapLogicTest, GenerateLevel0MapStaysWithinWallClockBudget) {
    constexpr auto kBudget = std::chrono::milliseconds(2000);

    auto world = PlanetWorld::create_new(tmp_dir("bench_level0"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const auto start   = std::chrono::steady_clock::now();
    const MapTilePayload payload = pipeline.get(TileCoord{0, 0, 0});
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ASSERT_FALSE(payload.elevation.empty());
    EXPECT_LT(elapsed, kBudget)
        << "level-0 generation took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
        << "ms, budget is " << kBudget.count() << "ms";
}

// --- MAP14 M216: planet->chunk descent benchmark (cold vs cached) -------------
//
// run_handoff() always resolves its hand-off tile at Map::MAX_LEVEL from the
// chunk's world position (see populate_map_context() in ChunkPipeline.cpp),
// regardless of the TileCoord level passed in -- so a single run_handoff()
// call already exercises the full planet->chunk descent (level 0 down to
// MAX_LEVEL=18) this benchmark is meant to measure. Calling it twice for the
// same location against the same (shared) MapPipeline/PlanetWorld measures
// cold (nothing persisted yet) vs cached (every level along that descent
// already generated and stored) -- same budget-with-margin approach as M215;
// see that test's comment for why the numbers aren't tied to a documented
// project-wide figure (none exists more specific than the MAP0 green-light
// gate's "< a few seconds" for a full descent).
TEST_F(PlanetMapLogicTest, DescendPlanetToChunkColdVsCachedStaysWithinBudget) {
    constexpr auto kColdBudget   = std::chrono::milliseconds(2000);
    constexpr auto kCachedBudget = std::chrono::milliseconds(500);

    auto world = PlanetWorld::create_new(tmp_dir("bench_descent"));
    MapPipeline pipeline(world, planet_params_from_config(world.config()));

    const TileCoord tile{14, 1234, 5678};

    const auto cold_start   = std::chrono::steady_clock::now();
    const HandoffResult cold = run_handoff(tile, pipeline);
    const auto cold_elapsed = std::chrono::steady_clock::now() - cold_start;

    const auto cached_start   = std::chrono::steady_clock::now();
    const HandoffResult cached = run_handoff(tile, pipeline);
    const auto cached_elapsed = std::chrono::steady_clock::now() - cached_start;

    // Not a redundant check against RunHandoffDeterministicForSameTile above:
    // that test asserts determinism is possible, this one asserts it holds
    // for the exact two calls this benchmark timed.
    EXPECT_EQ(cold.chunk, cached.chunk);
    EXPECT_EQ(cold.mc3_xml, cached.mc3_xml);

    EXPECT_LT(cold_elapsed, kColdBudget)
        << "cold descent took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(cold_elapsed).count()
        << "ms, budget is " << kColdBudget.count() << "ms";
    EXPECT_LT(cached_elapsed, kCachedBudget)
        << "cached descent took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(cached_elapsed).count()
        << "ms, budget is " << kCachedBudget.count() << "ms";
    EXPECT_LE(cached_elapsed, cold_elapsed)
        << "a fully-persisted descent should never be slower than the cold generation that persisted it";
}
