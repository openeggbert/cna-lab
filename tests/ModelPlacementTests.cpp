// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP11 tests. M167: include/ModelPlacement.hpp -- the placement struct.
// M168: ChunkGenerator::placements() -- ModelPlacements for streamable
// objects, in addition to (not replacing) generate()'s inline MC3
// instances. ForestGenerator is the first generator slice. M170: placement
// altitude -- y_min/y_max from an approximate per-definition height lookup
// (ObjectBoundingBox.hpp) + ground elevation.

#include <gtest/gtest.h>

#include <set>
#include <string>

#include "ChunkGenerator.hpp"
#include "ModelPlacement.hpp"
#include "ObjectBoundingBox.hpp"
#include "generators/BeachGenerator.hpp"
#include "generators/CaveGenerator.hpp"
#include "generators/DesertGenerator.hpp"
#include "generators/EmptyGenerator.hpp"
#include "generators/ForestGenerator.hpp"
#include "generators/JungleGenerator.hpp"
#include "generators/MeadowGenerator.hpp"
#include "generators/MountainGenerator.hpp"
#include "generators/OceanGenerator.hpp"
#include "generators/SwampGenerator.hpp"
#include "generators/TundraGenerator.hpp"
#include "WorldRenderer.hpp"

using namespace MeshWorld;

namespace {

ChunkContext make_ctx(uint64_t seed, ChunkCoord coord = {0, 0}) {
    ChunkContext ctx;
    ctx.coord        = coord;
    ctx.seed         = seed;
    ctx.chunk_size_m = 64.0f;
    return ctx;
}

// MAP20 (M319-328) -- shared checks every non-Forest placements() override
// below must satisfy: every definition_id is one of the generator's own
// recognized ids, every placement's altitude extent is positive and matches
// object_height_m(), positions stay within this chunk's world-space bounds,
// and the same seed reproduces byte-identical placements. Mirrors
// ForestGeneratorPlacementsTest's own individual tests (EveryPlacementIsA
// RecognizedTreeDefinition/EveryPlacementHasAPositiveAltitudeExtent/
// PositionsAreWorldSpaceWithinThisChunksBounds/IsDeterministicForTheSameSeed),
// combined into one helper since MAP20 adds 7 more generators needing the
// exact same checks.
template <typename Generator>
void expect_valid_deterministic_bounded_placements(const std::set<std::string>& valid_ids) {
    Generator gen;
    const ChunkCoord coord{3, -2};
    const ChunkContext ctx = make_ctx(9, coord);
    const double origin_x = ctx.coord.world_x(static_cast<int>(ctx.chunk_size_m));
    const double origin_z = ctx.coord.world_z(static_cast<int>(ctx.chunk_size_m));

    for (const auto& p : gen.placements(ctx)) {
        EXPECT_NE(valid_ids.find(p.definition_id), valid_ids.end()) << p.definition_id;
        EXPECT_GT(p.y_max - p.y_min, 0.0) << p.definition_id;
        EXPECT_DOUBLE_EQ(p.y_max - p.y_min, object_height_m(p.definition_id));
        // M328 -- every placements() override must derive lod_min from this
        // definition's own real height, not leave it at a fixed 0.
        EXPECT_EQ(p.lod_min, lod_tier_for_height(object_height_m(p.definition_id)))
            << p.definition_id;
        EXPECT_GE(p.pos_x, origin_x);
        EXPECT_LE(p.pos_x, origin_x + ctx.chunk_size_m);
        EXPECT_GE(p.pos_z, origin_z);
        EXPECT_LE(p.pos_z, origin_z + ctx.chunk_size_m);
    }

    const auto first  = gen.placements(make_ctx(123));
    const auto second = gen.placements(make_ctx(123));
    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        EXPECT_EQ(first[i].definition_id, second[i].definition_id);
        EXPECT_EQ(first[i].pos_x, second[i].pos_x);
        EXPECT_EQ(first[i].pos_z, second[i].pos_z);
        EXPECT_EQ(first[i].rot_y, second[i].rot_y);
        EXPECT_EQ(first[i].scale, second[i].scale);
        EXPECT_EQ(first[i].lod_min, second[i].lod_min);
    }
}

