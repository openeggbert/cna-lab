// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M157/M159/M163/M166 — ChunkPipeline populates ChunkContext.map_context from
// MapPipeline at the hand-off level and prefers the map layer's biome
// (zone) and ZoneCandidate (region, M156) over the flat WorldMap values
// once available, without disturbing the flat-WorldMap path used when no
// MapPipeline is attached or no map-derived value was actually assigned.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "ChunkPipeline.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "Map/MapPipeline.hpp"
#include "Map/PlanetConstants.hpp"
#include "Map/TileCoord.hpp"
#include "Map/ZoneCandidate.hpp"
#include "PlanetWorld.hpp"
#include "RegionType.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ZoneType.hpp"
#include "generators/map/PlanetGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

std::string tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_chunkmap_test_" + suffix)).string();
}

// M097's own convention (see MapPipelineTests.cpp): MapPipeline looks up Lua
// map generators through the process-wide LuaGeneratorRegistry::instance()
// singleton, which nothing populates automatically in a test binary (real
// apps populate it via ContentPackLoader at startup -- see NEXT.md §2's
// "Lua map generators are never loaded" finding, discovered while verifying
// this exact test). This guard registers a source for the test's own scope
// and always unregisters it on destruction.
class ScopedLuaMapGenerator {
public:
    ScopedLuaMapGenerator(std::string id, const std::string& source) : id_(std::move(id)) {
        LuaGeneratorRegistry::instance().register_source(id_, source);
    }
    ~ScopedLuaMapGenerator() { LuaGeneratorRegistry::instance().unregister_source(id_); }

private:
    std::string id_;
};

// M163-M166 — registers `source_template` (containing the literal marker
// "PLACEHOLDER_ID" in place of M.id) for the given quadtree `level`,
// substituting the real id in first. Factored out once this pattern hit
// its 3rd call site (M159's own IsCityAndNearestRiverPopulated... test
// introduced it, hardcoded to MAX_LEVEL); parameterized on level since
// M164 needs BOTH the hand-off level (MAX_LEVEL, for biome/zone) AND
// kStreetCrossingLevel's own level 15 (for road crossings) registered
// simultaneously.
ScopedLuaMapGenerator register_level_generator(int level, const std::string& source_template) {
    const std::string lua_id  = "lua.map.child.level" + std::to_string(level) + ".default";
    std::string        source = source_template;
    const std::string  marker = "PLACEHOLDER_ID";
    const auto          pos   = source.find(marker);
    source.replace(pos, marker.size(), lua_id);
    return ScopedLuaMapGenerator(lua_id, source);
}

ScopedLuaMapGenerator register_handoff_level_generator(const std::string& source_template) {
    return register_level_generator(MAX_LEVEL, source_template);
}

PlanetParams make_params() {
    PlanetParams p;
    p.planet_size_m  = PLANET_SIZE_M;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    return p;
}

// Independently reproduces what ChunkPipeline samples for a given chunk's
// hand-off tile, so tests can compare against build_context()'s output
// without depending on ChunkPipeline's internals.
struct ExpectedSample {
    MapTilePayload payload;       // hand-off (MAX_LEVEL) tile -- elevation/biome
    MapTilePayload city_payload;  // level-12 ancestor tile -- zone_candidates only
    int elev_gx{0}, elev_gy{0};
    int biome_gx{0}, biome_gy{0};
    int zc_gx{0}, zc_gy{0};
};

// The only level city.lua (the only script that ever calls
// setZoneCandidates()) runs at -- must match ChunkPipeline.cpp's own
// kCityZoningLevel exactly.
constexpr int kCityZoningLevel = 12;

// M157 — mirrors ChunkPipeline.cpp's own (private) region_from_zone_candidate():
// a direct 1:1 lookup, `none` mapping to RegionType::open (same as there).
RegionType expected_region_for_zone_candidate(ZoneCandidate c) {
    switch (c) {
        case ZoneCandidate::small_house_block: return RegionType::small_house_block;
        case ZoneCandidate::apartment_block:   return RegionType::apartment_block;
        case ZoneCandidate::shop_street:       return RegionType::shop_street;
        case ZoneCandidate::park:              return RegionType::park;
        case ZoneCandidate::square:            return RegionType::square;
        case ZoneCandidate::none:              break;
    }
    return RegionType::open;
}

