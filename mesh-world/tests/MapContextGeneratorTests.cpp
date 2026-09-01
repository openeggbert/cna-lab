// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M160 — chunk generators read ctx.map_context (elevation, nearest_river)
// instead of relying only on the flat WorldMap / RNG, once map_context.
// available is set. Falls back to pre-M160 behavior exactly when
// map_context.available is false (default-constructed ChunkContext), so
// existing GeneratorTests are unaffected (M163). Mountain/Ocean tweak an
// existing scalar/roll; Forest/Meadow instead append purely-additive extra
// content after every pre-M160 draw, so the base rng sequence is identical
// either way (an equally valid variant of the same "no behavior change when
// unavailable" rule, chosen where scaling an existing loop's trip count
// would have shifted every subsequent draw).

#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>
#include <tinyxml2.h>

#include "BuiltinMaterials.hpp"
#include "CaveLayout.hpp"
#include "generators/ForestGenerator.hpp"
#include "generators/MeadowGenerator.hpp"
#include "generators/MountainGenerator.hpp"
#include "generators/OceanGenerator.hpp"

// MAP17 (2026-07-10) -- the remaining 16 chunk generators, wired to
// map_context the same additive way ForestGenerator/MeadowGenerator already
// were: extra content appended after every pre-MAP17 draw, so the base rng
// sequence (and therefore every existing GeneratorTests assertion) is
// unaffected whether or not map_context is available.
#include "generators/ApartmentBlockGenerator.hpp"
#include "generators/BeachGenerator.hpp"
#include "generators/BridgeGenerator.hpp"
#include "generators/CaveGenerator.hpp"
#include "generators/CrossroadGenerator.hpp"
#include "generators/DesertGenerator.hpp"
#include "generators/EmptyGenerator.hpp"
#include "generators/JungleGenerator.hpp"
#include "generators/ParkGenerator.hpp"
#include "generators/RiverBankGenerator.hpp"
#include "generators/RoadGenerator.hpp"
#include "generators/ShopStreetGenerator.hpp"
#include "generators/SmallHouseBlockGenerator.hpp"
#include "generators/SquareGenerator.hpp"
#include "generators/SwampGenerator.hpp"
#include "generators/TundraGenerator.hpp"

using namespace MeshWorld;

namespace {

MeshWorld::ChunkContext make_ctx(uint64_t seed) {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = seed;
    ctx.chunk_size_m = 64.0f;
    return ctx;
}

// Finds <box id="cliff" ... size="sx sy sz"/> and returns sy.
std::optional<float> cliff_height(const std::string& xml) {
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS) return std::nullopt;
    for (auto* el = doc.RootElement()->FirstChildElement("objects")->FirstChildElement();
         el; el = el->NextSiblingElement()) {
        const char* id = el->Attribute("id");
        if (id && std::string(id) == "cliff") {
            std::istringstream iss(el->Attribute("size"));
            float sx, sy, sz;
            iss >> sx >> sy >> sz;
            return sy;
        }
    }
    return std::nullopt;
}

bool has_rock(const std::string& xml) {
    return xml.find("id=\"rock_0\"") != std::string::npos;
}

int count_matches(const std::string& xml, const std::string& id_prefix) {
    int   count = 0;
    auto  pos   = xml.find(id_prefix);
    while (pos != std::string::npos) {
        ++count;
        pos = xml.find(id_prefix, pos + id_prefix.size());
    }
    return count;
}

} // namespace

class MapContextGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override { MeshWorld::register_builtin_materials(); }
};

// --- MountainGenerator ------------------------------------------------------

TEST_F(MapContextGeneratorTest, MountainCliffHeightUnchangedWithoutMapContext) {
    ChunkContext ctx = make_ctx(42);
    ASSERT_FALSE(ctx.map_context.available);

    MountainGenerator gen;
    const auto ch = cliff_height(gen.generate(ctx));
    ASSERT_TRUE(ch.has_value());
    EXPECT_FLOAT_EQ(*ch, 12.0f);
}

TEST_F(MapContextGeneratorTest, MountainCliffTallerWithHighElevation) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 400.0f;  // 12 + 400*0.005 = 14, below the clamp

    MountainGenerator gen;
    const auto ch = cliff_height(gen.generate(ctx));
    ASSERT_TRUE(ch.has_value());
    EXPECT_FLOAT_EQ(*ch, 14.0f);
}

