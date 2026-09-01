// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP9 wiring — ChildGenerator (the C++ fallback for any tile level with no
// registered Lua generator, e.g. map.md's level 6 "road/rail trunk network
// between cities") now places a sparse, tile-local set of settlements
// (Settlements::place()/name()/appendLabels(), MAP9 M138/M148) and connects
// them with a real routed road network (Roads::build(), M143/M144) —
// algorithms that were tested standalone since MAP9 but never called from
// any real generator until now (see NEXT.md §5 #4/old §8 task).

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "Map/ChildGenerator.hpp"
#include "Map/MapTilePayload.hpp"
#include "generators/map/PlanetGenerator.hpp"

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

// Uniform 1000 m everywhere: comfortably land (>0), below the mountain
// threshold, outside the lake band — every settlement site-find attempt
// succeeds, mirroring CityLuaTests.cpp's own make_moderate_land_parent()
// fixture (same reasoning, different generator).
MapTilePayload make_moderate_land_parent() {
    MapTilePayload p;
    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.culture = "nordic";
    return p;
}

bool has_feature(const MapTilePayload& payload, FeatureType type) {
    for (const auto& f : payload.features)
        if (f.type == type) return true;
    return false;
}

// ChildGenerator::generate() derives its own settlement counts from
// entropy>>16 (city, mod 2) and entropy>>20 (town, mod 3). This value sets
// both bits, so city_count=1, town_count=1 — exactly 2 settlements, enough
// for Roads::build() to connect them with one edge.
constexpr std::uint64_t kTwoSettlementEntropy = 1114112ULL;

} // namespace

TEST(ChildGeneratorSettlementsTest, PlacesSettlementLabelsOverModerateLand) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{6, 0, 0}, &parent, kTwoSettlementEntropy);

    ASSERT_EQ(child.labels.size(), 2u);
    for (const auto& label : child.labels) {
        EXPECT_FALSE(label.name.empty());
        EXPECT_TRUE(label.kind == "city" || label.kind == "town") << label.kind;
    }
}

TEST(ChildGeneratorSettlementsTest, ConnectsSettlementsWithARoadFeature) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{6, 0, 0}, &parent, kTwoSettlementEntropy);

    ASSERT_TRUE(has_feature(child, FeatureType::Road));
    for (const auto& f : child.features) {
        if (f.type != FeatureType::Road) continue;
        EXPECT_FALSE(f.name.empty());
        EXPECT_GE(f.points.size(), 2u);
    }
}

TEST(ChildGeneratorSettlementsTest, NoSettlementsRequestedProducesNoRoadOrLabel) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    // entropy=0: (0>>16)%2 == 0 and (0>>20)%3 == 0 — zero settlements
    // requested, Settlements::place() returns an empty network, and
    // Roads::build() must not fabricate a road out of nothing.
    const MapTilePayload child = gen.generate(TileCoord{6, 0, 0}, &parent, 0ULL);

    EXPECT_TRUE(child.labels.empty());
    EXPECT_FALSE(has_feature(child, FeatureType::Road));
}

TEST(ChildGeneratorSettlementsTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    const MapTilePayload a = gen.generate(TileCoord{6, 0, 0}, &parent, kTwoSettlementEntropy);
    const MapTilePayload b = gen.generate(TileCoord{6, 0, 0}, &parent, kTwoSettlementEntropy);

    ASSERT_EQ(a.labels.size(), b.labels.size());
    for (std::size_t i = 0; i < a.labels.size(); ++i) {
        EXPECT_EQ(a.labels[i].name, b.labels[i].name);
        EXPECT_EQ(a.labels[i].pos, b.labels[i].pos);
    }
    ASSERT_EQ(a.features.size(), b.features.size());
}

// Countries::grow()/name() wiring (this session's follow-up to the
// Settlements/Roads wiring above) — real multi-capital contested-border
// growth, wired in only at level 4 ("kCountryRegionLevel" in
// ChildGenerator.cpp — map.md's own level table leaves this level with no
// generator at all, Lua or C++, so it's both semantically reasonable and
// guaranteed to actually run in real usage). entropy=0 already requests
// the minimum 2 capitals at this level (unconditional +2, only the extra
// 0-2 is entropy-driven), so no special entropy search is needed here.
TEST(ChildGeneratorSettlementsTest, PlacesCapitalsAndBordersAtTheCountryRegionLevel) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    const MapTilePayload child = gen.generate(TileCoord{4, 0, 0}, &parent, 0ULL);

    int capital_labels = 0;
    for (const auto& label : child.labels)
        if (label.kind == "capital") ++capital_labels;
    EXPECT_GE(capital_labels, 2) << "expected at least 2 capitals over moderate land at level 4";

    ASSERT_TRUE(has_feature(child, FeatureType::Border));
    for (const auto& f : child.features) {
        if (f.type != FeatureType::Border) continue;
        EXPECT_FALSE(f.name.empty());
        ASSERT_GE(f.points.size(), 4u);
        EXPECT_EQ(f.points.front()[0], f.points.back()[0]);
        EXPECT_EQ(f.points.front()[1], f.points.back()[1]);  // closed loop
    }
}

TEST(ChildGeneratorSettlementsTest, NoCapitalsOrBordersOutsideTheCountryRegionLevel) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    // Same entropy as the level-4 test above, but at level 6 -- capitals
    // stay at 0 everywhere except kCountryRegionLevel, so this must produce
    // neither a capital label nor a Border feature even though the same
    // moderate-land conditions would happily place capitals at level 4.
    const MapTilePayload child = gen.generate(TileCoord{6, 0, 0}, &parent, 0ULL);

    for (const auto& label : child.labels) EXPECT_NE(label.kind, "capital");
    EXPECT_FALSE(has_feature(child, FeatureType::Border));
}

TEST(ChildGeneratorSettlementsTest, CountryRegionLevelIsDeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent();
    const ChildGenerator gen(make_params());

    const MapTilePayload a = gen.generate(TileCoord{4, 0, 0}, &parent, 0ULL);
    const MapTilePayload b = gen.generate(TileCoord{4, 0, 0}, &parent, 0ULL);

    ASSERT_EQ(a.features.size(), b.features.size());
    ASSERT_EQ(a.labels.size(), b.labels.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].type, b.features[i].type);
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].points, b.features[i].points);
    }
}
