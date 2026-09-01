// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP4 tests. M051-M053: PlanetGenerator continent seeding. M067: count in
// range. M070 (partial): determinism given entropy.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "Map/MapPayloadCodec.hpp"
#include "ZoneType.hpp"
#include "generators/map/PlanetGenerator.hpp"

using namespace MeshWorld::Map;

// M236-M275 (MAP16, 2026-07-10): classify() now produces 6 distinct
// underwater outcomes (see ZoneType.hpp's is_ocean_family()), not always
// plain ZoneType::ocean -- wrap it here instead of comparing a single
// hardcoded ordinal.
static bool is_ocean_ordinal(std::uint8_t v) {
    return MeshWorld::is_ocean_family(static_cast<MeshWorld::ZoneType>(v));
}

namespace {

PlanetParams params(int cmin, int cmax, double size = 1000.0) {
    PlanetParams p;
    p.planet_size_m  = size;
    p.continents_min = cmin;
    p.continents_max = cmax;
    return p;
}

// M132 — a payload's features are no longer only continents: Hydrology/
// MountainRanges-derived River/Lake/MountainRange features live alongside
// them now. Tests that care specifically about continent placement must
// filter to FeatureType::Continent rather than assuming every feature is one.
std::vector<MapFeature> continent_features(const MapTilePayload& p) {
    std::vector<MapFeature> out;
    for (const auto& f : p.features)
        if (f.type == FeatureType::Continent) out.push_back(f);
    return out;
}

} // namespace

// M067 — the continent count always lies within the configured range.
TEST(PlanetGeneratorTest, ContinentCountWithinConfiguredRange) {
    PlanetGenerator gen(params(5, 12));
    for (std::uint64_t e = 0; e < 2000; ++e) {
        const int n = gen.continent_count(e);
        EXPECT_GE(n, 5);
        EXPECT_LE(n, 12);
    }
}

// min == max pins the count exactly.
TEST(PlanetGeneratorTest, FixedCountWhenMinEqualsMax) {
    PlanetGenerator gen(params(9, 9));
    for (std::uint64_t e = 0; e < 50; ++e)
        EXPECT_EQ(gen.continent_count(e), 9);
}

// The whole configured range is reachable across entropies (not stuck on one value).
TEST(PlanetGeneratorTest, ContinentCountSpansTheRange) {
    PlanetGenerator gen(params(4, 8));
    int seen_min = 99, seen_max = -1;
    for (std::uint64_t e = 0; e < 500; ++e) {
        const int n = gen.continent_count(e);
        seen_min = std::min(seen_min, n);
        seen_max = std::max(seen_max, n);
    }
    EXPECT_EQ(seen_min, 4);
    EXPECT_EQ(seen_max, 8);
}

// The payload carries exactly `continent_count` Continent features, all inside
// the planet square, with the tile/entropy/generator metadata set.
TEST(PlanetGeneratorTest, GeneratesContinentFeaturesInBounds) {
    PlanetGenerator gen(params(5, 12, 1000.0));
    const TileCoord root{0, 0, 0};
    const MapTilePayload p = gen.generate(root, nullptr, 42);

    EXPECT_EQ(p.tile, root);
    EXPECT_EQ(p.entropy, 42u);
    EXPECT_EQ(p.generator, "planet");
    const std::vector<MapFeature> continents = continent_features(p);
    EXPECT_EQ(continents.size(), static_cast<std::size_t>(gen.continent_count(42)));

    for (const auto& f : continents) {
        ASSERT_EQ(f.points.size(), 1u);
        EXPECT_GE(f.points[0][0], 0.0);
        EXPECT_LT(f.points[0][0], 1000.0);
        EXPECT_GE(f.points[0][1], 0.0);
        EXPECT_LT(f.points[0][1], 1000.0);
    }
}

// M070 (partial) — same entropy reproduces identical continents.
TEST(PlanetGeneratorTest, DeterministicForSameEntropy) {
    PlanetGenerator gen(params(5, 12, 22585000.0));
    const MapTilePayload a = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    const MapTilePayload b = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);

    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].points[0][0], b.features[i].points[0][0]);
        EXPECT_EQ(a.features[i].points[0][1], b.features[i].points[0][1]);
    }
}