TEST_F(MapContextGeneratorTest, MountainCliffHeightClampedAtExtremeElevation) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 1'000'000.0f;  // far past the clamp ceiling

    MountainGenerator gen;
    const auto ch = cliff_height(gen.generate(ctx));
    ASSERT_TRUE(ch.has_value());
    EXPECT_FLOAT_EQ(*ch, 20.0f);
}

TEST_F(MapContextGeneratorTest, MountainCliffHeightFloorAtNegativeElevation) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -500.0f;  // below sea level: clamped to the plain 12 m base

    MountainGenerator gen;
    const auto ch = cliff_height(gen.generate(ctx));
    ASSERT_TRUE(ch.has_value());
    EXPECT_FLOAT_EQ(*ch, 12.0f);
}

// --- OceanGenerator ----------------------------------------------------------
// Seeds picked by probing: with map_context unavailable, seed 0's roll spawns
// a rock outcrop; seed 2's roll doesn't. Both are overridden by extreme
// elevation and left alone by elevation in between.

TEST_F(MapContextGeneratorTest, OceanRockUnchangedWithoutMapContext) {
    OceanGenerator gen;
    EXPECT_TRUE(has_rock(gen.generate(make_ctx(0))));
    EXPECT_FALSE(has_rock(gen.generate(make_ctx(2))));
}

TEST_F(MapContextGeneratorTest, OceanRockForcedAbsentInDeepWater) {
    ChunkContext ctx = make_ctx(0);  // seed whose natural roll spawns a rock
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -2000.0f;  // well past the -1000 m deep-water cutoff

    OceanGenerator gen;
    EXPECT_FALSE(has_rock(gen.generate(ctx)));
}

TEST_F(MapContextGeneratorTest, OceanRockForcedPresentInShallowWater) {
    ChunkContext ctx = make_ctx(2);  // seed whose natural roll has no rock
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -10.0f;  // well within the -50 m shallow-water cutoff

    OceanGenerator gen;
    EXPECT_TRUE(has_rock(gen.generate(ctx)));
}

TEST_F(MapContextGeneratorTest, OceanRockUnaffectedAtMidDepth) {
    ChunkContext ctx0 = make_ctx(0);
    ctx0.map_context.available   = true;
    ctx0.map_context.elevation_m = -500.0f;  // between the two cutoffs: natural roll stands
    OceanGenerator gen;
    EXPECT_TRUE(has_rock(gen.generate(ctx0)));

    ChunkContext ctx2 = make_ctx(2);
    ctx2.map_context.available   = true;
    ctx2.map_context.elevation_m = -500.0f;
    EXPECT_FALSE(has_rock(gen.generate(ctx2)));
}

// --- ForestGenerator ----------------------------------------------------------

TEST_F(MapContextGeneratorTest, ForestNoRiverMushroomsWithoutMapContext) {
    ChunkContext ctx = make_ctx(42);
    ASSERT_FALSE(ctx.map_context.available);

    ForestGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_mushroom_"), 0);
}

TEST_F(MapContextGeneratorTest, ForestNoRiverMushroomsWhenNoRiverFound) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available               = true;
    ctx.map_context.nearest_river_distance_m = -1.0f;  // sentinel: no river found

    ForestGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_mushroom_"), 0);
}

TEST_F(MapContextGeneratorTest, ForestExtraMushroomsCloseToRiver) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available               = true;
    ctx.map_context.nearest_river_distance_m = 50.0f;  // within the 80 m "close" band

    ForestGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_mushroom_"), 4);
}

TEST_F(MapContextGeneratorTest, ForestFewerExtraMushroomsFartherFromRiver) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available               = true;
    ctx.map_context.nearest_river_distance_m = 150.0f;  // within 200 m but past the 80 m band

    ForestGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_mushroom_"), 2);
}

TEST_F(MapContextGeneratorTest, ForestNoExtraMushroomsFarFromRiver) {
    ChunkContext ctx = make_ctx(42);
    ctx.map_context.available               = true;
    ctx.map_context.nearest_river_distance_m = 500.0f;  // past the 200 m cutoff

    ForestGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_mushroom_"), 0);
}

