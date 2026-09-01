// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T138-T146: C++ chunk generator unit tests

#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <regex>
#include <random>
#include <sstream>
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"
#include "WorldMap.hpp"
#include "MC3Validator.hpp"
#include "AssetRegistry.hpp"
#include "BuiltinMaterials.hpp"
#include "BuiltinStyles.hpp"
#include "ComposerAssets.hpp"
#include "ObjectDefinitionLibrary.hpp"
#include "generators/ParkGenerator.hpp"
#include "generators/RoadGenerator.hpp"
#include "generators/CrossroadGenerator.hpp"
#include "generators/SmallHouseBlockGenerator.hpp"
#include "generators/ApartmentBlockGenerator.hpp"
#include "generators/ForestGenerator.hpp"

namespace {

MeshWorld::ChunkContext make_ctx(MeshWorld::ZoneType zone   = MeshWorld::ZoneType::city,
                                  MeshWorld::RegionType region = MeshWorld::RegionType::park,
                                  uint64_t seed = 42) {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = seed;
    ctx.zone         = zone;
    ctx.region       = region;
    ctx.chunk_size_m = 64.0f;
    ctx.coord.x      = 0;
    ctx.coord.y      = 0;
    return ctx;
}

bool xml_ok(const std::string& xml, float size = 64.0f) {
    MeshWorld::MC3Validator v;
    return v.validate(xml, size).ok;
}

struct GenParam {
    const char*           label;
    MeshWorld::ZoneType   zone;
    MeshWorld::RegionType region;
};

} // namespace

// ── T138: Fixture ────────────────────────────────────────────────────────────

class GeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        MeshWorld::register_builtin_materials();
        // G11 fix (2026-07-11): ParkGenerator/RoadGenerator/
        // SmallHouseBlockGenerator all try StyleRegistry::instance().
        // get(ctx.style) -- previously never populated in this fixture, so
        // that lookup always silently fell back to hardcoded defaults.
        MeshWorld::register_builtin_styles();
    }
};

// ── T139: ParkGenerator ──────────────────────────────────────────────────────

TEST_F(GeneratorTest, ParkGeneratorValidXmlAndMetadata) {
    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml)) << xml;
    EXPECT_NE(xml.find("<metadata"), std::string::npos);
}

TEST_F(GeneratorTest, ParkGeneratorDeterminism) {
    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    EXPECT_EQ(gen.generate(ctx), gen.generate(ctx));
}

// ── T140: RoadGenerator ──────────────────────────────────────────────────────

TEST_F(GeneratorTest, RoadGeneratorNorthSouthExits) {
    MeshWorld::RoadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::road);
    ctx.exits.north_road = true;
    ctx.exits.south_road = true;
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));  // skip bounds: road coords go to chunk edge
    // NS road should have asphalt surface
    EXPECT_NE(xml.find("asphalt"), std::string::npos)
        << "Expected asphalt road in NS road output";
}

TEST_F(GeneratorTest, RoadGeneratorEastWestExits) {
    MeshWorld::RoadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::road);
    ctx.exits.east_road = true;
    ctx.exits.west_road = true;
    auto xml = gen.generate(ctx);

    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find("asphalt"), std::string::npos);
}

// R134 -- a legal road terminus has one canonical edge, so the renderer must
// not silently extend the asphalt to the opposing chunk boundary.
TEST_F(GeneratorTest, RoadGeneratorSingleExitDrawsOnlyThatBoundaryArm) {
    MeshWorld::RoadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::road);
    ctx.exits.north_road = true;
    const auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find(R"(id="road_n")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_s")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_e")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_w")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
}

// ── T141: CrossroadGenerator ─────────────────────────────────────────────────

TEST_F(GeneratorTest, CrossroadGeneratorValidXmlAndMetadata) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.exits.east_road  = ctx.exits.west_road  = true;
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find("<metadata"), std::string::npos);
}

// T230 -- with all 4 exits present, every arm + the center junction is drawn.
TEST_F(GeneratorTest, CrossroadGeneratorFourWayHasAllArms) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.exits.east_road  = ctx.exits.west_road  = true;
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find(R"(id="road_n")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_s")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_e")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_w")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
}

// T230 -- a genuine 3-way (T-junction, west missing) omits only the west arm.
TEST_F(GeneratorTest, CrossroadGeneratorThreeWayOmitsMissingArm) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    ctx.exits.north_road = true;
    ctx.exits.south_road = true;
    ctx.exits.east_road  = true;
    ctx.exits.west_road  = false;
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find(R"(id="road_n")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_s")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_e")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_w")"), std::string::npos)
        << "west arm should be omitted for a west-missing T-junction\n" << xml;
    EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
}

