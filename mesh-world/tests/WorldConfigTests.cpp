// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ChunkCoord.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "ProceduralWorldGen.hpp"
#include "Map/PlanetConstants.hpp"

using namespace MeshWorld;

// ---- WorldConfig loading ------------------------------------------------

TEST(WorldConfig, DefaultValuesWithoutFile) {
    WorldConfig cfg;
    EXPECT_EQ(cfg.seed,         42u);
    EXPECT_EQ(cfg.grid_w,       20);
    EXPECT_EQ(cfg.grid_h,       20);
    EXPECT_EQ(cfg.chunk_size_m, 64);
    EXPECT_EQ(cfg.map_size_m,   1280);
}

TEST(WorldConfig, LoadMissingFileReturnsFalse) {
    WorldConfig cfg;
    EXPECT_FALSE(cfg.load_from_file("nonexistent_file.json"));
}

// ---- M039: planetary map fields ----------------------------------------

TEST(WorldConfig, DefaultPlanetFields) {
    WorldConfig cfg;
    EXPECT_DOUBLE_EQ(cfg.planet_size_m, Map::PLANET_SIZE_M);
    EXPECT_EQ(cfg.continents_min, 5);
    EXPECT_EQ(cfg.continents_max, 12);
    EXPECT_DOUBLE_EQ(cfg.sea_level_m, 0.0);
    EXPECT_DOUBLE_EQ(cfg.equator_temp_c, 30.0);
    EXPECT_DOUBLE_EQ(cfg.pole_temp_c, -20.0);
}

TEST(WorldConfig, PlanetFieldsLoadFromJson) {
    namespace fs = std::filesystem;
    const fs::path p = fs::temp_directory_path() / "mw_worldconfig_planet_M039.json";
    {
        std::ofstream f(p);
        f << R"({
            "name": "planet_test",
            "planet_size_m": 12345678.0,
            "continents_min": 6,
            "continents_max": 15,
            "sea_level_m": -10.0,
            "equator_temp_c": 28.0,
            "pole_temp_c": -30.0
        })";
    }

    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file(p.string()));
    EXPECT_DOUBLE_EQ(cfg.planet_size_m, 12345678.0);
    EXPECT_EQ(cfg.continents_min, 6);
    EXPECT_EQ(cfg.continents_max, 15);
    EXPECT_DOUBLE_EQ(cfg.sea_level_m, -10.0);
    EXPECT_DOUBLE_EQ(cfg.equator_temp_c, 28.0);
    EXPECT_DOUBLE_EQ(cfg.pole_temp_c, -30.0);
    fs::remove(p);
}

// A world.json without planet fields still loads; the defaults apply.
TEST(WorldConfig, MissingPlanetFieldsKeepDefaults) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    EXPECT_DOUBLE_EQ(cfg.planet_size_m, Map::PLANET_SIZE_M);
    EXPECT_EQ(cfg.continents_min, 5);
    EXPECT_EQ(cfg.continents_max, 12);
}

// M040 — is_consistent() accepts the defaults and rejects out-of-range planet fields.
TEST(WorldConfig, PlanetFieldValidation) {
    WorldConfig cfg;  // defaults: chunk grid consistent, continents 5..12
    EXPECT_TRUE(cfg.is_consistent());

    cfg.continents_min = 3;  // below the allowed minimum of 4
    EXPECT_FALSE(cfg.is_consistent());

    cfg.continents_min = 5;
    cfg.continents_max = 21;  // above the allowed maximum of 20
    EXPECT_FALSE(cfg.is_consistent());

    cfg.continents_max = 8;
    cfg.continents_min = 9;  // min > max
    EXPECT_FALSE(cfg.is_consistent());

    cfg.continents_min = 4;
    cfg.continents_max = 20;  // boundary values are valid
    EXPECT_TRUE(cfg.is_consistent());

    cfg.planet_size_m = 0.0;  // non-positive planet size
    EXPECT_FALSE(cfg.is_consistent());
}