// --- MeadowGenerator ----------------------------------------------------------

TEST_F(MapContextGeneratorTest, MeadowNoAlpineRocksWithoutMapContext) {
    ChunkContext ctx = make_ctx(7);
    ASSERT_FALSE(ctx.map_context.available);

    MeadowGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_rock_"), 0);
}

TEST_F(MapContextGeneratorTest, MeadowNoAlpineRocksAtLowElevation) {
    ChunkContext ctx = make_ctx(7);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 200.0f;  // below the 800 m threshold

    MeadowGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_rock_"), 0);
}

TEST_F(MapContextGeneratorTest, MeadowExtraAlpineRocksAboveThreshold) {
    ChunkContext ctx = make_ctx(7);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 1000.0f;  // above 800 m, below the 1500 m "high" band

    MeadowGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_rock_"), 2);
}

TEST_F(MapContextGeneratorTest, MeadowMoreAlpineRocksAtHighElevation) {
    ChunkContext ctx = make_ctx(7);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 2000.0f;  // above the 1500 m "high" band

    MeadowGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_rock_"), 4);
}

// --- MAP17: the remaining 16 generators --------------------------------------

// --- ApartmentBlockGenerator ---

TEST_F(MapContextGeneratorTest, ApartmentNoBikeRacksWithoutMapContext) {
    ChunkContext ctx = make_ctx(1);
    ApartmentBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bikerack_"), 0);
}

TEST_F(MapContextGeneratorTest, ApartmentNoBikeRacksWithoutRoadCrossing) {
    ChunkContext ctx = make_ctx(1);
    ctx.map_context.available = true;
    ApartmentBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bikerack_"), 0);
}

TEST_F(MapContextGeneratorTest, ApartmentBikeRacksAtRoadCrossing) {
    ChunkContext ctx = make_ctx(1);
    ctx.map_context.available          = true;
    ctx.map_context.has_road_crossing  = true;
    ApartmentBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bikerack_"), 2);
}

// --- BeachGenerator ---

TEST_F(MapContextGeneratorTest, BeachNoWrackShellsWithoutMapContext) {
    ChunkContext ctx = make_ctx(2);
    BeachGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "wrack_shell_"), 0);
}

TEST_F(MapContextGeneratorTest, BeachNoWrackShellsAboveTideLine) {
    ChunkContext ctx = make_ctx(2);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 10.0f;
    BeachGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "wrack_shell_"), 0);
}

TEST_F(MapContextGeneratorTest, BeachWrackShellsNearSeaLevel) {
    ChunkContext ctx = make_ctx(2);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 0.5f;
    BeachGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "wrack_shell_"), 3);
}

// --- BridgeGenerator ---

TEST_F(MapContextGeneratorTest, BridgeNoMidSpanPierWithoutMapContext) {
    ChunkContext ctx = make_ctx(3);
    BridgeGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "arch_mid"), 0);
}

TEST_F(MapContextGeneratorTest, BridgeNoMidSpanPierAtLowElevation) {
    ChunkContext ctx = make_ctx(3);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 50.0f;
    BridgeGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "arch_mid"), 0);
}

TEST_F(MapContextGeneratorTest, BridgeMidSpanPierAtHighElevation) {
    ChunkContext ctx = make_ctx(3);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 800.0f;
    BridgeGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "arch_mid"), 1);
}

// --- CaveGenerator ---

TEST_F(MapContextGeneratorTest, CaveNoDeepCrystalsWithoutMapContext) {
    ChunkContext ctx = make_ctx(4);
    CaveGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_crystal_"), 0);
}

TEST_F(MapContextGeneratorTest, CaveNoDeepCrystalsAboveSeaLevel) {
    ChunkContext ctx = make_ctx(4);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 50.0f;
    CaveGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_crystal_"), 0);
}

TEST_F(MapContextGeneratorTest, CaveDeepCrystalsBelowSeaLevel) {
    ChunkContext ctx = make_ctx(4);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -50.0f;
    CaveGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_crystal_"), 4);
}

