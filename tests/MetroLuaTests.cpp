// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M147 — structural tests for generators/lua/map/metro.lua (level 9),
// mirroring CountryLuaTests.cpp's/RegionLuaTests.cpp's style (child-tile
// path, parent required).

#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <sstream>

#include "LuaSandbox.hpp"
#include "Map/MapTilePayload.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

constexpr int N = 64;

std::string read_metro_lua() {
    std::ifstream ifs("generators/lua/map/metro.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

// Uniform 1000 m everywhere: comfortably land (>0) and comfortably below the
// 2500 m mountain threshold, AND comfortably outside the 50 m lake band,
// even after the child's own detail perturbation (level 9's detail_amp
// tops out at 440 m, so 1000 +/- 220 never approaches [0, 50]) regardless
// of entropy — every site-find attempt succeeds, no lake is ever detected.
MapTilePayload make_moderate_land_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{8, 0, 0};
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
    p.tile    = TileCoord{8, 0, 0};
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

// Uniform 25 m everywhere: comfortably inside the [0, 50] m lake band near
// every tile edge (where the fade term is ~0, so elevation stays close to
// this base value regardless of detail noise) — reliably produces lake
// points even though the tile's interior can swing further away from it.
MapTilePayload make_near_sea_level_parent(std::uint64_t entropy) {
    MapTilePayload p;
    p.tile    = TileCoord{8, 0, 0};
    p.entropy = entropy;
    p.culture = "nordic";

    p.elevation.w = p.elevation.h = N;
    p.elevation.data.assign(static_cast<std::size_t>(N * N), 25.0f);
    p.temperature.w = p.temperature.h = N;
    p.temperature.data.assign(static_cast<std::size_t>(N * N), 15.0f);
    p.moisture.w = p.moisture.h = N;
    p.moisture.data.assign(static_cast<std::size_t>(N * N), 0.4f);
    p.biome.w = p.biome.h = N;
    p.biome.data.assign(static_cast<std::size_t>(N * N), 0);
    for (auto& e : p.edges) e.elevation.assign(static_cast<std::size_t>(N), 25.0f);
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

MapTilePayload run_metro(const MapTilePayload& parent, std::uint64_t entropy,
                         const TileCoord& tile, std::string* error_out = nullptr) {
    LuaSandbox sandbox;
    return sandbox.executeMap(read_metro_lua(), make_ctx(parent, entropy, tile), error_out);
}

} // namespace

TEST(MetroLuaTest, FileReadsAndRunsWithoutError) {
    const std::string source = read_metro_lua();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/metro.lua not found or empty";

    const MapTilePayload parent = make_moderate_land_parent(1);
    std::string error;
    const MapTilePayload payload = run_metro(parent, 2, TileCoord{9, 1, 0}, &error);

    EXPECT_TRUE(error.empty()) << error;
    EXPECT_FALSE(payload.elevation.empty());
    EXPECT_EQ(payload.generator, "lua.map.child.level9.default");
    EXPECT_EQ(payload.culture, "nordic");
}

TEST(MetroLuaTest, FieldShapesMatch64x64) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_metro(parent, 2, TileCoord{9, 1, 0});

    EXPECT_EQ(payload.elevation.w, N);
    EXPECT_EQ(payload.elevation.h, N);
    EXPECT_EQ(payload.temperature.w, N);
    EXPECT_EQ(payload.temperature.h, N);
    EXPECT_EQ(payload.moisture.w, N);
    EXPECT_EQ(payload.moisture.h, N);
    EXPECT_EQ(payload.biome.w, N);
    EXPECT_EQ(payload.biome.h, N);
}

TEST(MetroLuaTest, EdgeDescriptorsMatchElevationBoundary) {
    const MapTilePayload parent  = make_moderate_land_parent(1);
    const MapTilePayload payload = run_metro(parent, 2, TileCoord{9, 1, 0});

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

TEST(MetroLuaTest, EventuallyPlacesACityOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_city = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_city; ++entropy) {
        const MapTilePayload payload = run_metro(parent, entropy, TileCoord{9, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::City) {
                found_city = true;
                EXPECT_FALSE(f.name.empty());
                ASSERT_EQ(f.points.size(), 1u);
                break;
            }
        }
    }
    EXPECT_TRUE(found_city) << "expected at least one metro City across 20 entropies over moderate land";
}

TEST(MetroLuaTest, EventuallyPlacesASuburbOverModerateLandAcrossSeveralEntropies) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    bool found_suburb = false;
    for (std::uint64_t entropy = 1; entropy <= 20 && !found_suburb; ++entropy) {
        const MapTilePayload payload = run_metro(parent, entropy, TileCoord{9, 1, 0});
        for (const auto& f : payload.features) {
            if (f.type == FeatureType::Town) {
                found_suburb = true;
                EXPECT_FALSE(f.name.empty());
                ASSERT_EQ(f.points.size(), 1u);
                break;
            }
        }
    }
    EXPECT_TRUE(found_suburb) << "expected at least one suburb Town across 20 entropies over moderate land";
}