TEST(WorldConfig, LoadExampleWorldJson) {
    WorldConfig cfg;
    bool ok = cfg.load_from_file("examples/world.json");
    ASSERT_TRUE(ok) << "examples/world.json must be loadable from the working directory";

    EXPECT_EQ(cfg.seed,           42u);
    EXPECT_EQ(cfg.grid_w,         20);
    EXPECT_EQ(cfg.grid_h,         20);
    EXPECT_EQ(cfg.chunk_size_m,   64);
    EXPECT_EQ(cfg.name,           "demo_city");
    EXPECT_EQ(cfg.zone_default,   ZoneType::empty);
    EXPECT_EQ(cfg.region_default, RegionType::empty);
    EXPECT_FALSE(cfg.zones.empty());
}

TEST(WorldConfig, LoadedZoneHasNestedRegions) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    ASSERT_FALSE(cfg.zones.empty());

    const auto& city = cfg.zones[0];
    EXPECT_EQ(city.type,           ZoneType::city);
    EXPECT_EQ(city.region_default, RegionType::small_house_block);
    EXPECT_FALSE(city.regions.empty());
}

// ---- R128: WorldConfig::landmarks ---------------------------------------

TEST(WorldConfig, DefaultLandmarksAreEmpty) {
    WorldConfig cfg;
    EXPECT_TRUE(cfg.landmarks.empty());
}

TEST(WorldConfig, LandmarksLoadFromJson) {
    namespace fs = std::filesystem;
    const fs::path p = fs::temp_directory_path() / "mw_worldconfig_landmarks_R128.json";
    {
        std::ofstream f(p);
        f << R"({
            "name": "landmark_test",
            "landmarks": [
                { "chunk_x": 2, "chunk_y": 3, "definition_id": "landmark.clocktower_01",
                  "x": 10.0, "z": 20.0, "rotation_y": 45.0 }
            ]
        })";
    }

    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file(p.string()));
    ASSERT_EQ(cfg.landmarks.size(), 1u);
    EXPECT_EQ(cfg.landmarks[0].chunk_x, 2);
    EXPECT_EQ(cfg.landmarks[0].chunk_y, 3);
    EXPECT_EQ(cfg.landmarks[0].definition_id, "landmark.clocktower_01");
    EXPECT_FLOAT_EQ(cfg.landmarks[0].x, 10.0f);
    EXPECT_FLOAT_EQ(cfg.landmarks[0].z, 20.0f);
    EXPECT_FLOAT_EQ(cfg.landmarks[0].rotation_y, 45.0f);
    fs::remove(p);
}

TEST(WorldConfig, CityShowcaseHasOneLandmark) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/city_showcase.json"));
    EXPECT_TRUE(cfg.is_consistent());
    ASSERT_EQ(cfg.landmarks.size(), 1u);
    EXPECT_EQ(cfg.landmarks[0].definition_id, "landmark.clocktower_01");
    EXPECT_TRUE(cfg.use_world_composer)
        << "the showcase must actually demonstrate composer-generated content by default";
}

TEST(WorldConfig, CityShowcaseMixesAllComposerRegionsAroundItsCrossroads) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/city_showcase.json"));

    WorldMap map(cfg);
    EXPECT_EQ(map.region(3, 3), RegionType::crossroad);
    EXPECT_EQ(map.region(4, 2), RegionType::apartment_block);
    EXPECT_EQ(map.region(2, 4), RegionType::shop_street);
    EXPECT_EQ(map.region(1, 2), RegionType::square);
    EXPECT_EQ(map.region(2, 2), RegionType::small_house_block);

    // All featured composer regions are within the app's radius-3 circular
    // streaming area around the central crossroad, so the demo presents the
    // full asset mix immediately rather than requiring a long walk.
    for (const ChunkCoord featured : {
             ChunkCoord{4, 2}, ChunkCoord{2, 4}, ChunkCoord{4, 4},
             ChunkCoord{1, 2}, ChunkCoord{2, 2}}) {
        const int dx = featured.x - 3;
        const int dy = featured.y - 3;
        EXPECT_LE(dx * dx + dy * dy, 9);
    }
}

