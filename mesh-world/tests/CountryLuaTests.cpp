// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M146 — structural tests for generators/lua/map/country.lua (level 5),
// mirroring RegionLuaTests.cpp's/ContinentLuaTests.cpp's style (child-tile
// path, parent required).

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "MapValidator.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_country_lua() {
    std::ifstream ifs("generators/lua/map/country.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold even after the child's own ±(<=500 m) detail
// perturbation, regardless of entropy — every site-find attempt succeeds.
MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{4, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 1000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 1000.0f);
    return p;
}

// Uniform -2000 m everywhere: comfortably below sea level even after detail
// perturbation, regardless of entropy — no site-find attempt can succeed.
MapTilePayload make_all_ocean_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{4, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), -2000.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), -2000.0f);
    return p;
}

MapGenContext make_ctx(const MapTilePayload& parent, std::uint64_t entropy, const TileCoord& tile) {
    MapGenContext ctx;
    ctx.tile        = tile;
    ctx.entropy     = entropy;
    ctx.sea_level_m = 0.0;
    ctx.parent      = &parent;
    return ctx;
}

MapTilePayload run_country(const MapTilePayload& parent, std::uint64_t entropy,
                           const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_country_lua(), make_ctx(parent, entropy, tile), error_out);
}

} // namespace

TEST(CountryLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_country_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/country.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_country(parent, 2, TileCoord{5, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level5.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(CountryLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_country(parent, 2, TileCoord{5, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(CountryLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_country(parent, 2, TileCoord{5, 1, 0});

    ASSERT_EQ(static_cast<int>(payload.edges[0].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[1].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[2].elevation.size()), N);
    ASSERT_EQ(static_cast<int>(payload.edges[3].elevation.size()), N);

    for (int i = 0; i < N; ++i) {
        EXPECT_EQ(payload.edges[0].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, 0))     << "N edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[2].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(i, N - 1)) << "S edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[1].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(N - 1, i)) << "E edge mismatch at i=" << i;
        EXPECT_EQ(payload.edges[3].elevation[static_cast<std::size_t>(i)],
                  payload.elevation.at(0, i))     << "W edge mismatch at i=" << i;
    }
}

// M146's genuinely new functionality: a capital + one whole-tile border,
// eventually, across several entropies (capital-site placement is randomized
// like region.lua's own town placement).
TEST(CountryLuaTest, EventuallyPlacesACapitalAndBorderOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_capital = false, found_border = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !(found_capital && found_border); ++entropy) {
        const MapTilePayload payload = run_country(parent, entropy, TileCoord{5, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::City) {
                found_capital = true;
                EXPECT_FALSE(f.name.empty());
                ASSERT_EQ(f.points.size(), 1u);
            }
            if (f.type == FeatureType::Border) {
                found_border = true;
                EXPECT_FALSE(f.name.empty());
                EXPECT_EQ(f.points.size(), 4u);  // whole-tile rectangle (v1 simplification)
            }
        }
    }
    EXPECT_TRUE(found_capital) << "expected at least one capital City across 20 entropies over moderate land";
    EXPECT_TRUE(found_border) << "expected at least one Border across 20 entropies over moderate land";
}

// Regression: addBorder()'s whole-tile-rectangle polygon used to place 3 of
// its 4 corners exactly on the tile's max_x/max_z boundary, which
// MapValidator rejects (tile bounds are half-open [min,max) -- the same bug
// class §5 #18 fixed for city.lua's/neighborhood.lua's street endpoints).
TEST(CountryLuaTest, BorderPolygonPassesMapValidator) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapValidator   validator;
    bool                 found_border = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_border; ++entropy) {
        const MapTilePayload payload = run_country(parent, entropy, TileCoord{5, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type != FeatureType::Border) continue;
            found_border = true;
            const ValidationResult vr = validator.validate(payload);
            EXPECT_TRUE(vr.ok) << (vr.errors.empty() ? std::string("(no error text)") : vr.errors.front());
        }
    }
    ASSERT_TRUE(found_border) << "expected at least one Border across 20 entropies over moderate land";
}

TEST(CountryLuaTest, NeverPlacesACapitalOrBorderOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_country(parent, entropy, TileCoord{5, 0, 0});
        for (const auto& f : payload.features) {
            EXPECT_NE(f.type, FeatureType::City) << "entropy=" << entropy;
            EXPECT_NE(f.type, FeatureType::Border) << "entropy=" << entropy;
            EXPECT_NE(f.type, FeatureType::Road) << "entropy=" << entropy;
        }
    }
}

// Every Road feature emitted must start exactly at the capital's own
// position -- the hub-and-spoke shape this v1 uses instead of a ported
// Roads::build() MST (see country.lua's own header note).
TEST(CountryLuaTest, EveryTrunkRoadStartsAtTheCapital) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_road = false;
    for (std::uint64_t entropy = 1; entropy <= 30 && !found_road; ++entropy) {
        const MapTilePayload payload = run_country(parent, entropy, TileCoord{5, 1, 0});

        double capital_x = 0.0, capital_z = 0.0;
        bool   has_capital = false;
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::City) {
                capital_x   = f.points[0][0];
                capital_z   = f.points[0][1];
                has_capital = true;
                break;
            }
        }
        if (!has_capital) continue;

        for (const auto& f : payload.features) {
            if (f.type == FeatureType::Road) {
                found_road = true;
                ASSERT_EQ(f.points.size(), 2u);
                // LuaRuntime's table_to_points() (used by addRoad's path
                // argument) round-trips each coordinate through a `float`
                // intermediate (tbl_float()), unlike addCity's scalar x/z
                // args which stay `double` throughout -- a pre-existing
                // precision characteristic shared by every path-based Lua
                // map feature (addRiver/addMountainRange/addBorder too),
                // not something specific to addRoad. EXPECT_NEAR with a
                // few-meters tolerance accounts for that float rounding at
                // this planet's multi-million-meter coordinate magnitudes,
                // rather than asserting bit-exact equality across a
                // double/float round-trip boundary.
                EXPECT_NEAR(f.points[0][0], capital_x, 5.0);
                EXPECT_NEAR(f.points[0][1], capital_z, 5.0);
            }
        }
    }
    EXPECT_TRUE(found_road) << "expected at least one trunk road across 30 entropies over moderate land";
}

TEST(CountryLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_country(parent, 999, TileCoord{5, 1, 0});
    const MapTilePayload b = run_country(parent, 999, TileCoord{5, 1, 0});

    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    EXPECT_EQ(a.elevation.data, b.elevation.data);
    EXPECT_EQ(a.temperature.data, b.temperature.data);
    EXPECT_EQ(a.moisture.data, b.moisture.data);
    ASSERT_EQ(a.features.size(), b.features.size());
    for (std::size_t i = 0; i < a.features.size(); ++i) {
        EXPECT_EQ(a.features[i].name, b.features[i].name);
        EXPECT_EQ(a.features[i].type, b.features[i].type);
    }
}

TEST(CountryLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{5, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_country_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