// M168's own "purely additive" invariant, checked generically: generate()'s
// own inline MC3 output must be unaffected by placements() existing --
// calling generate() twice must still produce byte-identical output (the
// same check GenerateOutputIsUnaffectedByPlacementsExisting makes for
// ForestGenerator, minus its own generator-specific <instance> count, which
// doesn't generalize across generators with different content).
template <typename Generator>
void expect_generate_is_deterministic(uint64_t seed = 55) {
    Generator gen;
    const ChunkContext ctx = make_ctx(seed);
    const std::string first  = gen.generate(ctx);
    const std::string second = gen.generate(ctx);
    EXPECT_EQ(first, second);
}

} // namespace

TEST(ModelPlacementTest, DefaultConstructedPlacementHasSensibleDefaults) {
    ModelPlacement p;
    EXPECT_TRUE(p.definition_id.empty());
    EXPECT_EQ(p.pos_x, 0.0);
    EXPECT_EQ(p.pos_y, 0.0);
    EXPECT_EQ(p.pos_z, 0.0);
    EXPECT_EQ(p.y_min, 0.0);
    EXPECT_EQ(p.y_max, 0.0);
    EXPECT_EQ(p.rot_y, 0.0f);
    EXPECT_EQ(p.scale, 1.0f);
    EXPECT_EQ(p.lod_min, 0);
}

TEST(ChunkGeneratorPlacementsTest, DefaultImplementationReturnsEmpty) {
    EmptyGenerator gen;
    const ChunkContext ctx = make_ctx(1);
    EXPECT_TRUE(gen.placements(ctx).empty());
}

TEST(ForestGeneratorPlacementsTest, ProducesExactlyThirtyTwoTreePlacements) {
    ForestGenerator gen;
    const auto placements = gen.placements(make_ctx(42));
    EXPECT_EQ(placements.size(), 32u);
}

TEST(ForestGeneratorPlacementsTest, EveryPlacementIsARecognizedTreeDefinition) {
    ForestGenerator gen;
    const std::set<std::string> valid_trees{"tree_oak", "tree_pine", "tree_birch", "tree_beech"};
    for (const auto& p : gen.placements(make_ctx(7))) {
        EXPECT_NE(valid_trees.find(p.definition_id), valid_trees.end()) << p.definition_id;
    }
}

TEST(ForestGeneratorPlacementsTest, IsDeterministicForTheSameSeed) {
    ForestGenerator gen;
    const auto first = gen.placements(make_ctx(123));
    const auto second = gen.placements(make_ctx(123));
    ASSERT_EQ(first.size(), second.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        EXPECT_EQ(first[i].definition_id, second[i].definition_id);
        EXPECT_EQ(first[i].pos_x, second[i].pos_x);
        EXPECT_EQ(first[i].pos_z, second[i].pos_z);
        EXPECT_EQ(first[i].rot_y, second[i].rot_y);
        EXPECT_EQ(first[i].scale, second[i].scale);
    }
}

TEST(ForestGeneratorPlacementsTest, DifferentSeedsProduceDifferentPlacements) {
    ForestGenerator gen;
    const auto a = gen.placements(make_ctx(1));
    const auto b = gen.placements(make_ctx(2));
    ASSERT_EQ(a.size(), b.size());

    bool any_different = false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].pos_x != b[i].pos_x || a[i].pos_z != b[i].pos_z) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST(ForestGeneratorPlacementsTest, PositionsAreWorldSpaceWithinThisChunksBounds) {
    ForestGenerator gen;
    const ChunkCoord coord{3, -2};
    const ChunkContext ctx = make_ctx(9, coord);
    const double origin_x = ctx.coord.world_x(static_cast<int>(ctx.chunk_size_m));
    const double origin_z = ctx.coord.world_z(static_cast<int>(ctx.chunk_size_m));

    for (const auto& p : gen.placements(ctx)) {
        EXPECT_GE(p.pos_x, origin_x);
        EXPECT_LE(p.pos_x, origin_x + ctx.chunk_size_m);
        EXPECT_GE(p.pos_z, origin_z);
        EXPECT_LE(p.pos_z, origin_z + ctx.chunk_size_m);
    }
}