// M332 (MAP21) -- wall count matches CaveLayout::openings_for() exactly:
// a solid wall is drawn on every side WITHOUT an opening, and omitted on
// every side WITH one. Checked across several (world_seed, coord) pairs so
// this isn't just true for one coincidental combination.
TEST_F(MapContextGeneratorTest, CaveWallPresenceMatchesLayoutOpenings) {
    CaveGenerator gen;
    const std::pair<std::uint64_t, ChunkCoord> cases[] = {
        {1, ChunkCoord{0, 0}},   {2, ChunkCoord{5, -3}},  {3, ChunkCoord{-7, 2}},
        {4, ChunkCoord{10, 10}}, {5, ChunkCoord{-1, -1}}, {6, ChunkCoord{20, -8}},
    };
    for (const auto& [world_seed, coord] : cases) {
        ChunkContext ctx = make_ctx(chunk_seed(world_seed, coord));
        ctx.coord       = coord;
        ctx.world_seed  = world_seed;

        const auto xml      = gen.generate(ctx);
        const auto openings = CaveLayout::openings_for(world_seed, coord);

        EXPECT_EQ(xml.find("id=\"wall_n\"") != std::string::npos, !openings.north)
            << "world_seed=" << world_seed << " coord=(" << coord.x << "," << coord.y << ")";
        EXPECT_EQ(xml.find("id=\"wall_s\"") != std::string::npos, !openings.south)
            << "world_seed=" << world_seed << " coord=(" << coord.x << "," << coord.y << ")";
        EXPECT_EQ(xml.find("id=\"wall_e\"") != std::string::npos, !openings.east)
            << "world_seed=" << world_seed << " coord=(" << coord.x << "," << coord.y << ")";
        EXPECT_EQ(xml.find("id=\"wall_w\"") != std::string::npos, !openings.west)
            << "world_seed=" << world_seed << " coord=(" << coord.x << "," << coord.y << ")";
    }
}

// The rest of the cave (floor/ceiling/stalactites/pool/crystals) doesn't
// consume any rng() draws for wall placement, so omitting walls must not
// shift or otherwise affect that content -- same "purely additive, no
// hidden side effect" discipline every other M160/MAP17 map_context change
// already follows.
TEST_F(MapContextGeneratorTest, CaveNonWallContentUnaffectedByOpenings) {
    CaveGenerator gen;
    ChunkContext sealed = make_ctx(4);
    sealed.coord      = ChunkCoord{0, 0};
    sealed.world_seed = 12345;  // whatever openings this produces, don't care here
    const auto xml = gen.generate(sealed);
    EXPECT_EQ(count_matches(xml, "stalac_"), 14);
    EXPECT_EQ(count_matches(xml, "stalag_"), 10);
    // "crystal_" alone also matches the "crystal_blue" material name inside
    // each <instance>'s own definition attribute (2 hits per instance) --
    // id="crystal_ is the precise count of actual crystal instances.
    EXPECT_EQ(count_matches(xml, "id=\"crystal_"), 5);
    EXPECT_NE(xml.find("id=\"ceiling\""), std::string::npos);
    EXPECT_NE(xml.find("id=\"pool\""), std::string::npos);
}

// M335 (MAP21) -- rock rubble is genuinely new content (unlike stalactites/
// stalagmites/crystals above, which already existed pre-M333/335), added to
// generate()'s own inline output too, not just placements() -- same "new
// content goes in both channels" precedent MAP20's M326 (coral/kelp)
// established. Purely additive at the very end of generate(), so it must
// not affect any of the counts the tests above already assert.
TEST_F(MapContextGeneratorTest, CaveGenerateIncludesRockRubble) {
    CaveGenerator gen;
    ChunkContext ctx = make_ctx(4);
    ctx.coord      = ChunkCoord{0, 0};
    ctx.world_seed = 4;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"rubble_"), 8);
}