// R142 -- this is a deliberately authored visual tour, not a procedural map
// sample. Keep all nine environments adjacent to the forest spawn so the app
// demonstrates nature immediately and a reviewer can reach every family.
TEST(WorldConfig, BiomeShowcasePlacesNineNaturalZonesAroundForestSpawn) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/biome_showcase.json"));
    ASSERT_TRUE(cfg.is_consistent());
    EXPECT_EQ(cfg.grid_w, 9);
    EXPECT_EQ(cfg.grid_h, 9);

    WorldMap map(cfg);
    EXPECT_EQ(map.zone(4, 4), ZoneType::forest);
    EXPECT_EQ(map.zone(1, 1), ZoneType::jungle);
    EXPECT_EQ(map.zone(4, 1), ZoneType::mountain);
    EXPECT_EQ(map.zone(7, 1), ZoneType::tundra);
    EXPECT_EQ(map.zone(1, 4), ZoneType::meadow);
    EXPECT_EQ(map.zone(7, 4), ZoneType::desert);
    EXPECT_EQ(map.zone(1, 7), ZoneType::beach);
    EXPECT_EQ(map.zone(4, 7), ZoneType::swamp);
    EXPECT_EQ(map.zone(7, 7), ZoneType::ocean);
    EXPECT_EQ(map.region(4, 4), RegionType::open);
}

// ---- ChunkCoord ---------------------------------------------------------

TEST(ChunkCoord, FromWorldOrigin) {
    auto c = ChunkCoord::from_world(0.0f, 0.0f, 64);
    EXPECT_EQ(c.x, 0);
    EXPECT_EQ(c.y, 0);
}

TEST(ChunkCoord, FromWorldCenter) {
    auto c = ChunkCoord::from_world(640.0f, 640.0f, 64);
    EXPECT_EQ(c.x, 10);
    EXPECT_EQ(c.y, 10);
}

TEST(ChunkCoord, FromWorldEdge) {
    auto c = ChunkCoord::from_world(1279.9f, 1279.9f, 64);
    EXPECT_EQ(c.x, 19);
    EXPECT_EQ(c.y, 19);
}

TEST(ChunkCoord, ToString) {
    ChunkCoord c{3, 7};
    EXPECT_EQ(c.to_string(), "3_7");
}

TEST(ChunkCoord, Neighbors) {
    ChunkCoord c{5, 5};
    EXPECT_EQ(c.north(), (ChunkCoord{5, 4}));
    EXPECT_EQ(c.south(), (ChunkCoord{5, 6}));
    EXPECT_EQ(c.west(),  (ChunkCoord{4, 5}));
    EXPECT_EQ(c.east(),  (ChunkCoord{6, 5}));
}

// ---- chunk_seed ---------------------------------------------------------

TEST(ChunkSeed, DifferentCoordsGiveDifferentSeeds) {
    uint64_t s00 = chunk_seed(42, {0, 0});
    uint64_t s01 = chunk_seed(42, {0, 1});
    uint64_t s10 = chunk_seed(42, {1, 0});
    EXPECT_NE(s00, s01);
    EXPECT_NE(s00, s10);
    EXPECT_NE(s01, s10);
}

TEST(ChunkSeed, SameInputsGiveSameOutput) {
    EXPECT_EQ(chunk_seed(42, {3, 7}), chunk_seed(42, {3, 7}));
}

TEST(ChunkSeed, DifferentWorldSeedsGiveDifferentChunkSeeds) {
    EXPECT_NE(chunk_seed(1, {5, 5}), chunk_seed(2, {5, 5}));
}

// ---- ZoneType -----------------------------------------------------------