// R134 -- a crossroad with no canonical outgoing edge must not guess all four
// arms; that used to paint visual roads into unrelated neighbouring chunks.
TEST_F(GeneratorTest, CrossroadGeneratorWithoutExitsDrawsNoBoundaryArms) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_EQ(xml.find(R"(id="road_n")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_s")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_e")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="road_w")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="road_center")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="cw_n_1")"), std::string::npos);
    EXPECT_EQ(xml.find(R"(id="cw_s_1")"), std::string::npos);
}

// R121 zone/chunk audit follow-up (2026-07-12) -- crossroad.lua was found
// to have real content (crosswalk stripes, traffic lights) this C++
// fallback lacked; both are now ported here.
TEST_F(GeneratorTest, CrossroadGeneratorHasCrosswalkStripes) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    ctx.exits.north_road = true;
    ctx.exits.south_road = true;
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    EXPECT_NE(xml.find("cw_s_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("cw_n_1"), std::string::npos) << xml;
    EXPECT_NE(xml.find("road_line_white"), std::string::npos);
}

TEST_F(GeneratorTest, CrossroadGeneratorHasTrafficLightsAtAllFourCorners) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml, 0.0f));
    for (int i = 1; i <= 4; ++i) {
        const std::string idx = std::to_string(i);
        EXPECT_NE(xml.find("tl_pole_" + idx), std::string::npos) << "corner " << i;
        EXPECT_NE(xml.find("tl_head_" + idx), std::string::npos) << "corner " << i;
    }
    EXPECT_NE(xml.find("light_red"), std::string::npos);
    EXPECT_NE(xml.find("light_amber"), std::string::npos);
    EXPECT_NE(xml.find("light_green"), std::string::npos);
}

TEST_F(GeneratorTest, CrossroadGeneratorTrafficLightsHaveOneDeterministicLitStatePerApproach) {
    MeshWorld::CrossroadGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::crossroad);
    ctx.seed = 0;  // north/south green; east/west red
    const auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    // The green north/south pair and red east/west pair are lit; each other
    // lens stays visibly coloured but dim. This prevents the old impossible
    // all-three-lenses-on signal from returning unnoticed.
    EXPECT_NE(xml.find("light_green"), std::string::npos);
    EXPECT_NE(xml.find("light_red"), std::string::npos);
    EXPECT_NE(xml.find("light_amber_dim"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="tl_red_1")"), std::string::npos);
    EXPECT_NE(xml.find(R"(id="tl_green_2")"), std::string::npos);
}

// ── T142: SmallHouseBlockGenerator ──────────────────────────────────────────

TEST_F(GeneratorTest, SmallHouseBlockValidXmlAndDeterminism) {
    MeshWorld::SmallHouseBlockGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::small_house_block);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml)) << xml;
    EXPECT_EQ(xml, gen.generate(ctx)) << "SmallHouseBlock must be deterministic";
}

// ── T143: ApartmentBlockGenerator ───────────────────────────────────────────

TEST_F(GeneratorTest, ApartmentBlockValidXml) {
    MeshWorld::ApartmentBlockGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::apartment_block);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml)) << xml;
}

// T233 -- floor count is now a real discrete property (5-7 floors, 3.2 m
// each), not a continuous height jitter. Across many seeds, "block_n"'s
// own height (the y component of its size="x y z") must always be an
// exact multiple of 3.2 m in {16.0, 19.2, 22.4} -- proof it's floor-count
// driven, not an arbitrary continuous value.
TEST_F(GeneratorTest, ApartmentBlockHeightIsDiscreteFloorMultiple) {
    MeshWorld::ApartmentBlockGenerator gen;
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        auto ctx = make_ctx(MeshWorld::ZoneType::city, MeshWorld::RegionType::apartment_block, seed);
        auto xml = gen.generate(ctx);
        ASSERT_FALSE(xml.empty());

        const auto id_pos = xml.find(R"(id="block_n")");
        ASSERT_NE(id_pos, std::string::npos) << "seed=" << seed;
        const auto size_pos = xml.find(R"(size=")", id_pos);
        ASSERT_NE(size_pos, std::string::npos) << "seed=" << seed;
        const auto value_start = size_pos + std::string(R"(size=")").size();
        const auto value_end = xml.find('"', value_start);
        std::istringstream iss(xml.substr(value_start, value_end - value_start));
        float sx = 0.0f, sy = 0.0f, sz = 0.0f;
        iss >> sx >> sy >> sz;

        const bool is_valid_floor_height =
            std::abs(sy - 16.0f) < 0.01f || std::abs(sy - 19.2f) < 0.01f || std::abs(sy - 22.4f) < 0.01f;
        EXPECT_TRUE(is_valid_floor_height) << "seed=" << seed << " height=" << sy;
    }
}