// M336/M337 (MAP21) -- surface-entrance ceiling geometry and ambient-light
// metadata must always agree with each other: whenever the "cave" metadata
// object declares hasSurfaceEntrance=true, the XML has the 4-plane
// "ceiling_*" frame (never the single solid "id=\"ceiling\"" plane) and
// ambientLightLevel is spliced in as a number; whenever it declares false,
// it's the other way around. Checked without map_context (must always be
// false/no entrance) and at several elevations relative to the mountain
// threshold (2500m) to cover both sides of the tapering probability.
TEST_F(MapContextGeneratorTest, CaveWithoutMapContextNeverHasASurfaceEntrance) {
    CaveGenerator gen;
    ChunkContext ctx = make_ctx(7);
    ctx.coord      = ChunkCoord{3, -2};
    ctx.world_seed = 7;
    const auto xml = gen.generate(ctx);
    EXPECT_NE(xml.find("id=\"ceiling\""), std::string::npos);
    EXPECT_EQ(xml.find("id=\"ceiling_n\""), std::string::npos);
    EXPECT_NE(xml.find("\"hasSurfaceEntrance\": false"), std::string::npos);
}

TEST_F(MapContextGeneratorTest, CaveCeilingGeometryAlwaysAgreesWithMetadataFlag) {
    CaveGenerator gen;
    const std::pair<std::uint64_t, ChunkCoord> cases[] = {
        {1, ChunkCoord{0, 0}},   {2, ChunkCoord{5, -3}},  {3, ChunkCoord{-7, 2}},
        {4, ChunkCoord{10, 10}}, {5, ChunkCoord{-1, -1}}, {6, ChunkCoord{20, -8}},
        {8, ChunkCoord{2, 9}},   {9, ChunkCoord{-4, 6}},
    };
    const float elevations[] = {2400.0f, 2500.0f, 2700.0f, 3200.0f, 4500.0f};
    for (const auto& [world_seed, coord] : cases) {
        for (float elevation : elevations) {
            ChunkContext ctx = make_ctx(chunk_seed(world_seed, coord));
            ctx.coord                   = coord;
            ctx.world_seed               = world_seed;
            ctx.map_context.available   = true;
            ctx.map_context.elevation_m = elevation;

            const auto xml = gen.generate(ctx);
            const bool has_frame = xml.find("id=\"ceiling_n\"") != std::string::npos;
            const bool has_solid = xml.find("id=\"ceiling\"") != std::string::npos;
            const bool metadata_true  = xml.find("\"hasSurfaceEntrance\": true") != std::string::npos;
            const bool metadata_false = xml.find("\"hasSurfaceEntrance\": false") != std::string::npos;

            EXPECT_NE(has_frame, has_solid)
                << "exactly one ceiling representation, world_seed=" << world_seed
                << " elevation=" << elevation;
            EXPECT_EQ(has_frame, metadata_true)
                << "world_seed=" << world_seed << " elevation=" << elevation;
            EXPECT_EQ(has_solid, metadata_false)
                << "world_seed=" << world_seed << " elevation=" << elevation;
            EXPECT_NE(xml.find("\"ambientLightLevel\": "), std::string::npos);
        }
    }
}

// M338 (MAP21) -- cave structure generation (walls, ceiling, entrance,
// ambient-light metadata, all of it) is fully deterministic: the same
// ChunkContext must produce byte-identical output across repeated calls,
// including the M336/M337 additions (their own RNG streams are freshly
// seeded from ctx.seed/ctx.world_seed every call, not from any process-
// global state).
TEST_F(MapContextGeneratorTest, CaveStructureGenerationIsFullyDeterministic) {
    CaveGenerator gen;
    ChunkContext ctx = make_ctx(chunk_seed(11, ChunkCoord{4, -6}));
    ctx.coord                   = ChunkCoord{4, -6};
    ctx.world_seed               = 11;
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 2650.0f;

    const auto first  = gen.generate(ctx);
    const auto second = gen.generate(ctx);
    EXPECT_EQ(first, second);
}