TEST(ZoneType, RoundTrip) {
    for (auto z : {ZoneType::city, ZoneType::jungle, ZoneType::desert,
                   ZoneType::forest, ZoneType::ocean, ZoneType::mountain,
                   ZoneType::tundra, ZoneType::swamp, ZoneType::cave,
                   ZoneType::meadow, ZoneType::beach, ZoneType::empty}) {
        EXPECT_EQ(zone_from_string(to_string(z)), z);
    }
}

TEST(ZoneType, UnknownThrows) {
    EXPECT_THROW(zone_from_string("banana"), std::invalid_argument);
}

// ---- RegionType ---------------------------------------------------------

TEST(RegionType, RoundTrip) {
    for (auto r : {RegionType::road, RegionType::park, RegionType::open,
                   RegionType::water, RegionType::oasis, RegionType::cave_chamber,
                   RegionType::empty}) {
        EXPECT_EQ(region_from_string(to_string(r)), r);
    }
}

TEST(RegionType, LegacyAlias) {
    EXPECT_EQ(region_from_string("empty_field"), RegionType::empty);
}

TEST(RegionType, UnknownThrows) {
    EXPECT_THROW(region_from_string("banana"), std::invalid_argument);
}

TEST(RegionType, IsRoadRegion) {
    EXPECT_TRUE(is_road_region(RegionType::road));
    EXPECT_TRUE(is_road_region(RegionType::crossroad));
    EXPECT_FALSE(is_road_region(RegionType::park));
    EXPECT_FALSE(is_road_region(RegionType::open));
}

// ---- WorldMap -----------------------------------------------------------

TEST(WorldMap, OutOfBoundsReturnsEmpty) {
    WorldConfig cfg;
    WorldMap map(cfg);
    EXPECT_EQ(map.zone(-1, 0),  ZoneType::empty);
    EXPECT_EQ(map.zone(0, -1),  ZoneType::empty);
    EXPECT_EQ(map.zone(20, 0),  ZoneType::empty);
    EXPECT_EQ(map.zone(0, 20),  ZoneType::empty);
}

TEST(WorldMap, LoadedMapHasCorrectRegions) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    WorldMap map(cfg);

    // region = chunk type
    EXPECT_EQ(map.region(9,  9),  RegionType::square);
    EXPECT_EQ(map.region(10, 10), RegionType::square);
    EXPECT_EQ(map.region(2,  9),  RegionType::park);
    EXPECT_EQ(map.region(0,  5),  RegionType::river_bank);
    EXPECT_EQ(map.region(0,  0),  RegionType::river_bank);

    // outside all zone overrides → region_default = empty
    EXPECT_EQ(map.region(19, 19), RegionType::empty);
}

TEST(WorldMap, LoadedMapHasCorrectZones) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    WorldMap map(cfg);

    // zone = large area type
    EXPECT_EQ(map.zone(9,  9),  ZoneType::city);
    EXPECT_EQ(map.zone(0,  5),  ZoneType::city);

    // outside all zone overrides → zone_default = empty
    EXPECT_EQ(map.zone(19, 19), ZoneType::empty);
}

// R113 v3 -- WorldMap::build() now computes exits for EVERY chunk (was
// previously gated to only chunks that are themselves road/crossroad),
// so a chunk's own exit flag means "my neighbor in this direction is a
// road", independent of my OWN region type. This is no longer a
// symmetric relation across a shared border in general (a house-block
// chunk bordering a road chunk correctly sees east_road=true on its own
// side, while the road chunk's west_road reflects whether ITS OWN west
// neighbor -- the house block -- is ALSO a road, which it isn't) --
// that asymmetry IS the whole point (R113 v3's own cross-chunk
// continuity fix), not a bug. The real invariant to check is that each
// chunk's own exit flag exactly matches its real neighbor's region type.
TEST(WorldMap, RoadExitsMatchRealNeighborRegionType) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    WorldMap map(cfg);

    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            const auto e = map.exits(x, y);
            if (x > 0)
                EXPECT_EQ(e.west_road, is_road_region(map.region(x - 1, y)))
                    << "(" << x << "," << y << ") west_road";
            if (x < cfg.grid_w - 1)
                EXPECT_EQ(e.east_road, is_road_region(map.region(x + 1, y)))
                    << "(" << x << "," << y << ") east_road";
            if (y > 0)
                EXPECT_EQ(e.north_road, is_road_region(map.region(x, y - 1)))
                    << "(" << x << "," << y << ") north_road";
            if (y < cfg.grid_h - 1)
                EXPECT_EQ(e.south_road, is_road_region(map.region(x, y + 1)))
                    << "(" << x << "," << y << ") south_road";
        }
    }
}

