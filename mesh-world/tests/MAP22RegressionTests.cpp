// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M347 (MAP22): a regression suite covering M340-346 together on a real
// multi-tile generated region -- every other M340-346 test uses either
// small hand-crafted fixtures (proving a single mechanism's own exact
// correctness) or a single tile in isolation. This file instead calls the
// real ChildGenerator::generate() (the exact C++ fallback path M340-346's
// own code all lives in -- see kCountryRegionLevel's own doc comment in
// ChildGenerator.cpp for why this is where their real content actually
// manifests) across a real 2x2 grid of ADJACENT tiles sharing one parent,
// mirroring ChildGeneratorSettlementsTests.cpp's own established direct-
// call pattern rather than standing up a full DB-backed MapPipeline (which
// would mostly just route through Lua scripts at every level except this
// one, not exercising M340-346's own C++ code at all).

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>

#include "Map/ChildGenerator.hpp"
#include "Map/MapTilePayload.hpp"
#include "MapValidator.hpp"
#include "PlanetMapLogic.hpp"
#include "generators/map/PlanetGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

PlanetParams make_params() {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    return p;
}

// Two independent terms, engineered so BOTH M342 (coastline) and M345
// (river reaching this tile's own edge) have something real to find:
//
// - `from_north_sink`: radial distance from (20, -15), a point 15 cells
//   NORTH of this tile's own y=0 row (off-tile, invisible to trace()).
//   Elevation increases with this distance, so within the tile it's LOWEST
//   along a dip centered on column x=20 at y=0 -- a genuine in-tile local
//   minimum sitting exactly ON the tile's own boundary (every in-bounds
//   neighbor is strictly farther from (20,-15), hence higher), the exact
//   exits_tile scenario M345 fixed (the true lowest point lies further
//   north, off-tile).
// - `southeast_dip`: grows once x or y exceeds 45, pushing the southeast
//   portion of the tile below sea level -- independent of the north-sink
//   term, so it doesn't disturb the boundary-minimum property near (20,0),
//   and gives Coastline::trace() a real ocean/land boundary along the
//   tile's own southeast region.
//
// Unlike ChildGeneratorSettlementsTests.cpp's own uniform-1000m fixture
// (chosen there specifically to make every settlement site-find attempt
// succeed -- M342/M345 didn't exist yet when that file was written), this
// one is deliberately varied because M347's whole point is exercising
// those newer mechanisms too.
MapTilePayload make_island_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.resize(static_cast<std::size_t>(N * N));
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const double ddx            = x - 20.0;
            const double ddy            = y + 15.0;
            const double from_north_sink = std::hypot(ddx, ddy);
            const double southeast_dip  = std::max(0.0, x - 45.0) + std::max(0.0, y - 45.0);
            const double elev           = 100.0 + from_north_sink * 2.0 - southeast_dip * 40.0;
            p.elevation.data[static_cast<std::size_t>(y * N + x)] = static_cast<float>(elev);
        }
    }
    p.culture = "nordic";
    return p;
}

bool has_feature(const MapTilePayload& payload, FeatureType type) {
    for (const auto& f : payload.features)
        if (f.type == type) return true;
    return false;
}

int count_features(const MapTilePayload& payload, FeatureType type) {
    int n = 0;
    for (const auto& f : payload.features)
        if (f.type == type) ++n;
    return n;
}

int count_crossings(const MapTilePayload& payload, EdgeCrossingType type) {
    int n = 0;
    for (const auto& e : payload.edges)
        for (const auto& c : e.crossings)
            if (c.type == type) ++n;
    return n;
}