// ── T144: ForestGenerator ────────────────────────────────────────────────────

TEST_F(GeneratorTest, ForestGeneratorValidXmlAndMetadata) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml)) << xml;
    EXPECT_NE(xml.find("<metadata"), std::string::npos);
}

// T226-229 -- dense tree clusters, bushes, a clearing, and a winding path.
TEST_F(GeneratorTest, ForestGeneratorHasTreeClusters) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_TRUE(xml_ok(xml)) << xml;
    EXPECT_NE(xml.find("cluster_0_0"), std::string::npos) << xml;
    EXPECT_NE(xml.find("cluster_1_0"), std::string::npos) << xml;
}

TEST_F(GeneratorTest, ForestGeneratorHasBushes) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    for (int i = 0; i < 5; ++i) {
        const std::string idx = std::to_string(i);
        EXPECT_NE(xml.find("bush_" + idx + "_a"), std::string::npos) << "bush " << i;
    }
    EXPECT_NE(xml.find("shrub_foliage"), std::string::npos);
}

TEST_F(GeneratorTest, ForestGeneratorHasAClearing) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find(R"(id="clearing")"), std::string::npos) << xml;
    EXPECT_NE(xml.find("grass_courtyard"), std::string::npos);
}

// R143b -- the clearing is a real canopy exclusion zone, not merely a grass
// plane painted underneath the old uniform tree scatter.
TEST_F(GeneratorTest, ForestGeneratorKeepsPrimaryTreesOutsideItsClearing) {
    constexpr std::uint64_t seed = 42;
    MeshWorld::ForestGenerator gen;
    const auto xml = gen.generate(make_ctx(MeshWorld::ZoneType::forest,
                                           MeshWorld::RegionType::open, seed));

    std::mt19937_64 layout_rng(seed ^ 0xC1EA71A6ULL);
    std::uniform_real_distribution<float> pos(12.0f, 52.0f);
    const float cx = pos(layout_rng);
    const float cz = pos(layout_rng);
    constexpr float min_distance = 6.5f; // clearing radius plus tree margin

    const std::regex tree(R"tree(<instance id="tree_[0-9]+" position="([^ ]+) [^ ]+ ([^"]+)")tree");
    std::size_t count = 0;
    for (std::sregex_iterator it(xml.begin(), xml.end(), tree), end; it != end; ++it) {
        const float x = std::stof((*it)[1].str());
        const float z = std::stof((*it)[2].str());
        const float dx = x - cx;
        const float dz = z - cz;
        EXPECT_GE(dx * dx + dz * dz, min_distance * min_distance);
        ++count;
    }
    EXPECT_EQ(count, 32u);

    const std::regex cluster(
        R"tree(<instance id="cluster_[0-9]+_[0-9]+" position="([^ ]+) [^ ]+ ([^"]+)")tree");
    std::size_t cluster_count = 0;
    for (std::sregex_iterator it(xml.begin(), xml.end(), cluster), end; it != end; ++it) {
        const float x = std::stof((*it)[1].str());
        const float z = std::stof((*it)[2].str());
        const float dx = x - cx;
        const float dz = z - cz;
        EXPECT_GE(dx * dx + dz * dz, min_distance * min_distance);
        ++cluster_count;
    }
    EXPECT_EQ(cluster_count, 10u);
}

TEST_F(GeneratorTest, ForestGeneratorHasAWindingPath) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    auto xml = gen.generate(ctx);

    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find(R"(id="path_0")"), std::string::npos) << xml;
    EXPECT_NE(xml.find("path_gravel"), std::string::npos);
}

TEST_F(GeneratorTest, ForestGeneratorDeterministic) {
    MeshWorld::ForestGenerator gen;
    auto ctx = make_ctx(MeshWorld::ZoneType::forest, MeshWorld::RegionType::open);
    EXPECT_EQ(gen.generate(ctx), gen.generate(ctx));
}

// ── T145: All 20 C++ generators produce valid XML ───────────────────────────

class AllGeneratorsTest : public ::testing::TestWithParam<GenParam> {
protected:
    void SetUp() override {
        MeshWorld::register_builtin_materials();
        MeshWorld::register_builtin_styles();
    }
};

TEST_P(AllGeneratorsTest, ProducesValidXml) {
    auto p = GetParam();
    MeshWorld::ChunkContext ctx = make_ctx(p.zone, p.region);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.exits.east_road  = ctx.exits.west_road  = true;

    MeshWorld::ChunkGenerator* gen = MeshWorld::get_generator(p.zone, p.region);
    ASSERT_NE(gen, nullptr);

    std::string xml = gen->generate(ctx);
    ASSERT_FALSE(xml.empty()) << p.label << " returned empty XML";

    MeshWorld::MC3Validator v;
    auto result = v.validate(xml, 0.0f);  // skip bounds for road-type generators
    for (const auto& e : result.errors)
        ADD_FAILURE() << p.label << " error: " << e;
    EXPECT_TRUE(result.ok) << p.label << " produced invalid XML";
}

// M163 (MAP10) — the same battery, but with a populated map_context this
// time (available=true + representative elevation/biome/river data),
// proving every generator still produces valid XML once the map layer is
// attached. Originally written when only 4 generators (MountainGenerator/
// OceanGenerator/ForestGenerator/MeadowGenerator, M160) actually read
// ctx.map_context and the other ~16 silently ignored it; MAP17 (M281-297)
// later wired all 20 to read it for real, so this test's own value shifted
// from "prove it's a no-op for most" to "prove every real map_context read
// still produces valid XML across every generator/zone combination."
// Reuses AllGeneratorsTest's own INSTANTIATE_TEST_SUITE_P parameter list
// automatically (gtest runs
// every TEST_P registered against a fixture across all its instantiated
// parameter sets).
TEST_P(AllGeneratorsTest, ProducesValidXmlWithMapContextAvailable) {
    auto p = GetParam();
    MeshWorld::ChunkContext ctx = make_ctx(p.zone, p.region);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.exits.east_road  = ctx.exits.west_road  = true;

    ctx.map_context.available                = true;
    ctx.map_context.elevation_m              = 500.0f;
    ctx.map_context.biome_ordinal             = static_cast<std::uint8_t>(p.zone);
    ctx.map_context.is_city                   = (p.zone == MeshWorld::ZoneType::city);
    ctx.map_context.nearest_river_name        = "Test River";
    ctx.map_context.nearest_river_distance_m  = 120.0f;

    MeshWorld::ChunkGenerator* gen = MeshWorld::get_generator(p.zone, p.region);
    ASSERT_NE(gen, nullptr);

    std::string xml = gen->generate(ctx);
    ASSERT_FALSE(xml.empty()) << p.label << " returned empty XML with map_context available";

    MeshWorld::MC3Validator v;
    auto result = v.validate(xml, 0.0f);
    for (const auto& e : result.errors)
        ADD_FAILURE() << p.label << " error: " << e;
    EXPECT_TRUE(result.ok) << p.label << " produced invalid XML with map_context available";
}

// T308 (old T-series backlog, resolved 2026-07-11 as part of a triage pass) --
// "all definition IDs referenced by C++ generators are resolvable from
// ObjectDefinitionLibrary". This is the test that would have caught a real,
// long-standing bug found during that same triage: ForestGenerator.cpp
// instanced "tree_pine"/"tree_beech", but neither was ever registered in
// ObjectDefinitionLibrary::load_all() -- WorldRenderer::inject_definitions()
// silently no-ops for an unresolvable ID (by design, so one bad reference
// doesn't break an entire chunk), so those trees were invisible in every
// rendered forest chunk with no error anywhere. Regex-extracts every
// `definition="..."` attribute from a generator's own serialized XML output
// (matching how Mc3XmlWriter.cpp actually writes it) rather than needing a
// full Mc3Document parse -- MC3Validator's own approach elsewhere in this
// file works the same way, directly against the XML string.
TEST_P(AllGeneratorsTest, InstanceDefinitionsResolveFromObjectDefinitionLibrary) {
    static const bool loaded = [] {
        MeshWorld::ObjectDefinitionLibrary::instance().load_all();
        return true;
    }();
    (void)loaded;

    auto p = GetParam();
    MeshWorld::ChunkContext ctx = make_ctx(p.zone, p.region);
    ctx.exits.north_road = ctx.exits.south_road = true;
    ctx.exits.east_road  = ctx.exits.west_road  = true;

    MeshWorld::ChunkGenerator* gen = MeshWorld::get_generator(p.zone, p.region);
    ASSERT_NE(gen, nullptr);

    std::string xml = gen->generate(ctx);
    ASSERT_FALSE(xml.empty()) << p.label << " returned empty XML";

    static const std::regex kDefRe(R"re(definition="([^"]+)")re");
    auto& lib = MeshWorld::ObjectDefinitionLibrary::instance();
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), kDefRe); it != std::sregex_iterator();
         ++it) {
        const std::string id = (*it)[1].str();
        EXPECT_TRUE(lib.has(id)) << p.label << " instances unresolvable definition \"" << id << "\"";
    }
}

// M297 (MAP17, 2026-07-10) -- the audit test ChunkGenerator.hpp's own
// contract comment (right above the class) points at: every one of the 20
// generators must produce DIFFERENT output for at least one populated
// map_context than for none, not just "doesn't crash" (that's what
// ProducesValidXmlWithMapContextAvailable above already proved). Different
// generators react to different fields in opposite directions (e.g. Beach
// wants LOW elevation, Bridge/Tundra want HIGH), so no single shared
// map_context config can trigger all 20 at once -- try several distinct
// configurations per generator and require at least one to differ.
TEST_P(AllGeneratorsTest, RespondsToAtLeastOneMapContextConfiguration) {
    auto p = GetParam();
    auto make = [&](MeshWorld::ZoneType z, MeshWorld::RegionType r) {
        MeshWorld::ChunkContext c = make_ctx(z, r);
        c.exits.north_road = c.exits.south_road = true;
        c.exits.east_road  = c.exits.west_road  = true;
        return c;
    };

    MeshWorld::ChunkGenerator* gen = MeshWorld::get_generator(p.zone, p.region);
    ASSERT_NE(gen, nullptr);

    MeshWorld::ChunkContext baseline = make(p.zone, p.region);
    ASSERT_FALSE(baseline.map_context.available);
    const std::string baseline_xml = gen->generate(baseline);

    std::vector<MeshWorld::ChunkContext> candidates;

    MeshWorld::ChunkContext low_elev = make(p.zone, p.region);
    low_elev.map_context.available   = true;
    low_elev.map_context.elevation_m = -50.0f;
    candidates.push_back(low_elev);

    MeshWorld::ChunkContext high_elev = make(p.zone, p.region);
    high_elev.map_context.available   = true;
    high_elev.map_context.elevation_m = 2500.0f;
    candidates.push_back(high_elev);

    MeshWorld::ChunkContext road = make(p.zone, p.region);
    road.map_context.available         = true;
    road.map_context.has_road_crossing = true;
    candidates.push_back(road);

    MeshWorld::ChunkContext place = make(p.zone, p.region);
    place.map_context.available          = true;
    place.map_context.nearest_place_name = "Vorhavn";
    place.map_context.nearest_place_kind = "city";
    candidates.push_back(place);

    MeshWorld::ChunkContext river = make(p.zone, p.region);
    river.map_context.available               = true;
    river.map_context.nearest_river_name      = "Testfluss";
    river.map_context.nearest_river_distance_m = 30.0f;
    candidates.push_back(river);

    bool any_different = false;
    for (const auto& c : candidates) {
        if (gen->generate(c) != baseline_xml) { any_different = true; break; }
    }
    EXPECT_TRUE(any_different)
        << p.label << " produced identical output for every populated map_context "
        << "tried (low/high elevation, road crossing, named place, nearby river) -- "
        << "violates the ChunkGenerator.hpp contract (M297, MAP17)";
}

INSTANTIATE_TEST_SUITE_P(
    AllCppGenerators, AllGeneratorsTest,
    ::testing::Values(
        GenParam{"Park",            MeshWorld::ZoneType::city,     MeshWorld::RegionType::park},
        GenParam{"Road",            MeshWorld::ZoneType::city,     MeshWorld::RegionType::road},
        GenParam{"Crossroad",       MeshWorld::ZoneType::city,     MeshWorld::RegionType::crossroad},
        GenParam{"SmallHouseBlock", MeshWorld::ZoneType::city,     MeshWorld::RegionType::small_house_block},
        GenParam{"ApartmentBlock",  MeshWorld::ZoneType::city,     MeshWorld::RegionType::apartment_block},
        GenParam{"ShopStreet",      MeshWorld::ZoneType::city,     MeshWorld::RegionType::shop_street},
        GenParam{"Square",          MeshWorld::ZoneType::city,     MeshWorld::RegionType::square},
        GenParam{"RiverBank",       MeshWorld::ZoneType::city,     MeshWorld::RegionType::river_bank},
        GenParam{"Bridge",          MeshWorld::ZoneType::city,     MeshWorld::RegionType::bridge},
        GenParam{"Forest",          MeshWorld::ZoneType::forest,   MeshWorld::RegionType::open},
        GenParam{"Jungle",          MeshWorld::ZoneType::jungle,   MeshWorld::RegionType::open},
        GenParam{"Desert",          MeshWorld::ZoneType::desert,   MeshWorld::RegionType::open},
        GenParam{"Cave",            MeshWorld::ZoneType::cave,     MeshWorld::RegionType::open},
        GenParam{"Mountain",        MeshWorld::ZoneType::mountain, MeshWorld::RegionType::open},
        GenParam{"Meadow",          MeshWorld::ZoneType::meadow,   MeshWorld::RegionType::open},
        GenParam{"Beach",           MeshWorld::ZoneType::beach,    MeshWorld::RegionType::open},
        GenParam{"Ocean",           MeshWorld::ZoneType::ocean,    MeshWorld::RegionType::open},
        GenParam{"Swamp",           MeshWorld::ZoneType::swamp,    MeshWorld::RegionType::open},
        GenParam{"Tundra",          MeshWorld::ZoneType::tundra,   MeshWorld::RegionType::open},
        // MAP20, M326 -- coral_reef/kelp_forest now dispatch to
        // OceanGenerator (ChunkGenerator.cpp) instead of falling through to
        // EmptyGenerator; covered by the same generic suite every other
        // zone-open generator already is.
        GenParam{"CoralReef",       MeshWorld::ZoneType::coral_reef,  MeshWorld::RegionType::open},
        GenParam{"KelpForest",      MeshWorld::ZoneType::kelp_forest, MeshWorld::RegionType::open},
        GenParam{"Empty",           MeshWorld::ZoneType::empty,    MeshWorld::RegionType::empty}
    ),
    [](const ::testing::TestParamInfo<GenParam>& info) {
        return info.param.label;
    }
);

// ── T146: Determinism across calls ──────────────────────────────────────────

TEST_F(GeneratorTest, SameSeedSameRegionSameOutput) {
    // Test several generators for determinism
    const std::pair<MeshWorld::ZoneType, MeshWorld::RegionType> combos[] = {
        {MeshWorld::ZoneType::city,   MeshWorld::RegionType::park},
        {MeshWorld::ZoneType::forest, MeshWorld::RegionType::open},
        {MeshWorld::ZoneType::desert, MeshWorld::RegionType::open},
    };
    for (auto [zone, region] : combos) {
        auto ctx = make_ctx(zone, region, 99);
        auto* gen = MeshWorld::get_generator(zone, region);
        EXPECT_EQ(gen->generate(ctx), gen->generate(ctx))
            << "Generator not deterministic for zone=" << static_cast<int>(zone)
            << " region=" << static_cast<int>(region);
    }
}

// MAP20, M326 -- coral_reef/kelp_forest must no longer fall through to
// EmptyGenerator (the pre-M326 state the audit found for all 40 new MAP16
// biomes): both dispatch to the exact same OceanGenerator instance ordinary
// ocean chunks already use, which then branches its own content on ctx.zone.
TEST_F(GeneratorTest, CoralReefAndKelpForestDispatchToOceanNotEmpty) {
    MeshWorld::ChunkGenerator* coral = MeshWorld::get_generator(MeshWorld::ZoneType::coral_reef, MeshWorld::RegionType::open);
    MeshWorld::ChunkGenerator* kelp  = MeshWorld::get_generator(MeshWorld::ZoneType::kelp_forest, MeshWorld::RegionType::open);
    MeshWorld::ChunkGenerator* ocean = MeshWorld::get_generator(MeshWorld::ZoneType::ocean, MeshWorld::RegionType::open);
    EXPECT_EQ(coral, ocean);
    EXPECT_EQ(kelp, ocean);
}

// R142 -- every reachable MAP16 biome must at least render through its
// closest established natural family while Stage 6 authors distinct assets.
// A named biome silently falling back to EmptyGenerator is a player-visible
// blank world and defeats the planetary classifier's additional detail.
TEST_F(GeneratorTest, ModernBiomeFamiliesDoNotDispatchToEmptyGenerator) {
    const MeshWorld::ChunkGenerator* const empty =
        MeshWorld::get_generator(MeshWorld::ZoneType::empty, MeshWorld::RegionType::empty);
    const MeshWorld::ZoneType zones[] = {
        MeshWorld::ZoneType::savanna, MeshWorld::ZoneType::steppe,
        MeshWorld::ZoneType::prairie, MeshWorld::ZoneType::chaparral,
        MeshWorld::ZoneType::shrubland, MeshWorld::ZoneType::taiga,
        MeshWorld::ZoneType::temperate_rainforest, MeshWorld::ZoneType::mixed_forest,
        MeshWorld::ZoneType::cloud_forest, MeshWorld::ZoneType::mangrove,
        MeshWorld::ZoneType::bamboo_forest, MeshWorld::ZoneType::riparian_forest,
        MeshWorld::ZoneType::tropical_dry_forest, MeshWorld::ZoneType::marsh,
        MeshWorld::ZoneType::floodplain, MeshWorld::ZoneType::bog,
        MeshWorld::ZoneType::muskeg, MeshWorld::ZoneType::dunes,
        MeshWorld::ZoneType::rocky_desert, MeshWorld::ZoneType::cold_desert,
        MeshWorld::ZoneType::salt_flat, MeshWorld::ZoneType::badlands,
        MeshWorld::ZoneType::mesa, MeshWorld::ZoneType::canyon,
        MeshWorld::ZoneType::oasis, MeshWorld::ZoneType::glacier,
        MeshWorld::ZoneType::permafrost, MeshWorld::ZoneType::alpine_meadow,
        MeshWorld::ZoneType::ice_cap, MeshWorld::ZoneType::volcanic,
        MeshWorld::ZoneType::geothermal, MeshWorld::ZoneType::ash_plain,
        MeshWorld::ZoneType::volcanic_island, MeshWorld::ZoneType::deep_ocean,
        MeshWorld::ZoneType::lagoon, MeshWorld::ZoneType::fjord,
        MeshWorld::ZoneType::tidal_flat, MeshWorld::ZoneType::sea_cliff,
    };
    for (const MeshWorld::ZoneType zone : zones) {
        EXPECT_NE(MeshWorld::get_generator(zone, MeshWorld::RegionType::open), empty)
            << "zone=" << MeshWorld::to_string(zone);
    }
}

// R142 -- a showcase must be valid for more than one lucky random draw.
// Beach and forest previously let generated primitives pass a chunk boundary;
// this sweeps a bounded representative seed range through MC3 validation.
TEST_F(GeneratorTest, BeachAndForestGeneratedScenesStayValidAcrossSeeds) {
    for (const MeshWorld::ZoneType zone :
         {MeshWorld::ZoneType::beach, MeshWorld::ZoneType::forest}) {
        MeshWorld::ChunkGenerator* const generator =
            MeshWorld::get_generator(zone, MeshWorld::RegionType::open);
        for (std::uint64_t seed = 0; seed < 64; ++seed) {
            const auto result = MeshWorld::MC3Validator{}.validate(
                generator->generate(make_ctx(zone, MeshWorld::RegionType::open, seed)), 64.0f);
            EXPECT_TRUE(result.ok) << "zone=" << MeshWorld::to_string(zone)
                                   << ", seed=" << seed;
        }
    }
}

// R143a -- reusable natural definitions must be registered with real metadata
// and must replace the inline-only fallback when the normal app/tool startup
// path has populated the asset registry.
TEST_F(GeneratorTest, NatureAssetKitsAreMetadataTaggedAndSelectedByBiomes) {
    MeshWorld::ObjectDefinitionLibrary::instance().load_all();
    MeshWorld::register_composer_assets();
    const auto forest_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_tree", {"temperate_forest"});
    const auto jungle_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_tree", {"jungle"});
    const auto desert_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_plant", {"desert"});
    const auto mountain_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_rock", {"mountain"});
    const auto swamp_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_tree", {"swamp"});
    const auto coast_assets = MeshWorld::AssetRegistry::instance().query(
        "nature_prop", {"coast"});
    for (const auto& assets : {forest_assets, jungle_assets, desert_assets, mountain_assets, swamp_assets, coast_assets}) {
        ASSERT_FALSE(assets.empty());
        for (const auto* asset : assets) {
            ASSERT_NE(asset, nullptr);
            EXPECT_TRUE(asset->meta.lods.contains("low"));
            EXPECT_FALSE(asset->meta.collisionProxy.empty());
        }
    }
    // R143c: the formerly singleton wetland/coast kits must offer distinct
    // silhouettes, so deterministic selection can make adjacent chunks read
    // as an environment rather than one repeatedly instanced prop.
    EXPECT_GE(swamp_assets.size(), 2u);
    EXPECT_GE(coast_assets.size(), 2u);

    const auto forest_xml = MeshWorld::get_generator(MeshWorld::ZoneType::forest,
        MeshWorld::RegionType::open)->generate(make_ctx(MeshWorld::ZoneType::forest,
            MeshWorld::RegionType::open, 143));
    const auto jungle_xml = MeshWorld::get_generator(MeshWorld::ZoneType::jungle,
        MeshWorld::RegionType::open)->generate(make_ctx(MeshWorld::ZoneType::jungle,
            MeshWorld::RegionType::open, 143));
    EXPECT_NE(forest_xml.find("nature.tree.temperate."), std::string::npos);
    EXPECT_NE(jungle_xml.find("nature.tree.jungle."), std::string::npos);
}

// T315: every generated chunk must contain an <environment> block with <fog>.
TEST_F(GeneratorTest, ChunkXmlContainsEnvironmentAndFog) {
    MeshWorld::register_builtin_materials();
    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    std::string xml = gen.generate(ctx);
    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<environment"), std::string::npos)
        << "Missing <environment> block in generated chunk XML";
    EXPECT_NE(xml.find("<fog"), std::string::npos)
        << "Missing <fog> element in generated chunk XML";
    EXPECT_NE(xml.find("<background"), std::string::npos)
        << "Missing <background> element in generated chunk XML";
}

// T312: every generated chunk must contain a directional sun and an ambient light.
TEST_F(GeneratorTest, ChunkXmlContainsSunAndAmbientLight) {
    MeshWorld::register_builtin_materials();
    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    std::string xml = gen.generate(ctx);
    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find("<directional"), std::string::npos)
        << "Missing directional sun light in generated chunk XML";
    EXPECT_NE(xml.find("<ambient"), std::string::npos)
        << "Missing ambient light in generated chunk XML";
}