// M339 (MAP21) -- entrance probability tapers off with elevation above the
// mountain threshold: chunks close to it should have a surface entrance far
// more often than chunks deep inside a much taller massif, across a large
// sample of independent seeds. This is a statistical property, not a
// per-chunk guarantee (CaveLayout.hpp's own doc comment already establishes
// that neither this function nor CaveLayout can see a neighbor's real
// content, so "every chamber reachable from an entrance" cannot be proven
// as a hard per-chunk fact -- this test verifies the honest, achievable
// claim: entrances exist, and they cluster where the design intends).
TEST_F(MapContextGeneratorTest, CaveEntranceLikelihoodTapersWithElevationAboveThreshold) {
    CaveGenerator gen;
    auto entrance_fraction = [&](float elevation_m) {
        int entrances = 0;
        constexpr int kTrials = 200;
        for (int i = 0; i < kTrials; ++i) {
            ChunkContext ctx = make_ctx(static_cast<std::uint64_t>(1000 + i));
            ctx.coord                   = ChunkCoord{i, -i};
            ctx.world_seed               = static_cast<std::uint64_t>(1000 + i);
            ctx.map_context.available   = true;
            ctx.map_context.elevation_m = elevation_m;
            if (gen.generate(ctx).find("id=\"ceiling_n\"") != std::string::npos)
                ++entrances;
        }
        return static_cast<double>(entrances) / kTrials;
    };

    const double near_threshold = entrance_fraction(2550.0f);   // just above 2500m
    const double deep_massif    = entrance_fraction(6000.0f);   // far above it

    EXPECT_GT(near_threshold, 0.5) << "near-threshold chunks should mostly have an entrance";
    EXPECT_LT(deep_massif, 0.3) << "deep chunks should rarely have an entrance";
    EXPECT_GT(near_threshold, deep_massif);
}

// M339 (MAP21) -- placement density: CaveGenerator's total emitted element
// count (walls/ceiling/floor/stalactites/stalagmites/pool/crystals/rubble,
// including M336's own ceiling-frame planes) must stay within this
// generator's own GeneratorConstraints::max_objects budget (default 80) --
// a real check of a field that nothing enforced before this test.
TEST_F(MapContextGeneratorTest, CaveElementCountStaysWithinDefaultObjectBudget) {
    CaveGenerator gen;
    ChunkContext ctx = make_ctx(chunk_seed(3, ChunkCoord{0, 0}));
    ctx.coord                   = ChunkCoord{0, 0};
    ctx.world_seed               = 3;
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -50.0f;  // also triggers the deep_crystal extras -- worst case

    const auto xml = gen.generate(ctx);
    const int element_count = count_matches(xml, "id=\"");
    EXPECT_LE(element_count, ctx.constraints.max_objects);
    EXPECT_GT(element_count, 0);
}

// --- CrossroadGenerator ---

TEST_F(MapContextGeneratorTest, CrossroadNoSignpostWithoutMapContext) {
    ChunkContext ctx = make_ctx(5);
    CrossroadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"signpost\""), 0);
}

TEST_F(MapContextGeneratorTest, CrossroadNoSignpostWithoutNearestPlace) {
    ChunkContext ctx = make_ctx(5);
    ctx.map_context.available = true;
    CrossroadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"signpost\""), 0);
}

TEST_F(MapContextGeneratorTest, CrossroadSignpostNearNamedPlace) {
    ChunkContext ctx = make_ctx(5);
    ctx.map_context.available          = true;
    ctx.map_context.nearest_place_name = "Vorhavn";
    CrossroadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"signpost\""), 1);
}

// --- DesertGenerator ---

TEST_F(MapContextGeneratorTest, DesertNoOasisScrubWithoutMapContext) {
    ChunkContext ctx = make_ctx(6);
    DesertGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "oasis_scrub_"), 0);
}

TEST_F(MapContextGeneratorTest, DesertNoOasisScrubFarFromRiver) {
    ChunkContext ctx = make_ctx(6);
    ctx.map_context.available                = true;
    ctx.map_context.nearest_river_distance_m  = 500.0f;
    DesertGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "oasis_scrub_"), 0);
}

TEST_F(MapContextGeneratorTest, DesertOasisScrubNearRiver) {
    ChunkContext ctx = make_ctx(6);
    ctx.map_context.available                = true;
    ctx.map_context.nearest_river_distance_m  = 40.0f;
    DesertGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "oasis_scrub_"), 3);
}

// --- EmptyGenerator ---

TEST_F(MapContextGeneratorTest, EmptyPlainDirtWithoutMapContext) {
    ChunkContext ctx = make_ctx(8);
    EmptyGenerator gen;
    const std::string xml = gen.generate(ctx);
    EXPECT_NE(xml.find("dirt"), std::string::npos);
    EXPECT_EQ(xml.find("swamp_mud"), std::string::npos);
    EXPECT_EQ(xml.find("rock_snow_covered"), std::string::npos);
}