TEST(ForestGeneratorPlacementsTest, GenerateOutputIsUnaffectedByPlacementsExisting) {
    // M168 is purely additive -- generate()'s own inline MC3 output must be
    // byte-for-byte identical to before placements() existed.
    ForestGenerator gen;
    const ChunkContext ctx = make_ctx(55);
    const std::string first = gen.generate(ctx);
    const std::string second = gen.generate(ctx);
    EXPECT_EQ(first, second);

    int instance_count = 0;
    std::size_t pos = 0;
    while ((pos = first.find("<instance", pos)) != std::string::npos) {
        ++instance_count;
        pos += 1;
    }
    // 32 trees + 6 mushrooms + 10 T226 tree-cluster instances (2 clusters x
    // 5 trees each) = 48 <instance> elements (river_mushroom extras only
    // appear when map_context.available, which make_ctx() leaves false;
    // T227-229's bushes/clearing/path use <cylinder>/<plane>, not
    // <instance>, so they don't affect this count).
    EXPECT_EQ(instance_count, 48);
}

TEST(ForestGeneratorPlacementsTest, EveryPlacementHasAPositiveAltitudeExtent) {
    ForestGenerator gen;
    for (const auto& p : gen.placements(make_ctx(3))) {
        EXPECT_GT(p.y_max - p.y_min, 0.0) << p.definition_id;
        EXPECT_DOUBLE_EQ(p.y_max - p.y_min, object_height_m(p.definition_id));
    }
}

TEST(ForestGeneratorPlacementsTest, WithoutMapContextGroundElevationDefaultsToZero) {
    ForestGenerator gen;
    ChunkContext ctx = make_ctx(3);
    ASSERT_FALSE(ctx.map_context.available);
    for (const auto& p : gen.placements(ctx)) {
        EXPECT_DOUBLE_EQ(p.pos_y, 0.0);
        EXPECT_DOUBLE_EQ(p.y_min, 0.0);
    }
}

TEST(ForestGeneratorPlacementsTest, MapContextElevationShiftsPositionAndAltitudeExtentTogether) {
    ForestGenerator gen;
    ChunkContext ctx = make_ctx(3);
    ctx.map_context.available = true;
    ctx.map_context.elevation_m = 250.0f;

    for (const auto& p : gen.placements(ctx)) {
        EXPECT_DOUBLE_EQ(p.pos_y, 250.0);
        EXPECT_DOUBLE_EQ(p.y_min, 250.0);
        EXPECT_DOUBLE_EQ(p.y_max, 250.0 + object_height_m(p.definition_id));
    }
}

TEST(ObjectBoundingBoxTest, KnownTreeDefinitionsHaveDistinctPositiveHeights) {
    for (const char* id : {"tree_oak", "tree_pine", "tree_birch", "tree_beech"}) {
        EXPECT_GT(object_height_m(id), 0.0f) << id;
    }
}

TEST(ObjectBoundingBoxTest, UnknownDefinitionGetsAConservativeDefault) {
    EXPECT_GT(object_height_m("some_unlisted_definition"), 0.0f);
}

// ---------------------------------------------------------------------------
// M327 -- object_height_m() now derives real heights by walking
// ObjectDefinitionLibrary geometry instead of an all-hardcoded lookup table.
// These hand-verified exact values (computed from each definition's own
// Mc3Object primitives/transforms in ObjectDefinitionLibrary.cpp) catch a
// regression in the geometry-walking logic itself (ObjectBoundingBox.cpp's
// object_y_extent()), not just "returns something positive".
// ---------------------------------------------------------------------------

TEST(ObjectBoundingBoxTest, ComputesExactGeometryDerivedHeightForSimpleStackedShape) {
    // cactus_saguaro = a 2.2 m cylinder body + two 0.8 m arm boxes centered
    // at 1.0-1.8 m -- the arms never exceed the body's own top.
    EXPECT_FLOAT_EQ(object_height_m("cactus_saguaro"), 2.2f);
}

TEST(ObjectBoundingBoxTest, ComputesExactGeometryDerivedHeightForThreeStackedBoxes) {
    // rock_pile_small = three boxes stacked with increasing base_y offsets;
    // the topmost (r2, base_y=0.50, sy=0.22) box's own top (0.50+0.11=0.72 m)
    // is the tallest point.
    EXPECT_FLOAT_EQ(object_height_m("rock_pile_small"), 0.72f);
}