ExpectedSample sample_for_chunk(MapPipeline& map_pipeline, const ChunkCoord& coord, int chunk_size_m) {
    const double world_x = static_cast<double>(coord.world_x(chunk_size_m)) + chunk_size_m * 0.5;
    const double world_z = static_cast<double>(coord.world_z(chunk_size_m)) + chunk_size_m * 0.5;

    const TileCoord   tile   = TileCoord::from_world(world_x, world_z, MAX_LEVEL);
    const WorldBounds bounds = tile.world_bounds();
    const double u = (world_x - bounds.min_x) / (bounds.max_x - bounds.min_x);
    const double v = (world_z - bounds.min_z) / (bounds.max_z - bounds.min_z);

    ExpectedSample s;
    s.payload = map_pipeline.get(tile);  // same cache entry ChunkPipeline will hit
    if (!s.payload.elevation.empty()) {
        s.elev_gx = std::clamp(static_cast<int>(u * s.payload.elevation.w), 0, s.payload.elevation.w - 1);
        s.elev_gy = std::clamp(static_cast<int>(v * s.payload.elevation.h), 0, s.payload.elevation.h - 1);
    }
    if (!s.payload.biome.empty()) {
        s.biome_gx = std::clamp(static_cast<int>(u * s.payload.biome.w), 0, s.payload.biome.w - 1);
        s.biome_gy = std::clamp(static_cast<int>(v * s.payload.biome.h), 0, s.payload.biome.h - 1);
    }
    // zone_candidates only ever exists at level 12 (city.lua) -- a separate
    // ancestor lookup, not the hand-off tile above (mirrors ChunkPipeline.
    // cpp's own populate_map_context()).
    const TileCoord   city_tile   = TileCoord::from_world(world_x, world_z, kCityZoningLevel);
    const WorldBounds city_bounds = city_tile.world_bounds();
    const double city_u = (world_x - city_bounds.min_x) / (city_bounds.max_x - city_bounds.min_x);
    const double city_v = (world_z - city_bounds.min_z) / (city_bounds.max_z - city_bounds.min_z);
    s.city_payload = map_pipeline.get(city_tile);
    if (!s.city_payload.zone_candidates.empty()) {
        s.zc_gx = std::clamp(static_cast<int>(city_u * s.city_payload.zone_candidates.w), 0,
                             s.city_payload.zone_candidates.w - 1);
        s.zc_gy = std::clamp(static_cast<int>(city_v * s.city_payload.zone_candidates.h), 0,
                             s.city_payload.zone_candidates.h - 1);
    }
    return s;
}

} // namespace

// M163 — with no MapPipeline attached, map_context stays unavailable and the
// existing (WorldMap-only) fields still populate normally.
TEST(ChunkPipelineMapContextTest, UnavailableWithoutMapPipeline) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_FALSE(ctx.map_context.available);
    EXPECT_FALSE(ctx.map_context.is_city);
    EXPECT_FALSE(ctx.map_context.has_road_crossing);

    // Unaffected: get() must still produce a chunk exactly as before M159.
    EXPECT_FALSE(pipeline.get(0, 0).empty());
}