// Two adjacent chunks that are BOTH road/crossroad-type still see each
// other symmetrically -- that part of the old invariant remains real and
// worth its own explicit check.
TEST(WorldMap, RoadExitsAreSymmetricBetweenTwoRoadChunks) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    WorldMap map(cfg);

    bool checked_at_least_one = false;
    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w - 1; ++x) {
            if (!is_road_region(map.region(x, y)) || !is_road_region(map.region(x + 1, y))) continue;
            checked_at_least_one = true;
            EXPECT_TRUE(map.exits(x, y).east_road);
            EXPECT_TRUE(map.exits(x + 1, y).west_road);
        }
    }
    EXPECT_TRUE(checked_at_least_one) << "examples/world.json's own road grid should contain "
                                          "at least one adjacent road-road pair";
}

// R134 -- physical road crossings are distinct from the legacy parcel
// frontage flags above. Every shared road edge is set on both endpoints, and
// a road cell never gains a spur merely because a house happens to border it.
TEST(WorldMap, CanonicalRoadConnectionsAreSymmetricAcrossTheSevenBySevenShowcase) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/city_showcase.json"));
    WorldMap map(cfg);

    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            const EdgeExits edges = map.road_connections(x, y);
            if (x + 1 < cfg.grid_w)
                EXPECT_EQ(edges.east_road, map.road_connections(x + 1, y).west_road)
                    << "east/west mismatch at " << x << "," << y;
            if (y + 1 < cfg.grid_h)
                EXPECT_EQ(edges.south_road, map.road_connections(x, y + 1).north_road)
                    << "north/south mismatch at " << x << "," << y;
        }
    }
    EXPECT_TRUE(map.validate_road_network().empty());
}

TEST(WorldMap, RemovingALatePersistentRoadClearsRatherThanOringStaleConnections) {
    WorldConfig cfg;
    cfg.grid_w = 4;
    cfg.grid_h = 4;
    cfg.map_size_m = 256;
    WorldMap map(cfg);

    map.set_info(1, 1, ChunkInfo{ZoneType::city, RegionType::road, {}});
    map.set_info(2, 1, ChunkInfo{ZoneType::city, RegionType::road, {}});
    EXPECT_TRUE(map.road_connections(1, 1).east_road);
    EXPECT_TRUE(map.road_connections(2, 1).west_road);

    map.set_info(2, 1, ChunkInfo{ZoneType::city, RegionType::small_house_block, {}});
    EXPECT_FALSE(map.road_connections(1, 1).east_road);
    EXPECT_FALSE(map.road_connections(2, 1).west_road);
    EXPECT_TRUE(map.road_frontage(2, 1).west_road)
        << "the parcel still faces the remaining neighbouring road";
}

TEST(WorldMap, UnapprovedInteriorRoadEndIsReported) {
    WorldConfig cfg;
    cfg.grid_w = 4;
    cfg.grid_h = 4;
    cfg.map_size_m = 256;
    ZoneOverride city;
    city.x_min = 0; city.x_max = 3; city.y_min = 0; city.y_max = 3;
    city.type = ZoneType::city;
    city.region_default = RegionType::small_house_block;
    city.regions.push_back(RegionOverride{1, 1, 1, 1, RegionType::road});
    cfg.zones.push_back(city);

    WorldMap map(cfg);
    const auto issues = map.validate_road_network();
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].x, 1);
    EXPECT_EQ(issues[0].y, 1);
    EXPECT_NE(issues[0].message.find("isolated"), std::string::npos);
}