TEST(ObjectBoundingBoxTest, AccountsForChildScaleYOnADeformedIcoSphere) {
    // tree_birch's canopy icosphere has transform.scale.y = 1.55 (birch_tree()'s
    // "tall narrow icosphere canopy" deform) -- without applying that scale,
    // the computed height would be short by radius*(1.55-1.0) = 1.1 m.
    EXPECT_FLOAT_EQ(object_height_m("tree_birch"), 8.95f);
}

TEST(ObjectBoundingBoxTest, ComputesExactGeometryDerivedHeightForMultiTierCanopy) {
    // tree_oak = a 4.0 m trunk + two icosphere canopy tiers; the upper tier
    // (canopy_top, center_y=8.12, radius=1.68) is the tallest point.
    EXPECT_FLOAT_EQ(object_height_m("tree_oak"), 9.8f);
}

TEST(ObjectBoundingBoxTest, ComputesExactGeometryDerivedHeightForTheNewAquaticDefinitions) {
    // MAP20, M326's own coral_branching/kelp_strand -- verifies the
    // geometry-walking logic generalizes to definitions M327 never
    // specifically hand-tuned a fallback height for.
    EXPECT_FLOAT_EQ(object_height_m("coral_branching"), 0.5f);
    EXPECT_FLOAT_EQ(object_height_m("kelp_strand"), 3.68f);
}

TEST(ObjectBoundingBoxTest, RegisteredDefinitionsHaveMeaningfullyDistinctHeights) {
    // A regression guard against object_height_m() silently collapsing to
    // the same conservative default for everything (which would make all of
    // these equal).
    const float oak    = object_height_m("tree_oak");
    const float birch  = object_height_m("tree_birch");
    const float palm   = object_height_m("tree_palm");
    const float cactus = object_height_m("cactus_saguaro");
    EXPECT_NE(oak, birch);
    EXPECT_NE(oak, palm);
    EXPECT_NE(birch, cactus);
}

TEST(ObjectBoundingBoxTest, UnknownDefinitionFallsBackToTheConservativeDefault) {
    // Real bug fix, 2026-07-11 (T-series backlog triage): "tree_pine" used
    // to be this test's own example of a definition_id that
    // ChunkGenerator::placements() referenced but ObjectDefinitionLibrary
    // never registered under that exact name -- a real, currently-
    // invisible-in-game bug (ForestGenerator's own w.instance() calls),
    // not a naming quirk to paper over with a fallback height table. Fixed
    // at the source (ObjectDefinitionLibrary::load_all() now registers it,
    // along with 7 other IDs the same triage found broken the same way --
    // see ObjectBoundingBox.cpp's own doc comment on object_height_m()),
    // so the fallback table this test used to exercise was removed
    // entirely. This test now covers what's actually left: a genuinely
    // unknown ID (never registered, never will be) still returns the
    // conservative default rather than crashing or returning 0.
    EXPECT_FLOAT_EQ(object_height_m("totally_unregistered_definition_xyz"), 2.0f);
}

// ---------------------------------------------------------------------------
// MAP20 (M319-M325) -- ModelPlacements for the remaining biome generators,
// each mirroring ForestGenerator's own M168 pattern. See each generator's
// own placements() doc comment (src/generators/*.cpp) for exactly which of
// generate()'s instance() calls were converted and which weren't (only
// w.instance() calls become placements; raw MC3 primitives like w.box()/
// w.plane()/w.cylinder() have no ObjectDefinitionLibrary definition_id).
// ---------------------------------------------------------------------------

TEST(JungleGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    JungleGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 50u);
}

TEST(JungleGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<JungleGenerator>(
        {"tree_palm", "tree_banyan", "tree_tropical_fern", "tree_bamboo", "rock_mossy"});
}

TEST(JungleGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<JungleGenerator>();
}

TEST(DesertGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    DesertGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 12u);
}

TEST(DesertGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<DesertGenerator>(
        {"cactus_saguaro", "plant_desert_scrub"});
}

TEST(DesertGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<DesertGenerator>();
}

TEST(MountainGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    MountainGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 13u);
}

TEST(MountainGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<MountainGenerator>(
        {"rock_pile_small", "tree_pine_mountain"});
}

TEST(MountainGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<MountainGenerator>();
}