// MAP19, M317: also covers Lake -- Map::Hydrology::trace() only ever seeds
// sources from LAND cells (see its own doc comment), so an all-ocean tile
// can never produce a river/lake either, same reasoning as city/suburb.
TEST(MetroLuaTest, NeverPlacesACityOrSuburbOrLakeOverOcean) {
    const MapTilePayload parent = make_all_ocean_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_metro(parent, entropy, TileCoord{9, 0, 0});
        for (const auto& f : payload.features) {
            EXPECT_NE(f.type, FeatureType::City) << "entropy=" << entropy;
            EXPECT_NE(f.type, FeatureType::Town) << "entropy=" << entropy;
            EXPECT_NE(f.type, FeatureType::Lake) << "entropy=" << entropy;
        }
    }
}

TEST(MetroLuaTest, PlacesALakeOverNearSeaLevelTerrain) {
    const MapTilePayload parent  = make_near_sea_level_parent(1);
    const MapTilePayload payload = run_metro(parent, 2, TileCoord{9, 1, 0});

    bool found_lake = false;
    for (const auto& f : payload.features) {
        if (f.type == FeatureType::Lake) {
            found_lake = true;
            EXPECT_FALSE(f.name.empty());
            EXPECT_FALSE(f.points.empty());
        }
    }
    EXPECT_TRUE(found_lake) << "expected a Lake over uniformly near-sea-level terrain";
}

// MAP19, M317: real Map::Hydrology basin-filling (M123) replaced the prior
// crude near-sea-level threshold proxy metro.lua used to build a Lake from
// -- see metro.lua's own header note. That old proxy could only ever
// trigger near sea level, so "never over moderate (1000 m) land" held; the
// real algorithm floods ANY landlocked local minimum regardless of absolute
// elevation (an endorheic basin at 1000 m is exactly as real as one at
// 10 m), so a Lake CAN now legitimately appear over moderate land wherever
// the detail noise happens to carve one in -- this is no longer a bug to
// guard against, it is the new algorithm doing its job. Replaced with the
// invariant that DOES still hold: any Lake that IS produced must be a
// well-formed, named feature (mirrors PlacesALakeOverNearSeaLevelTerrain's
// own per-feature checks above).
TEST(MetroLuaTest, AnyLakeOverModerateLandIsWellFormed) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    for (std::uint64_t entropy = 1; entropy <= 10; ++entropy) {
        const MapTilePayload payload = run_metro(parent, entropy, TileCoord{9, 0, 0});
        for (const auto& f : payload.features) {
            if (f.type != FeatureType::Lake) continue;
            EXPECT_FALSE(f.name.empty()) << "entropy=" << entropy;
            EXPECT_FALSE(f.points.empty()) << "entropy=" << entropy;
        }
    }
}

TEST(MetroLuaTest, DeterministicForSameEntropy) {
    const MapTilePayload parent = make_moderate_land_parent(1);
    const MapTilePayload a = run_metro(parent, 999, TileCoord{9, 1, 0});
    const MapTilePayload b = run_metro(parent, 999, TileCoord{9, 1, 0});

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

TEST(MetroLuaTest, MissingParentReturnsEmptyPayloadNotCrash) {
    MapGenContext ctx;
    ctx.tile        = TileCoord{9, 0, 0};
    ctx.entropy     = 1;
    ctx.sea_level_m = 0.0;
    ctx.parent      = nullptr;

    LuaSandbox sandbox;
    std::string error;
    const MapTilePayload payload = sandbox.executeMap(read_metro_lua(), ctx, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(payload.elevation.empty());
}