// M159 — with a MapPipeline attached, build_context() looks up the chunk's
// enclosing hand-off tile and marks map_context available.
TEST(ChunkPipelineMapContextTest, PopulatedWithMapPipeline) {
    auto world = PlanetWorld::create_new(tmp_dir("populated"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_FALSE(ctx.map_context.is_city);          // biome isn't city here (no Lua registered)
    EXPECT_FALSE(ctx.map_context.has_road_crossing); // MAP9 data doesn't exist yet

    // get() must still generate a valid chunk with the map layer attached.
    EXPECT_FALSE(pipeline.get(0, 0).empty());
}

// M158 — without a MapPipeline attached, map_context stays entirely
// unavailable/default, so all 4 new M158 fields must read as "not
// computed" (empty name, negative distance).
TEST(ChunkPipelineMapContextTest, NewM158FieldsDefaultWithoutMapPipeline) {
    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.nearest_river_name.empty());
    EXPECT_FLOAT_EQ(ctx.map_context.nearest_river_distance_m, -1.0f);
    EXPECT_TRUE(ctx.map_context.nearest_place_name.empty());
    EXPECT_TRUE(ctx.map_context.nearest_place_kind.empty());
}

// M159 — nearest_place_name/nearest_place_kind still default to "not
// computed" even WITH a MapPipeline attached: nothing calls
// Settlements::appendLabels()/Countries::name() from any real generator
// yet (see NEXT.md §2), so MapTilePayload::labels is always empty.
// is_city/nearest_river_* are NOT checked here — M159 populates those for
// real from map data, so their value depends on the (randomly generated)
// world's own terrain; see the dedicated tests below for those.
TEST(ChunkPipelineMapContextTest, NearestPlaceStillDefaultWithMapPipeline) {
    auto world = PlanetWorld::create_new(tmp_dir("m158_defaults"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_TRUE(ctx.map_context.nearest_place_name.empty());
    EXPECT_TRUE(ctx.map_context.nearest_place_kind.empty());
}

// M159 — is_city is trivially derived from biome_ordinal == ZoneType::city
// (no MAP9 settlement data needed): the C++ BiomeClassifier never produces
// ZoneType::city (only city.lua's markUrbanCells() does), so without
// registering a Lua source, is_city must reliably stay false.
TEST(ChunkPipelineMapContextTest, IsCityStaysFalseWithoutAnyCityLuaRegistered) {
    auto world = PlanetWorld::create_new(tmp_dir("m159_is_city_false"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_FALSE(ctx.map_context.is_city);
}

// M159 — deterministic proof that is_city/nearest_river_* are populated
// for real: a minimal hand-written Lua source registered for the hand-off
// level itself (MAX_LEVEL) marks every cell urban and adds one river with
// a known point, mirroring RegionActuallyOverriddenByRealZoneCandidateData's
// own convention (a real random world's terrain can't be relied on to have
// either nearby chunk (0,0)).
TEST(ChunkPipelineMapContextTest, IsCityAndNearestRiverPopulatedFromHandoffTileData) {
    const std::string lua_id = "lua.map.child.level" + std::to_string(MAX_LEVEL) + ".default";
    const std::string source = R"lua(
local M = {}
M.id = "PLACEHOLDER_ID"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist, mask = {}, {}, {}, {}
    for i = 1, n * n do elev[i] = 50.0; temp[i] = 15.0; moist[i] = 0.4; mask[i] = 1 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:markUrbanCells(mask)
    map:addRiver("Test River", {{10.0, 10.0}, {20.0, 20.0}})
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    // Substitute the real id in (can't easily interpolate M.level inside
    // the Lua source itself without hardcoding MAX_LEVEL twice).
    const std::string patched_source =
        [&] {
            std::string s      = source;
            const std::string marker = "PLACEHOLDER_ID";
            const auto        pos    = s.find(marker);
            return s.replace(pos, marker.size(), lua_id);
        }();
    ScopedLuaMapGenerator guard(lua_id, patched_source);

    auto world = PlanetWorld::create_new(tmp_dir("m159_real_data"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    // Chunk (0,0)'s world center (32, 32) falls inside the level-MAX_LEVEL
    // tile (0,0,0), which always covers the world's own origin corner.
    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_TRUE(ctx.map_context.is_city);
    EXPECT_EQ(ctx.map_context.nearest_river_name, "Test River");
    // Nearest of {10,10}/{20,20} to (32,32) is (20,20): sqrt(12^2+12^2).
    EXPECT_NEAR(ctx.map_context.nearest_river_distance_m, std::sqrt(288.0), 0.01);
}

// M166 — chunk elevation matches the map elevation field sampled independently
// at the same chunk center.
TEST(ChunkPipelineMapContextTest, ElevationMatchesMapFieldAtChunkCenter) {
    auto world = PlanetWorld::create_new(tmp_dir("elevation"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkCoord coord{3, 5};
    const ExpectedSample expected = sample_for_chunk(map_pipeline, coord, cfg.chunk_size_m);
    ASSERT_FALSE(expected.payload.elevation.empty());

    const ChunkContext ctx = pipeline.build_context(coord.x, coord.y);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_FLOAT_EQ(ctx.map_context.elevation_m,
                     expected.payload.elevation.at(expected.elev_gx, expected.elev_gy));

    if (!expected.payload.biome.empty()) {
        EXPECT_EQ(ctx.map_context.biome_ordinal,
                  expected.payload.biome.at(expected.biome_gx, expected.biome_gy));
    }
}

// M157 — ChunkContext.zone comes from the map layer's biome once map_context
// is available, overriding the flat WorldMap zone (BiomeGrid stores raw
// ZoneType ordinals, so the cast round-trips exactly — see PlanetGenerator).
TEST(ChunkPipelineMapContextTest, ZoneOverriddenByMapBiomeWhenAvailable) {
    auto world = PlanetWorld::create_new(tmp_dir("zone_override"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkCoord coord{2, 7};
    const ExpectedSample expected = sample_for_chunk(map_pipeline, coord, cfg.chunk_size_m);
    ASSERT_FALSE(expected.payload.biome.empty());
    const ZoneType expected_zone =
        static_cast<ZoneType>(expected.payload.biome.at(expected.biome_gx, expected.biome_gy));

    const ChunkContext ctx = pipeline.build_context(coord.x, coord.y);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, expected_zone);
}

// M157 — without a MapPipeline attached, ChunkContext.zone must still come
// from the flat WorldMap exactly as before (no override to apply).
TEST(ChunkPipelineMapContextTest, ZoneUnchangedWithoutMapPipeline) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map);

    const ChunkInfo    flat = map.info(4, 4);
    const ChunkContext ctx  = pipeline.build_context(4, 4);
    EXPECT_FALSE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, flat.zone);
}

// R129 (zone-metadata bug fix, NEXT.md §4) — ChunkContext.authored_zone
// must always hold the flat WorldMap/WorldConfig zone, even once the M157
// override above has replaced ctx.zone with the map layer's own sampled
// biome. This is what lets GenerationMetadata report the zone this
// world's own flat config actually authored (e.g. "city") instead of an
// unrelated planet's biome at whatever coordinates a freshly auto-created
// hand-off world happens to occupy (e.g. "deep_ocean").
TEST(ChunkPipelineMapContextTest, AuthoredZoneUnaffectedByMapBiomeOverride) {
    auto world = PlanetWorld::create_new(tmp_dir("authored_zone_override"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkCoord coord{2, 7};
    const ExpectedSample expected = sample_for_chunk(map_pipeline, coord, cfg.chunk_size_m);
    ASSERT_FALSE(expected.payload.biome.empty());
    const ZoneType map_biome_zone =
        static_cast<ZoneType>(expected.payload.biome.at(expected.biome_gx, expected.biome_gy));
    const ChunkInfo flat = map.info(coord);

    const ChunkContext ctx = pipeline.build_context(coord.x, coord.y);
    EXPECT_TRUE(ctx.map_context.available);
    // The override still applies to ctx.zone, exactly as
    // ZoneOverriddenByMapBiomeWhenAvailable already proves...
    EXPECT_EQ(ctx.zone, map_biome_zone);
    // ...but ctx.authored_zone must stay the flat, WorldConfig-derived
    // value, regardless of what the map layer's biome happened to be.
    EXPECT_EQ(ctx.authored_zone, flat.zone);
}

// R129 — without a MapPipeline attached, authored_zone must equal zone
// exactly (both come from the same flat WorldMap value, no override to
// diverge them).
TEST(ChunkPipelineMapContextTest, AuthoredZoneUnchangedWithoutMapPipeline) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map);

    const ChunkInfo    flat = map.info(4, 4);
    const ChunkContext ctx  = pipeline.build_context(4, 4);
    EXPECT_FALSE(ctx.map_context.available);
    EXPECT_EQ(ctx.authored_zone, flat.zone);
    EXPECT_EQ(ctx.authored_zone, ctx.zone);
}

// M156/M157 — ChunkContext.region reflects the map layer's ZoneCandidate
// (city.lua, level 12) once map_context is available AND a real candidate
// was assigned there; `none` (city.lua never ran here, or this cell wasn't
// part of a zoned block) leaves region on the flat WorldMap path, untouched
// — same fallback discipline ZoneOverriddenByMapBiomeWhenAvailable proves
// for zone. Whether THIS specific coordinate happens to have a candidate
// depends on the freshly-generated world's own random layout, so this test
// checks the invariant that holds either way rather than assuming one.
TEST(ChunkPipelineMapContextTest, RegionReflectsMapZoneCandidateWhenAvailable) {
    auto world = PlanetWorld::create_new(tmp_dir("region_reflects"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkCoord coord{2, 7};
    const ExpectedSample expected = sample_for_chunk(map_pipeline, coord, cfg.chunk_size_m);
    const ChunkContext   ctx      = pipeline.build_context(coord.x, coord.y);
    EXPECT_TRUE(ctx.map_context.available);

    if (expected.city_payload.zone_candidates.empty()) {
        EXPECT_EQ(ctx.region, map.info(coord).region);
        return;
    }
    const auto candidate = static_cast<ZoneCandidate>(
        expected.city_payload.zone_candidates.at(expected.zc_gx, expected.zc_gy));
    if (candidate == ZoneCandidate::none) {
        EXPECT_EQ(ctx.region, map.info(coord).region);
    } else {
        EXPECT_EQ(ctx.region, expected_region_for_zone_candidate(candidate));
    }
}

// M157 — a stronger, existence-proving companion to the consistency check
// above: proves the override fires for REAL city.lua-produced data, not
// just "the two fallback paths happen to agree when there's nothing to
// override." Uses a minimal hand-written Lua source (same convention
// MapPipelineTests.cpp's own LEVEL3_LUA/GENERIC_CHILD_LUA use) that
// unconditionally assigns ZoneCandidate::shop_street (ordinal 3) to every
// cell, rather than the real city.lua file — real terrain is only
// buildable/urbanized where a randomly-generated planet happens to have
// land near the scanned tile, which a huge (22,585 km) sparse-continent
// planet makes unreliable to depend on on. This test exists specifically
// because of the bug the level-mismatch fix above closed: zone_candidates
// only ever exists at level 12, never at the hand-off's own MAX_LEVEL
// tile, so sampling the wrong tile would make this override a permanent
// no-op despite passing the weaker consistency test.
TEST(ChunkPipelineMapContextTest, RegionActuallyOverriddenByRealZoneCandidateData) {
    constexpr const char* ALWAYS_SHOP_STREET_LUA = R"lua(
local M = {}
M.id = "lua.map.child.level12.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist, zc = {}, {}, {}, {}
    for i = 1, n * n do elev[i] = 100.0; temp[i] = 15.0; moist[i] = 0.4; zc[i] = 3 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setZoneCandidates(zc)
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.child.level12.default", ALWAYS_SHOP_STREET_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("region_override_real"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    WorldMap    map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    // Chunk (0, 0)'s world center falls inside level-12 tile (0, 0, 0) --
    // guaranteed by quadtree tile (0,0,0) always covering the world's own
    // origin corner at every level, no dependence on real terrain.
    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.region, RegionType::shop_street);
}

// R138 -- map tiles enrich an explicitly configured/persistent city but must
// not replace its validated local layout. This is the exact protection that
// keeps a road/crossroad from becoming a map-selected parcel at generation.
TEST(ChunkPipelineMapContextTest, MapZoneCandidateDoesNotOverrideConfiguredRoadLayout) {
    constexpr const char* ALWAYS_SHOP_STREET_LUA = R"lua(
local M = {}
M.id = "lua.map.child.level12.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist, zc = {}, {}, {}, {}
    for i = 1, n * n do elev[i] = 100.0; temp[i] = 15.0; moist[i] = 0.4; zc[i] = 3 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setZoneCandidates(zc)
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.child.level12.default", ALWAYS_SHOP_STREET_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("configured_road_not_overridden"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig cfg;
    cfg.grid_w = 4; cfg.grid_h = 4;
    ZoneOverride city;
    city.x_min = 0; city.x_max = 3; city.y_min = 0; city.y_max = 3;
    city.type = ZoneType::city;
    city.region_default = RegionType::small_house_block;
    city.regions.push_back(RegionOverride{0, 0, 0, 3, RegionType::road});
    cfg.zones.push_back(city);
    WorldMap map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 1);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, ZoneType::city);
    EXPECT_EQ(ctx.region, RegionType::road);
}

// M162/R134 — MapContext::has_road_crossing reflects a REAL road/street
// crossing recorded on the level-15 (neighborhood.lua) tile covering this
// chunk, but it must not fabricate a local edge exit: the tile is coarser
// than chunks and cannot prove that the neighbouring chunk agrees. Same
// deterministic
// custom-Lua-source technique as RegionActuallyOverriddenByRealZoneCandidateData
// above, for the same reason: a real random world can't be relied on to
// have a street crossing near an arbitrary test coordinate.
TEST(ChunkPipelineMapContextTest, NearbyStreetCrossingDoesNotFabricateLocalExit) {
    constexpr const char* STREET_CROSSING_NORTH_LUA = R"lua(
local M = {}
M.id = "lua.map.child.level15.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 50.0; temp[i] = 15.0; moist[i] = 0.4 end
    map:setBiomeField(n, n, elev, temp, moist)
    -- Touches this tile's own N edge (z=0) at x=32, inside chunk (0,0)'s
    -- own [0,64] x-span, then heads inward -- a real street can only ever
    -- touch its own tile's boundary, never cross past it (MapValidator).
    map:addStreet("Test St", {{32.0, 0.0}, {32.0, 10.0}})
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.child.level15.default", STREET_CROSSING_NORTH_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("edge_exits_real"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    // Chunk (0,0)'s world span [0,64]x[0,64] sits inside level-15 tile
    // (0,0,0) and touches its N/W boundary (both at world 0) -- guaranteed
    // by quadtree tile (0,0,0) always covering the world's own origin
    // corner at every level.
    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_TRUE(ctx.map_context.has_road_crossing);
    EXPECT_FALSE(ctx.exits.north_road);
    EXPECT_FALSE(ctx.exits.south_road);
    EXPECT_FALSE(ctx.exits.east_road);
}

// M162/R134 — without a matching crossing nearby, the flat WorldMap edge
// graph stays unchanged and no map-context road hint is emitted.
TEST(ChunkPipelineMapContextTest, EdgeExitsUnaffectedWithNoNearbyStreetCrossing) {
    constexpr const char* NO_STREET_LUA = R"lua(
local M = {}
M.id = "lua.map.child.level15.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 50.0; temp[i] = 15.0; moist[i] = 0.4 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.child.level15.default", NO_STREET_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("edge_exits_none"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_FALSE(ctx.map_context.has_road_crossing);
    EXPECT_EQ(ctx.exits.north_road, map.info({0, 0}).exits.north_road);
    EXPECT_EQ(ctx.exits.south_road, map.info({0, 0}).exits.south_road);
    EXPECT_EQ(ctx.exits.east_road, map.info({0, 0}).exits.east_road);
    EXPECT_EQ(ctx.exits.west_road, map.info({0, 0}).exits.west_road);
}

// M165 (MAP10) — "a forest/ocean tile produces matching chunk biome at the
// hand-off" (plan.md's own wording). Deterministic custom Lua sources at
// the hand-off level itself control the exact biome directly via
// BiomeClassifier's own thresholds, rather than depending on a real random
// world's terrain (the same lesson M157's/M159's/M162's own tests
// established).
TEST(ChunkPipelineMapContextTest, ChunkZoneMatchesForestBiomeAtHandoff) {
    auto guard = register_handoff_level_generator(R"lua(
local M = {}
M.id = "PLACEHOLDER_ID"
function M.generate(ctx, map)
    local n = 4
    -- elevation=100 (land, below every mountain/alpine threshold), temp=15C,
    -- moisture=0.6 -- BiomeClassifier::classify() gives Forest for these.
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 100.0; temp[i] = 15.0; moist[i] = 0.6 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "nordic")
end
return M
)lua");

    auto world = PlanetWorld::create_new(tmp_dir("m165_forest"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, ZoneType::forest);
}

TEST(ChunkPipelineMapContextTest, ChunkZoneMatchesOceanBiomeAtHandoff) {
    auto guard = register_handoff_level_generator(R"lua(
local M = {}
M.id = "PLACEHOLDER_ID"
function M.generate(ctx, map)
    local n = 4
    -- elevation well below sea level -- BiomeClassifier::classify() gives
    -- Ocean unconditionally, regardless of temperature/moisture.
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = -500.0; temp[i] = 15.0; moist[i] = 0.4 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "nordic")
end
return M
)lua");

    auto world = PlanetWorld::create_new(tmp_dir("m165_ocean"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, ZoneType::ocean);
}

// M164 (MAP10) — "a city tile produces chunks with city ZoneType +
// connected roads" (plan.md's own wording). Two deterministic custom Lua
// sources registered simultaneously, matching ChunkPipeline.cpp's own two
// INDEPENDENT lookups: one at the hand-off level (MAX_LEVEL) marking every
// cell urban (ZoneType::city, sampled by populate_map_context()'s own
// biome lookup there), and one at level 15 (kStreetCrossingLevel in
// ChunkPipeline.cpp) with a street touching the tile's own N boundary
// (sampled by populate_nearby_road_crossing()'s own level-15-specific lookup) -- the
// generic C++ ChildGenerator that would otherwise fill levels 16-18 has no
// way to inherit a markUrbanCells() override from an ancestor
// (BiomeClassifier has no "city" output), so the zone half needs its own
// registration at MAX_LEVEL, independent of the roads half at level 15.
//
// NOTE: this deliberately does NOT prove real emergent road connectivity
// from city.lua's own actual generation -- §5 #18 already establishes
// that no script's real streets reach their own tile boundary yet
// (align_grid_line()'s cell-center formula, M155), so no genuinely-
// generated city tile can show this today. This test proves the
// map context reports the real crossing without converting the coarse map
// signal into a potentially one-sided local road. A future local map-road
// materialiser will consume this hint through its own symmetric graph.
TEST(ChunkPipelineMapContextTest, CityTileProducesCityZoneAndRoadCrossingHint) {
    auto zone_guard = register_handoff_level_generator(R"lua(
local M = {}
M.id = "PLACEHOLDER_ID"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist, mask = {}, {}, {}, {}
    for i = 1, n * n do elev[i] = 50.0; temp[i] = 15.0; moist[i] = 0.4; mask[i] = 1 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:markUrbanCells(mask)
    map:setMetadata(M.id, "nordic")
end
return M
)lua");
    auto roads_guard = register_level_generator(15, R"lua(
local M = {}
M.id = "PLACEHOLDER_ID"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 50.0; temp[i] = 15.0; moist[i] = 0.4 end
    map:setBiomeField(n, n, elev, temp, moist)
    -- Touches this tile's own N edge (z=0) at x=32, inside chunk (0,0)'s
    -- own [0,64] x-span, then heads inward.
    map:addStreet("Main St", {{32.0, 0.0}, {32.0, 10.0}})
    map:setMetadata(M.id, "nordic")
end
return M
)lua");

    auto world = PlanetWorld::create_new(tmp_dir("m164_city_roads"));
    MapPipeline map_pipeline(world, make_params());

    WorldConfig   cfg;
    WorldMap      map(cfg);
    ChunkPipeline pipeline(cfg, map, ChunkCache{}, &map_pipeline);

    const ChunkContext ctx = pipeline.build_context(0, 0);
    EXPECT_TRUE(ctx.map_context.available);
    EXPECT_EQ(ctx.zone, ZoneType::city);
    EXPECT_FALSE(ctx.exits.north_road);
    EXPECT_TRUE(ctx.map_context.has_road_crossing);
}