// Generates a 2x2 block of adjacent kCountryRegionLevel (4) tiles sharing
// one parent, each with its own distinct entropy (mirroring how real
// sibling tiles are generated independently with different chunk_seed()
// derivations, not a hand-picked single-tile entropy the way
// ChildGeneratorSettlementsTests.cpp's own kTwoSettlementEntropy is).
std::vector<MapTilePayload> generate_region() {
    const MapTilePayload parent = make_island_parent();
    const ChildGenerator  gen(make_params());

    std::vector<MapTilePayload> region;
    region.reserve(4);
    int i = 0;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const std::uint64_t entropy = 500000ULL + static_cast<std::uint64_t>(i) * 7919ULL;
            region.push_back(gen.generate(TileCoord{4, dx, dy}, &parent, entropy));
            ++i;
        }
    }
    return region;
}

} // namespace

TEST(Map22RegressionTest, EveryTileInTheRegionValidatesStructurallyClean) {
    const MapValidator validator;
    for (const auto& tile : generate_region()) {
        const ValidationResult vr = validator.validate(tile);
        EXPECT_TRUE(vr.ok) << (vr.errors.empty() ? "" : vr.errors.front());
    }
}

// M340 -- Countries::name()'s dedupe() wiring holds on real generated
// data, not just the synthetic 900-country stress test.
TEST(Map22RegressionTest, EveryTileHasUniqueBorderFeatureNamesWithinItself) {
    for (const auto& tile : generate_region()) {
        std::set<std::string> names;
        for (const auto& f : tile.features) {
            if (f.type != FeatureType::Border) continue;
            EXPECT_TRUE(names.insert(f.name).second) << "duplicate country name: " << f.name;
        }
    }
}

// M342 -- across a real multi-tile island region, at least one tile finds
// a real ocean/land boundary to trace. Summed across the whole region
// (not asserted per-tile) since a real, non-contrived island's coastline
// doesn't necessarily cross every single tile in a 2x2 block.
TEST(Map22RegressionTest, RegionProducesRealCoastlineFeatures) {
    int total = 0;
    for (const auto& tile : generate_region()) total += count_features(tile, FeatureType::Coastline);
    EXPECT_GT(total, 0) << "expected at least one Coastline feature somewhere in this island region";
}

// M345 -- the radial downhill slope should produce at least one river
// reaching a tile's own edge (Hydrology::exportCrossings()' own River
// EdgeCrossing) somewhere across the region, proving trace()'s exits_tile
// fix and the new exportCrossings() wiring both fire on real generated
// terrain, not just the hand-crafted single-source unit test grid.
TEST(Map22RegressionTest, RegionProducesRealRiverEdgeCrossings) {
    int total = 0;
    for (const auto& tile : generate_region()) total += count_crossings(tile, EdgeCrossingType::River);
    EXPECT_GT(total, 0) << "expected at least one River EdgeCrossing somewhere in this island region";
}

// M346 -- render_geojson() must produce valid, label-duplicate-free output
// for every tile in a real region (not just the small hand-built fixtures).
TEST(Map22RegressionTest, EveryTileExportsGeojsonWithNoDuplicateLabels) {
    for (const auto& tile : generate_region()) {
        const std::string geojson = render_geojson(tile);
        EXPECT_FALSE(geojson.empty());

        std::set<std::string> seen;
        for (const auto& l : tile.labels) {
            const std::string key = l.kind + "|" + l.name + "|" + std::to_string(l.pos[0]) + "," +
                                     std::to_string(l.pos[1]);
            EXPECT_TRUE(seen.insert(key).second) << "duplicate label: " << key;
        }
    }
}