// T231: ParkGenerator uses lamp_post_ornate instances (not raw cylinders) for lamps.
TEST_F(GeneratorTest, ParkGeneratorUsesLampPostInstances) {
    MeshWorld::register_builtin_materials();
    MeshWorld::ParkGenerator gen;
    auto ctx = make_ctx();
    std::string xml = gen.generate(ctx);
    ASSERT_FALSE(xml.empty());
    EXPECT_NE(xml.find("lamp_post_ornate"), std::string::npos)
        << "ParkGenerator should reference lamp_post_ornate instances";
    // No raw metal_lamp_ornate cylinder for lamp post anymore
    // (flower beds and other metal may still exist, so just check instances present)
    EXPECT_NE(xml.find("<instance"), std::string::npos)
        << "ParkGenerator should emit <instance> elements";
}

// T330: zone_color returns a distinct RGB triple for every ZoneType value.
TEST(WorldMapZoneColorTest, DistinctColorsForAllZones) {
    const MeshWorld::ZoneType all[] = {
        MeshWorld::ZoneType::city,     MeshWorld::ZoneType::jungle,
        MeshWorld::ZoneType::desert,   MeshWorld::ZoneType::forest,
        MeshWorld::ZoneType::ocean,    MeshWorld::ZoneType::mountain,
        MeshWorld::ZoneType::tundra,   MeshWorld::ZoneType::swamp,
        MeshWorld::ZoneType::cave,     MeshWorld::ZoneType::meadow,
        MeshWorld::ZoneType::beach,    MeshWorld::ZoneType::empty,
    };

    std::vector<std::array<float,3>> colors;
    for (auto z : all)
        colors.push_back(MeshWorld::WorldMap::zone_color(z));

    for (size_t i = 0; i < colors.size(); ++i)
        for (size_t j = i + 1; j < colors.size(); ++j)
            EXPECT_NE(colors[i], colors[j])
                << "zone_color collision between zone index " << i << " and " << j;
}