TEST(TundraGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    TundraGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 14u);
}

TEST(TundraGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<TundraGenerator>(
        {"tree_bare_winter", "plant_lichen"});
}

TEST(TundraGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<TundraGenerator>();
}

TEST(SwampGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    SwampGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 32u);
}

TEST(SwampGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<SwampGenerator>(
        {"tree_dead_gnarled", "plant_marsh_grass"});
}

TEST(SwampGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<SwampGenerator>();
}

TEST(MeadowGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    MeadowGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 62u);
}

TEST(MeadowGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<MeadowGenerator>(
        {"flower_poppy", "flower_daisy", "flower_bluebell", "flower_buttercup",
         "tree_oak", "grass_tuft_tall", "rock_grey_small"});
}

TEST(MeadowGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<MeadowGenerator>();
}

TEST(BeachGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    BeachGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 16u);
}

TEST(BeachGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<BeachGenerator>(
        {"shell_seashell", "plant_sea_grass", "tree_palm"});
}

TEST(BeachGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<BeachGenerator>();
}

// OceanGenerator's placement count is not fixed (spawn_rock is a 1-in-4 roll,
// and coral/kelp only appear for their own ctx.zone), so it gets bespoke
// zone-conditioned tests instead of the ProducesExpectedPlacementCount
// pattern the other 7 MAP20 generators use.
TEST(OceanGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<OceanGenerator>(
        {"plant_sea_weed", "coral_branching", "kelp_strand"});
}

TEST(OceanGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<OceanGenerator>();
}

TEST(OceanGeneratorPlacementsTest, CoralReefZoneProducesAtLeastTenCoralPlacements) {
    OceanGenerator gen;
    ChunkContext ctx = make_ctx(4);
    ctx.zone = ZoneType::coral_reef;
    int coral_count = 0;
    for (const auto& p : gen.placements(ctx))
        if (p.definition_id == "coral_branching") ++coral_count;
    EXPECT_GE(coral_count, 10);
}

TEST(OceanGeneratorPlacementsTest, KelpForestZoneProducesAtLeastFourteenKelpPlacements) {
    OceanGenerator gen;
    ChunkContext ctx = make_ctx(4);
    ctx.zone = ZoneType::kelp_forest;
    int kelp_count = 0;
    for (const auto& p : gen.placements(ctx))
        if (p.definition_id == "kelp_strand") ++kelp_count;
    EXPECT_GE(kelp_count, 14);
}

TEST(OceanGeneratorPlacementsTest, DefaultOceanZoneNeverProducesCoralOrKelp) {
    OceanGenerator gen;
    for (uint64_t seed = 1; seed <= 10; ++seed) {
        ChunkContext ctx = make_ctx(seed);  // ctx.zone defaults to ZoneType::empty
        for (const auto& p : gen.placements(ctx)) {
            EXPECT_NE(p.definition_id, "coral_branching") << "seed=" << seed;
            EXPECT_NE(p.definition_id, "kelp_strand") << "seed=" << seed;
        }
    }
}

// ---------------------------------------------------------------------------
// M328 -- lod_tier_for_height() and its use across every placements()
// override (the per-placement EXPECT_EQ(p.lod_min, lod_tier_for_height(...))
// check inside expect_valid_deterministic_bounded_placements() above already
// covers all 8 MAP20 generators generically; these add the tiering
// function's own threshold behavior plus a real-generator variety check).
// ---------------------------------------------------------------------------

TEST(LodTierForHeightTest, TallObjectsGetTierZero) {
    EXPECT_EQ(lod_tier_for_height(9.8f), 0);
    EXPECT_EQ(lod_tier_for_height(3.0f), 0);  // exactly at the boundary
}

TEST(LodTierForHeightTest, MediumObjectsGetTierOne) {
    EXPECT_EQ(lod_tier_for_height(2.2f), 1);
    EXPECT_EQ(lod_tier_for_height(1.0f), 1);  // exactly at the boundary
}

TEST(LodTierForHeightTest, ShortObjectsGetTierTwo) {
    EXPECT_EQ(lod_tier_for_height(0.65f), 2);
    EXPECT_EQ(lod_tier_for_height(0.0f), 2);
}