TEST_F(MapContextGeneratorTest, EmptyMudBelowSeaLevel) {
    ChunkContext ctx = make_ctx(8);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = -20.0f;
    EmptyGenerator gen;
    EXPECT_NE(gen.generate(ctx).find("swamp_mud"), std::string::npos);
}

TEST_F(MapContextGeneratorTest, EmptySnowRockAtHighElevation) {
    ChunkContext ctx = make_ctx(8);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 2000.0f;
    EmptyGenerator gen;
    EXPECT_NE(gen.generate(ctx).find("rock_snow_covered"), std::string::npos);
}

TEST_F(MapContextGeneratorTest, EmptyPlainDirtAtModerateElevationWithMapContext) {
    ChunkContext ctx = make_ctx(8);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 100.0f;
    EmptyGenerator gen;
    const std::string xml = gen.generate(ctx);
    EXPECT_NE(xml.find("dirt"), std::string::npos);
    EXPECT_EQ(xml.find("swamp_mud"), std::string::npos);
    EXPECT_EQ(xml.find("rock_snow_covered"), std::string::npos);
}

// --- JungleGenerator ---

TEST_F(MapContextGeneratorTest, JungleNoRiverVinesWithoutMapContext) {
    ChunkContext ctx = make_ctx(9);
    JungleGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_vine_"), 0);
}

TEST_F(MapContextGeneratorTest, JungleNoRiverVinesFarFromRiver) {
    ChunkContext ctx = make_ctx(9);
    ctx.map_context.available                = true;
    ctx.map_context.nearest_river_distance_m  = 300.0f;
    JungleGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_vine_"), 0);
}

TEST_F(MapContextGeneratorTest, JungleRiverVinesNearRiver) {
    ChunkContext ctx = make_ctx(9);
    ctx.map_context.available                = true;
    ctx.map_context.nearest_river_distance_m  = 30.0f;
    JungleGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "river_vine_"), 4);
}

// --- ParkGenerator ---

TEST_F(MapContextGeneratorTest, ParkNoPlaceBenchesWithoutMapContext) {
    ChunkContext ctx = make_ctx(10);
    ParkGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bench_place_"), 0);
}

TEST_F(MapContextGeneratorTest, ParkNoPlaceBenchesWithoutNearestPlace) {
    ChunkContext ctx = make_ctx(10);
    ctx.map_context.available = true;
    ParkGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bench_place_"), 0);
}

TEST_F(MapContextGeneratorTest, ParkPlaceBenchesNearNamedPlace) {
    ChunkContext ctx = make_ctx(10);
    ctx.map_context.available          = true;
    ctx.map_context.nearest_place_name = "Vorhavn";
    ParkGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bench_place_"), 2);
}

// --- RiverBankGenerator ---

TEST_F(MapContextGeneratorTest, RiverBankNoLushWillowsWithoutMapContext) {
    ChunkContext ctx = make_ctx(11);
    RiverBankGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "lush_willow_"), 0);
}

TEST_F(MapContextGeneratorTest, RiverBankNoLushWillowsAtHighElevation) {
    ChunkContext ctx = make_ctx(11);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 500.0f;
    RiverBankGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "lush_willow_"), 0);
}

TEST_F(MapContextGeneratorTest, RiverBankLushWillowsAtLowElevation) {
    ChunkContext ctx = make_ctx(11);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 10.0f;
    RiverBankGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "lush_willow_"), 3);
}

// T234 -- reeds along the waterline + scattered stones on the grass side.
TEST_F(MapContextGeneratorTest, RiverBankHasReedsAndStones) {
    ChunkContext ctx = make_ctx(11);
    RiverBankGenerator gen;
    const std::string xml = gen.generate(ctx);
    EXPECT_EQ(count_matches(xml, "reed_"), 10);
    // `id="stone_` (not just "stone_") -- this generator's own embankment
    // material is literally named "stone_embank", which would otherwise
    // inflate a bare "stone_" substring count by 1.
    EXPECT_EQ(count_matches(xml, R"(id="stone_)"), 6);
    EXPECT_NE(xml.find("plant_marsh_grass"), std::string::npos);
}

// --- RoadGenerator ---

TEST_F(MapContextGeneratorTest, RoadNoGuardrailsWithoutMapContext) {
    ChunkContext ctx = make_ctx(12);
    ctx.exits.north_road = ctx.exits.south_road = true;
    RoadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "guardrail_"), 0);
}

