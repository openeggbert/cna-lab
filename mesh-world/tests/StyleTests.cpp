// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T186: Style system tests — palette lookup, registry, and style-dependent generation.

#include <gtest/gtest.h>
#include "StyleRegistry.hpp"
#include "BuiltinStyles.hpp"
#include "BuiltinMaterials.hpp"
#include "generators/ParkGenerator.hpp"
#include "ChunkGenerator.hpp"
#include "WorldMap.hpp"
#include "WorldConfig.hpp"

namespace {

void ensure_styles_registered() {
    static bool done = false;
    if (!done) {
        MeshWorld::register_builtin_materials();
        MeshWorld::register_builtin_styles();
        done = true;
    }
}

MeshWorld::ChunkContext make_park_ctx(const std::string& style_id) {
    MeshWorld::ChunkContext ctx;
    ctx.coord        = {5, 5};
    ctx.seed         = 42;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.style        = style_id;
    ctx.chunk_size_m = 64.0f;
    return ctx;
}

} // namespace

// T186a — StyleRegistry has all four builtin styles
TEST(StyleTests, RegistryHasBuiltinStyles) {
    ensure_styles_registered();
    auto& reg = MeshWorld::StyleRegistry::instance();
    EXPECT_TRUE(reg.has("central_europe_small_city"));
    EXPECT_TRUE(reg.has("nordic_town"));
    EXPECT_TRUE(reg.has("desert_outpost"));
    EXPECT_TRUE(reg.has("jungle_village"));
}

// T186b — Style::mat() returns palette value when key is present
TEST(StyleTests, MatReturnsFromPalette) {
    ensure_styles_registered();
    const auto& central = MeshWorld::StyleRegistry::instance().get("central_europe_small_city");
    EXPECT_EQ(central.mat("park.ground", "fallback"), "grass_park");
    EXPECT_EQ(central.mat("road.surface", "fallback"), "asphalt");
    EXPECT_EQ(central.mat("block.facade.0", "fallback"), "brick_red");
}

// T186c — Style::mat() returns fallback for unknown key
TEST(StyleTests, MatReturnsFallbackForUnknownKey) {
    ensure_styles_registered();
    const auto& s = MeshWorld::StyleRegistry::instance().get("nordic_town");
    EXPECT_EQ(s.mat("nonexistent.key", "my_fallback"), "my_fallback");
}

// T186d — nordic_town uses different ground material than central_europe (snow vs grass)
TEST(StyleTests, NordicTownDifferentGroundThanCentralEurope) {
    ensure_styles_registered();
    const auto& central = MeshWorld::StyleRegistry::instance().get("central_europe_small_city");
    const auto& nordic  = MeshWorld::StyleRegistry::instance().get("nordic_town");
    const std::string central_ground = central.mat("park.ground", "grass_park");
    const std::string nordic_ground  = nordic.mat("park.ground", "grass_park");
    EXPECT_NE(central_ground, nordic_ground)
        << "nordic_town park ground should differ from central_europe_small_city";
    EXPECT_EQ(central_ground, "grass_park");
    EXPECT_EQ(nordic_ground, "snow");
}

// T186e — ParkGenerator with nordic_town produces XML containing snow, not grass_park
TEST(StyleTests, ParkGeneratorNordicUsesSnowGround) {
    ensure_styles_registered();

    MeshWorld::ParkGenerator gen;
    const auto ctx_central = make_park_ctx("central_europe_small_city");
    const auto ctx_nordic  = make_park_ctx("nordic_town");

    std::string xml_central = gen.generate(ctx_central);
    std::string xml_nordic  = gen.generate(ctx_nordic);

    EXPECT_NE(xml_central, xml_nordic) << "Different styles should produce different XML";
    EXPECT_NE(xml_nordic.find("snow"), std::string::npos)
        << "nordic_town park XML should contain snow material";
    EXPECT_EQ(xml_nordic.find("grass_park"), std::string::npos)
        << "nordic_town park XML should not contain grass_park";
    EXPECT_NE(xml_central.find("grass_park"), std::string::npos)
        << "central_europe park XML should contain grass_park";
}

// T186f — desert_outpost park uses sand ground
TEST(StyleTests, DesertOutpostParkUsesSand) {
    ensure_styles_registered();

    MeshWorld::ParkGenerator gen;
    std::string xml = gen.generate(make_park_ctx("desert_outpost"));
    EXPECT_NE(xml.find("sand"), std::string::npos)
        << "desert_outpost park XML should contain sand material";
}