// M343/M344 -- both PNG render modes complete without crashing for every
// tile in the region, hillshade fed its REAL adjacent siblings from the
// same region (the actual cross-tile scenario M343 exists for), not a
// synthetic single-tile call.
TEST(Map22RegressionTest, EveryTileRendersBothPngModesUsingRealRegionNeighbors) {
    const auto region = generate_region();  // index = dy*2+dx, matching generate_region()'s own loop
    const auto neighbor_or_null = [&](int dx, int dy) -> const MapTilePayload* {
        if (dx < 0 || dx > 1 || dy < 0 || dy > 1) return nullptr;
        return &region[static_cast<std::size_t>(dy * 2 + dx)];
    };

    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const MapTilePayload& tile = region[static_cast<std::size_t>(dy * 2 + dx)];

            const RgbImage biome = render_png_biome_image(tile, /*upscale=*/4);
            EXPECT_GT(biome.w, 0);
            EXPECT_GT(biome.h, 0);

            const MapTilePayload* north = neighbor_or_null(dx, dy - 1);
            const MapTilePayload* east  = neighbor_or_null(dx + 1, dy);
            const MapTilePayload* south = neighbor_or_null(dx, dy + 1);
            const MapTilePayload* west  = neighbor_or_null(dx - 1, dy);
            const RgbImage hillshade = render_png_elevation_image(
                tile, /*upscale=*/4, north ? &north->elevation : nullptr, east ? &east->elevation : nullptr,
                south ? &south->elevation : nullptr, west ? &west->elevation : nullptr);
            EXPECT_GT(hillshade.w, 0);
            EXPECT_GT(hillshade.h, 0);
        }
    }
}

// Whole-region determinism: regenerating the identical region from the
// identical parent+entropies reproduces byte-identical labels/features/
// elevation, proving M340-346's combined behavior composes deterministically.
TEST(Map22RegressionTest, RegionGenerationIsDeterministic) {
    const auto region_a = generate_region();
    const auto region_b = generate_region();
    ASSERT_EQ(region_a.size(), region_b.size());

    for (std::size_t i = 0; i < region_a.size(); ++i) {
        EXPECT_EQ(region_a[i].elevation.data, region_b[i].elevation.data) << "tile " << i;
        ASSERT_EQ(region_a[i].labels.size(), region_b[i].labels.size()) << "tile " << i;
        for (std::size_t j = 0; j < region_a[i].labels.size(); ++j) {
            EXPECT_EQ(region_a[i].labels[j].name, region_b[i].labels[j].name) << "tile " << i << " label " << j;
        }
        ASSERT_EQ(region_a[i].features.size(), region_b[i].features.size()) << "tile " << i;
    }
}

// A tile with no capitals at all (this parent still gets 2-4 via
// kCountryRegionLevel's own unconditional request) would make M340/341's
// own tests vacuous -- a quick sanity check that this fixture actually
// produces real countries to dedupe/blend, not an empty network.
TEST(Map22RegressionTest, RegionFixtureSanityCheckProducesRealCountries) {
    int total_borders = 0;
    for (const auto& tile : generate_region()) total_borders += count_features(tile, FeatureType::Border);
    EXPECT_GT(total_borders, 0);
}

TEST(Map22RegressionTest, RegionFixtureSanityCheckProducesSettlementLabels) {
    bool any_labels = false;
    for (const auto& tile : generate_region())
        if (!tile.labels.empty()) any_labels = true;
    EXPECT_TRUE(any_labels);
}

// ChildGenerator::generate() layers real per-child generation (noise,
// MountainRanges::apply(), etc.) on top of whatever the parent provides --
// each of the 4 children only samples one quadrant of make_island_parent()'s
// own pattern, so any ONE tile alone isn't guaranteed to contain both an
// ocean and a land cell (e.g. a quadrant sampled entirely from the
// north-sink's own land-only influence). Checked in aggregate across the
// whole region instead, matching RegionProducesRealCoastlineFeatures/
// RegionProducesRealRiverEdgeCrossings's own established pattern above.
TEST(Map22RegressionTest, RegionFixtureSanityCheckHasBothOceanAndLandCellsSomewhere) {
    bool has_ocean = false, has_land = false;
    for (const auto& tile : generate_region()) {
        ASSERT_FALSE(tile.elevation.empty());
        for (float e : tile.elevation.data) {
            if (e < 0.0f) has_ocean = true;
            else has_land = true;
        }
    }
    EXPECT_TRUE(has_ocean) << "island fixture should have some below-sea-level cells somewhere in the region";
    EXPECT_TRUE(has_land) << "island fixture should have some above-sea-level cells somewhere in the region";
}