TEST(LodTierForHeightTest, HigherTierMeansShorterVisibleDistance) {
    // Ties lod_tier_for_height() to the LOD mechanism it actually feeds
    // (WorldRenderer::placement_lod_visible_distance_m()): a taller object's
    // tier must never be visible from a SHORTER distance than a shorter
    // object's tier.
    constexpr float kMaxRenderDistanceM = 200.0f;
    const float tall_dist   = placement_lod_visible_distance_m(lod_tier_for_height(9.8f), kMaxRenderDistanceM);
    const float medium_dist = placement_lod_visible_distance_m(lod_tier_for_height(2.2f), kMaxRenderDistanceM);
    const float short_dist  = placement_lod_visible_distance_m(lod_tier_for_height(0.3f), kMaxRenderDistanceM);
    EXPECT_GT(tall_dist, medium_dist);
    EXPECT_GT(medium_dist, short_dist);
}

TEST(MeadowGeneratorPlacementsTest, TreesAndGroundCoverGetDifferentLodTiers) {
    // MeadowGenerator mixes tree_oak (tall, tier 0) with flowers/grass tufts/
    // a small rock (all short, tier 2) -- a regression guard against every
    // placement silently collapsing back to the same tier regardless of
    // this generator's own genuinely mixed content.
    MeadowGenerator gen;
    bool saw_tier0 = false, saw_tier2 = false;
    for (const auto& p : gen.placements(make_ctx(1))) {
        if (p.definition_id == "tree_oak") saw_tier0 = saw_tier0 || p.lod_min == 0;
        else saw_tier2 = saw_tier2 || p.lod_min == 2;
    }
    EXPECT_TRUE(saw_tier0);
    EXPECT_TRUE(saw_tier2);
}

// ---------------------------------------------------------------------------
// MAP21 (M333-M335) -- ModelPlacements for CaveGenerator: stalactites/
// stalagmites/crystals/rubble. Unlike every MAP20 generator, CaveGenerator
// mixes ground-based placements (stalagmites/crystals/rubble, base at
// ground_elevation_m, extending upward -- the usual pattern) with
// CEILING-based ones (stalactites, base at ceiling - height so the TOP
// lands at the cave's own ceiling instead of the bottom sitting on the
// floor) -- see CaveGenerator.cpp's own add_hanging()/add_ground() split.
// ---------------------------------------------------------------------------

TEST(CaveGeneratorPlacementsTest, ProducesExpectedPlacementCount) {
    CaveGenerator gen;
    EXPECT_EQ(gen.placements(make_ctx(1)).size(), 37u);  // 14+10+5+8
}

TEST(CaveGeneratorPlacementsTest, PlacementsAreValidDeterministicAndBounded) {
    expect_valid_deterministic_bounded_placements<CaveGenerator>(
        {"stalactite_hanging", "stalagmite_rising", "crystal_blue", "rock_rubble"});
}

TEST(CaveGeneratorPlacementsTest, GenerateOutputIsDeterministic) {
    expect_generate_is_deterministic<CaveGenerator>();
}

TEST(CaveGeneratorPlacementsTest, StalactitesHangFromTheCeilingEverythingElseSitsOnTheFloor) {
    CaveGenerator gen;
    ChunkContext ctx = make_ctx(3);
    ctx.map_context.available   = true;
    ctx.map_context.elevation_m = 250.0f;  // matches ForestGenerator's own ground-elevation test value

    constexpr double kCeilingM = 8.0;
    bool saw_stalactite = false, saw_ground = false;
    for (const auto& p : gen.placements(ctx)) {
        if (p.definition_id == "stalactite_hanging") {
            saw_stalactite = true;
            // Top of the stalactite must land exactly at the cave ceiling.
            EXPECT_DOUBLE_EQ(p.y_max, 250.0 + kCeilingM) << "stalactite top should touch the ceiling";
            EXPECT_GT(p.pos_y, 250.0) << "stalactite base should be above the floor, not on it";
        } else {
            saw_ground = true;
            EXPECT_DOUBLE_EQ(p.pos_y, 250.0) << p.definition_id << " should sit on the floor";
            EXPECT_DOUBLE_EQ(p.y_min, 250.0) << p.definition_id << " should sit on the floor";
        }
    }
    EXPECT_TRUE(saw_stalactite);
    EXPECT_TRUE(saw_ground);
}