// M071 (partial) — different entropy yields materially different continents.
TEST(PlanetGeneratorTest, DifferentEntropyDiffersMaterially) {
    PlanetGenerator gen(params(7, 7, 22585000.0));  // pin the count to isolate position
    const MapTilePayload a = gen.generate(TileCoord{0, 0, 0}, nullptr, 1);
    const MapTilePayload b = gen.generate(TileCoord{0, 0, 0}, nullptr, 2);

    const std::vector<MapFeature> ca = continent_features(a);
    const std::vector<MapFeature> cb = continent_features(b);
    ASSERT_EQ(ca.size(), 7u);
    ASSERT_EQ(cb.size(), 7u);
    int diff = 0;
    for (std::size_t i = 0; i < ca.size(); ++i)
        if (ca[i].points[0][0] != cb[i].points[0][0]) ++diff;
    EXPECT_GT(diff, 0);
}

// M059 — temperature field has the right dimensions and is within physical range.
TEST(PlanetGeneratorTest, TemperatureFieldDimensionsAndRange) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 42);
    ASSERT_FALSE(payload.temperature.empty());
    EXPECT_EQ(payload.temperature.w, payload.elevation.w);
    EXPECT_EQ(payload.temperature.h, payload.elevation.h);

    // Ocean cells have no lapse-rate correction, so their temperature equals
    // base_temp exactly — bounded by [pole_temp_c, equator_temp_c].
    // High land cells can be colder than pole_temp_c (lapse rate), but not
    // warmer than equator_temp_c (lapse rate only cools).
    for (auto t : payload.temperature.data)
        EXPECT_LE(t, static_cast<float>(p.equator_temp_c) + 0.01f);
}

// M059 — equatorial rows are warmer on average than polar rows.
TEST(PlanetGeneratorTest, TemperatureDecreasesPoleward) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 99);
    const int W = payload.temperature.w;
    const int H = payload.temperature.h;

    // Average temperature of centre 4 rows vs. outer 4 rows.
    auto avg_rows = [&](int y0, int y1) {
        double sum = 0.0;
        int    cnt = 0;
        for (int gy = y0; gy < y1; ++gy)
            for (int gx = 0; gx < W; ++gx, ++cnt)
                sum += payload.temperature.at(gx, gy);
        return sum / cnt;
    };
    const int mid = H / 2;
    const double avg_equator = avg_rows(mid - 2, mid + 2);
    const double avg_pole    = (avg_rows(0, 4) + avg_rows(H - 4, H)) * 0.5;
    EXPECT_GT(avg_equator, avg_pole) << "equatorial rows should be warmer than polar rows";
}

// M059 — temperature field is deterministic.
TEST(PlanetGeneratorTest, TemperatureFieldDeterministic) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.equator_temp_c = 35.0;
    p.pole_temp_c    = -15.0;
    PlanetGenerator gen(p);

    const MapTilePayload a = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    const MapTilePayload b = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    ASSERT_EQ(a.temperature.data.size(), b.temperature.data.size());
    for (std::size_t i = 0; i < a.temperature.data.size(); ++i)
        EXPECT_EQ(a.temperature.data[i], b.temperature.data[i]) << "index=" << i;
}

// M060 — moisture field has the right dimensions, is within [0,1], and varies
// across cells (not a constant placeholder).
TEST(PlanetGeneratorTest, MoistureFieldDimensionsAndRange) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 42);
    ASSERT_FALSE(payload.moisture.empty());
    EXPECT_EQ(payload.moisture.w, payload.elevation.w);
    EXPECT_EQ(payload.moisture.h, payload.elevation.h);

    bool all_same = true;
    const float first = payload.moisture.data.front();
    for (auto m : payload.moisture.data) {
        EXPECT_GE(m, 0.0f);
        EXPECT_LE(m, 1.0f);
        if (m != first) all_same = false;
    }
    EXPECT_FALSE(all_same) << "moisture should vary across the grid, not be a flat placeholder";
}

// M060 — moisture field is deterministic for the same entropy.
TEST(PlanetGeneratorTest, MoistureFieldDeterministic) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    PlanetGenerator gen(p);

    const MapTilePayload a = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    const MapTilePayload b = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    ASSERT_EQ(a.moisture.data.size(), b.moisture.data.size());
    for (std::size_t i = 0; i < a.moisture.data.size(); ++i)
        EXPECT_EQ(a.moisture.data[i], b.moisture.data[i]) << "index=" << i;
}

// M068 — land/ocean ratio within sane bounds (20–45 % land) across several entropies.
TEST(PlanetGeneratorTest, LandOceanRatioWithinSaneBounds) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    PlanetGenerator gen(p);

    const TileCoord root{0, 0, 0};
    for (std::uint64_t e : {100ULL, 999ULL, 42000ULL, 0xdeadbeefULL, 777777ULL}) {
        const MapTilePayload payload = gen.generate(root, nullptr, e);
        ASSERT_FALSE(payload.biome.empty()) << "entropy=" << e;
        int land  = 0;
        int total = payload.biome.w * payload.biome.h;
        for (auto v : payload.biome.data)
            if (!is_ocean_ordinal(v)) ++land;
        const double ratio = static_cast<double>(land) / static_cast<double>(total);
        EXPECT_GE(ratio, 0.20) << "entropy=" << e << " land_ratio=" << ratio;
        EXPECT_LE(ratio, 0.45) << "entropy=" << e << " land_ratio=" << ratio;
    }
}