TEST_F(MapContextGeneratorTest, RoadNoGuardrailsAtLowElevation) {
    ChunkContext ctx = make_ctx(12);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 50.0f;
    RoadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "guardrail_"), 0);
}

TEST_F(MapContextGeneratorTest, RoadGuardrailsAtHighElevation) {
    ChunkContext ctx = make_ctx(12);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 800.0f;
    RoadGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "guardrail_"), 8);
}

// --- ShopStreetGenerator ---

TEST_F(MapContextGeneratorTest, ShopStreetNoPlaceBenchesWithoutMapContext) {
    ChunkContext ctx = make_ctx(13);
    ShopStreetGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bench_place_"), 0);
}

TEST_F(MapContextGeneratorTest, ShopStreetPlaceBenchesNearNamedPlace) {
    ChunkContext ctx = make_ctx(13);
    ctx.map_context.available          = true;
    ctx.map_context.nearest_place_name = "Vorhavn";
    ShopStreetGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "bench_place_"), 2);
}

// --- SmallHouseBlockGenerator ---

TEST_F(MapContextGeneratorTest, SmallHouseNoSnowguardsWithoutMapContext) {
    ChunkContext ctx = make_ctx(14);
    SmallHouseBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "snowguard_"), 0);
}

TEST_F(MapContextGeneratorTest, SmallHouseNoSnowguardsAtLowElevation) {
    ChunkContext ctx = make_ctx(14);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 100.0f;
    SmallHouseBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "snowguard_"), 0);
}

TEST_F(MapContextGeneratorTest, SmallHouseSnowguardsAtHighElevation) {
    ChunkContext ctx = make_ctx(14);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 1200.0f;
    SmallHouseBlockGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "snowguard_"), 4);
}

// --- SquareGenerator ---

TEST_F(MapContextGeneratorTest, SquareNoMonumentWithoutMapContext) {
    ChunkContext ctx = make_ctx(15);
    SquareGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"monument\""), 0);
}

TEST_F(MapContextGeneratorTest, SquareMonumentNearNamedPlace) {
    ChunkContext ctx = make_ctx(15);
    ctx.map_context.available          = true;
    ctx.map_context.nearest_place_name = "Vorhavn";
    SquareGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "id=\"monument\""), 1);
}

// --- SwampGenerator ---

TEST_F(MapContextGeneratorTest, SwampNoDeepPoolsWithoutMapContext) {
    ChunkContext ctx = make_ctx(16);
    SwampGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_pool_"), 0);
}

TEST_F(MapContextGeneratorTest, SwampNoDeepPoolsAboveThreshold) {
    ChunkContext ctx = make_ctx(16);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 50.0f;
    SwampGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_pool_"), 0);
}

TEST_F(MapContextGeneratorTest, SwampDeepPoolsNearSeaLevel) {
    ChunkContext ctx = make_ctx(16);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 5.0f;
    SwampGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "deep_pool_"), 3);
}

// --- TundraGenerator ---

TEST_F(MapContextGeneratorTest, TundraNoAlpineBouldersWithoutMapContext) {
    ChunkContext ctx = make_ctx(17);
    TundraGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_boulder_"), 0);
}

TEST_F(MapContextGeneratorTest, TundraNoAlpineBouldersAtLowElevation) {
    ChunkContext ctx = make_ctx(17);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 200.0f;
    TundraGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_boulder_"), 0);
}

TEST_F(MapContextGeneratorTest, TundraAlpineBouldersAboveThreshold) {
    ChunkContext ctx = make_ctx(17);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 1500.0f;
    TundraGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_boulder_"), 3);
}

TEST_F(MapContextGeneratorTest, TundraMoreAlpineBouldersAtExtremeElevation) {
    ChunkContext ctx = make_ctx(17);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 2500.0f;
    TundraGenerator gen;
    EXPECT_EQ(count_matches(gen.generate(ctx), "alpine_boulder_"), 6);
}