// R113 v3's second half of the same user request ("a real street network
// with intersections"): examples/world.json's own road grid was extended
// with an outer ring (+ two partial connector segments, deliberately
// routed around the existing park regions) so the small_house_block ring
// -- previously bordering nothing but the apartment_block's own interior
// road grid -- gets real streets too. A real regression guard for that
// extension, computed via WorldMap directly (fast, no chunk generation
// needed) rather than trusting a one-off manual MeshWorldExport check.
TEST(WorldMap, SmallHouseBlockMostlyHasRealStreetAdjacencyAfterOuterRingExtension) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));
    WorldMap map(cfg);

    int total = 0, adjacent = 0;
    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            if (map.region(x, y) != RegionType::small_house_block) continue;
            ++total;
            const auto e = map.exits(x, y);
            if (e.north_road || e.south_road || e.east_road || e.west_road) ++adjacent;
        }
    }
    ASSERT_GT(total, 0);
    // Exactly 4 cells (x=18, y=8-11 -- immediately beside the existing
    // park regions, deliberately not routed through to avoid overwriting
    // them) are honestly expected to remain uncovered; a documented,
    // accepted limitation, not silently claimed as 100%.
    EXPECT_GE(adjacent, total - 4)
        << total << " small_house_block cells, only " << adjacent << " road-adjacent";
    EXPECT_GT(static_cast<double>(adjacent) / static_cast<double>(total), 0.9);
}

TEST(WorldMap, DefaultMapIsAllEmpty) {
    WorldConfig cfg;  // no file loaded: zone_default = empty, region_default = empty
    WorldMap map(cfg);
    for (int y = 0; y < cfg.grid_h; ++y)
        for (int x = 0; x < cfg.grid_w; ++x) {
            EXPECT_EQ(map.zone(x, y),   ZoneType::empty);
            EXPECT_EQ(map.region(x, y), RegionType::empty);
        }
}

// ---- ProceduralWorldGen -------------------------------------------------

TEST(ProceduralWorldGen, DeterministicForSameSeed) {
    ProceduralWorldGen gen(42, 4);
    EXPECT_EQ(gen.zone_at(0,  0),  gen.zone_at(0,  0));
    EXPECT_EQ(gen.zone_at(7,  3),  gen.zone_at(7,  3));
    EXPECT_EQ(gen.zone_at(19, 19), gen.zone_at(19, 19));
}

TEST(ProceduralWorldGen, DifferentSeedsGiveDifferentMaps) {
    ProceduralWorldGen gen1(1, 4);
    ProceduralWorldGen gen2(2, 4);
    int diff = 0;
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            if (gen1.zone_at(x, y) != gen2.zone_at(x, y)) ++diff;
    EXPECT_GT(diff, 0);
}

TEST(ProceduralWorldGen, ChunksInSameCellShareZone) {
    ProceduralWorldGen gen(42, 4);
    // Chunks (0,0),(1,0),(2,0),(3,0) are all in cell (0,0).
    const ZoneType z = gen.zone_at(0, 0);
    EXPECT_EQ(gen.zone_at(1, 0), z);
    EXPECT_EQ(gen.zone_at(2, 0), z);
    EXPECT_EQ(gen.zone_at(3, 0), z);
}

TEST(ProceduralWorldGen, NeverReturnsEmptyZone) {
    ProceduralWorldGen gen(999, 3);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            EXPECT_NE(gen.zone_at(x, y), ZoneType::empty);
}

// ---- WorldMap procedural mode -------------------------------------------

// ---- T242: world.local.json override ------------------------------------