// M057 — elevation field is set, land cells are above sea level, ocean cells below.
TEST(PlanetGeneratorTest, ElevationFieldConsistentWithLandMask) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 12345);
    ASSERT_FALSE(payload.elevation.empty());
    ASSERT_EQ(payload.elevation.w, payload.biome.w);
    ASSERT_EQ(payload.elevation.h, payload.biome.h);

    const int w = payload.elevation.w;
    const int h = payload.elevation.h;
    for (int gy = 0; gy < h; ++gy) {
        for (int gx = 0; gx < w; ++gx) {
            const float   elev   = payload.elevation.at(gx, gy);
            const bool is_ocean = is_ocean_ordinal(payload.biome.at(gx, gy));
            if (!is_ocean)
                EXPECT_GT(elev, 0.0f) << "land cell (" << gx << "," << gy << ") below sea level";
            else
                EXPECT_LT(elev, 0.0f) << "ocean cell (" << gx << "," << gy << ") above sea level";
        }
    }
}

// M061 — biome grid uses real ZoneType ordinals (not placeholder 0/1).
// Every non-ocean land cell must be a valid zone; ocean cells must match elevation.
TEST(PlanetGeneratorTest, BiomeGridUsesZoneTypeOrdinals) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 777);
    ASSERT_FALSE(payload.biome.empty());

    const std::uint8_t max_valid =
        static_cast<std::uint8_t>(MeshWorld::ZoneType::empty);

    bool saw_non_ocean = false;
    bool saw_ocean     = false;
    for (auto v : payload.biome.data) {
        EXPECT_LE(v, max_valid) << "biome ordinal out of ZoneType range";
        if (is_ocean_ordinal(v)) saw_ocean = true;
        else                     saw_non_ocean = true;
    }
    EXPECT_TRUE(saw_ocean)     << "expected at least one ocean cell";
    EXPECT_TRUE(saw_non_ocean) << "expected at least one non-ocean cell";
}

// M066 — edge descriptors are populated with GRID_SIZE samples matching the elevation grid.
TEST(PlanetGeneratorTest, EdgeDescriptorsMatchElevationBoundary) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const MapTilePayload payload = gen.generate(TileCoord{0, 0, 0}, nullptr, 55555);
    const int W = payload.elevation.w;
    const int H = payload.elevation.h;

    // All four edges must have exactly W (= GRID_SIZE) samples.
    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), W);  // N
    ASSERT_EQ(static_cast<int>(payload.edges[1].elevation.size()), H);  // E
    ASSERT_EQ(static_cast<int>(payload.edges[2].elevation.size()), W);  // S
    ASSERT_EQ(static_cast<int>(payload.edges[3].elevation.size()), H);  // W

    // N edge = top row (gy=0), W edge = left col (gx=0), etc.
    for (int i = 0; i < W; ++i) {
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0))     << "N edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[2].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, H - 1)) << "S edge mismatch at i=" << i;
    }
    for (int i = 0; i < H; ++i) {
        EXPECT_EQ(payload.edges[1].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(W - 1, i)) << "E edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[3].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(0, i))     << "W edge mismatch at i=" << i;
    }
}

// M070 — same entropy → byte-identical encoded payload (full end-to-end determinism).
TEST(PlanetGeneratorTest, EncodedPayloadByteIdenticalForSameEntropy) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const TileCoord root{0, 0, 0};
    for (std::uint64_t e : {1ULL, 42ULL, 0xdeadbeefULL}) {
        const std::string json_a = MapPayloadCodec::encode(gen.generate(root, nullptr, e));
        const std::string json_b = MapPayloadCodec::encode(gen.generate(root, nullptr, e));
        EXPECT_EQ(json_a, json_b) << "non-deterministic output for entropy=" << e;
    }
}

// M070 — different entropy produces different encoded payloads.
TEST(PlanetGeneratorTest, EncodedPayloadDiffersForDifferentEntropy) {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 7;
    p.continents_max = 7;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    PlanetGenerator gen(p);

    const TileCoord root{0, 0, 0};
    const std::string json_1 = MapPayloadCodec::encode(gen.generate(root, nullptr, 1));
    const std::string json_2 = MapPayloadCodec::encode(gen.generate(root, nullptr, 2));
    EXPECT_NE(json_1, json_2);
}