TEST(WorldConfig, LocalJsonOverridesBaseFields) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "mw_worldconfig_local_T242_a";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path base_path  = dir / "world.json";
    const fs::path local_path = dir / "world.local.json";

    {
        std::ofstream f(base_path);
        f << R"({"name": "base_world", "seed": 1, "grid_w": 20, "grid_h": 20})";
    }
    {
        std::ofstream f(local_path);
        f << R"({"seed": 999})";
    }

    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file(base_path.string()));
    EXPECT_EQ(cfg.name, "base_world");  // untouched by the override
    EXPECT_EQ(cfg.seed, 999u);          // overridden

    fs::remove_all(dir);
}

TEST(WorldConfig, NoLocalJsonBehavesLikePlainLoad) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "mw_worldconfig_local_T242_b";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path base_path = dir / "world.json";

    {
        std::ofstream f(base_path);
        f << R"({"name": "no_override_world", "seed": 5})";
    }

    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file(base_path.string()));
    EXPECT_EQ(cfg.name, "no_override_world");
    EXPECT_EQ(cfg.seed, 5u);

    fs::remove_all(dir);
}

TEST(WorldConfig, MalformedLocalJsonFailsTheWholeLoad) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "mw_worldconfig_local_T242_c";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path base_path  = dir / "world.json";
    const fs::path local_path = dir / "world.local.json";

    {
        std::ofstream f(base_path);
        f << R"({"name": "base_world"})";
    }
    {
        std::ofstream f(local_path);
        f << "{ not valid json";
    }

    WorldConfig cfg;
    EXPECT_FALSE(cfg.load_from_file(base_path.string()));

    fs::remove_all(dir);
}

TEST(WorldConfig, LocalJsonReplacesZonesArrayWholesaleNotAppend) {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "mw_worldconfig_local_T242_d";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path base_path  = dir / "world.json";
    const fs::path local_path = dir / "world.local.json";

    {
        std::ofstream f(base_path);
        f << R"({
            "zones": [
                {"x_min": 0, "x_max": 5, "y_min": 0, "y_max": 5, "type": "city"}
            ]
        })";
    }
    {
        std::ofstream f(local_path);
        f << R"({
            "zones": [
                {"x_min": 0, "x_max": 3, "y_min": 0, "y_max": 3, "type": "forest"}
            ]
        })";
    }

    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file(base_path.string()));
    ASSERT_EQ(cfg.zones.size(), 1u);  // replaced, not appended (JSON Merge Patch semantics)
    EXPECT_EQ(cfg.zones[0].type, ZoneType::forest);

    fs::remove_all(dir);
}

TEST(WorldConfig, ProceduralFlagLoadsFromJson) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world_procedural.json"));
    EXPECT_TRUE(cfg.procedural);
    EXPECT_EQ(cfg.procedural_cell_size, 3);
    EXPECT_EQ(cfg.name, "procedural_world");
}

TEST(WorldMap, ProceduralModeAssignsNonEmptyZones) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world_procedural.json"));
    WorldMap map(cfg);

    int empty_count = 0;
    for (int y = 0; y < cfg.grid_h; ++y)
        for (int x = 0; x < cfg.grid_w; ++x)
            if (map.zone(x, y) == ZoneType::empty) ++empty_count;

    EXPECT_EQ(empty_count, 0);
}

TEST(WorldMap, ProceduralModeJsonOverrideHasPriority) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world_procedural.json"));
    WorldMap map(cfg);

    // The explicit city override covers (8-12, 8-12).
    EXPECT_EQ(map.zone(9,  9),   ZoneType::city);
    EXPECT_EQ(map.zone(10, 10),  ZoneType::city);
    EXPECT_EQ(map.region(9,  9), RegionType::square);
}

TEST(WorldMap, ProceduralModeIsDeterministic) {
    WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world_procedural.json"));
    WorldMap map1(cfg);
    WorldMap map2(cfg);

    for (int y = 0; y < cfg.grid_h; ++y)
        for (int x = 0; x < cfg.grid_w; ++x)
            EXPECT_EQ(map1.zone(x, y), map2.zone(x, y));
}
