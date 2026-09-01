// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// M098/M099/M103 — MapValidator: structural checks on a Map::MapTilePayload
// before persistence, mirroring MC3Validator's role for chunk XML.

#include <gtest/gtest.h>

#include <cstdint>

#include "MapValidator.hpp"
#include "ZoneType.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

// A minimal, fully valid 2x2 payload: matching field sizes, valid biome
// ordinals, one in-bounds feature, and self-consistent edge lengths.
MapTilePayload make_valid_payload() {
    MapTilePayload p;
    p.tile    = TileCoord{5, 3, 7};
    p.entropy = 42;

    p.elevation.w = 2;
    p.elevation.h = 2;
    p.elevation.data = {10.0f, 20.0f, 30.0f, 40.0f};

    p.temperature.w = 2;
    p.temperature.h = 2;
    p.temperature.data = {1.0f, 2.0f, 3.0f, 4.0f};

    p.moisture.w = 2;
    p.moisture.h = 2;
    p.moisture.data = {0.1f, 0.2f, 0.3f, 0.4f};

    p.biome.w = 2;
    p.biome.h = 2;
    p.biome.data = {static_cast<std::uint8_t>(ZoneType::ocean),
                     static_cast<std::uint8_t>(ZoneType::forest),
                     static_cast<std::uint8_t>(ZoneType::desert),
                     static_cast<std::uint8_t>(ZoneType::mountain)};

    const WorldBounds b = p.tile.world_bounds();
    const double cx = (b.min_x + b.max_x) * 0.5;
    const double cz = (b.min_z + b.max_z) * 0.5;
    MapFeature f;
    f.type   = FeatureType::City;
    f.name   = "Vorhavn";
    f.points = {{cx, cz}};
    p.features.push_back(f);

    p.edges[0].elevation = {10.0f, 30.0f}; // N: top row, w samples
    p.edges[1].elevation = {30.0f, 40.0f}; // E: right col, h samples
    p.edges[2].elevation = {30.0f, 40.0f}; // S: bottom row, w samples
    p.edges[3].elevation = {10.0f, 30.0f}; // W: left col, h samples

    return p;
}

} // namespace

TEST(MapValidatorTest, ValidPayloadPasses) {
    MapValidator validator;
    const ValidationResult r = validator.validate(make_valid_payload());

    for (const auto& e : r.errors) ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(r.ok);
}

TEST(MapValidatorTest, PayloadWithNoFeaturesOrEdgesStillPasses) {
    MapTilePayload p = make_valid_payload();
    p.features.clear();
    p.edges = {};

    MapValidator validator;
    const ValidationResult r = validator.validate(p);
    EXPECT_TRUE(r.ok) << "features/edges are optional, not required";
}

// M103 — a feature outside the tile's world bounds fails validation.
TEST(MapValidatorTest, FeatureOutsideTileBoundsFails) {
    MapTilePayload p = make_valid_payload();
    const WorldBounds b = p.tile.world_bounds();

    MapFeature bad;
    bad.type   = FeatureType::City;
    bad.name   = "Faraway";
    bad.points = {{b.max_x + 1000.0, b.max_z + 1000.0}}; // well outside
    p.features.push_back(bad);

    MapValidator validator;
    const ValidationResult r = validator.validate(p);

    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
    bool mentions_name = false;
    for (const auto& e : r.errors)
        if (e.find("Faraway") != std::string::npos) mentions_name = true;
    EXPECT_TRUE(mentions_name) << "error should identify the offending feature";
}

TEST(MapValidatorTest, EmptyElevationFieldFails) {
    MapTilePayload p = make_valid_payload();
    p.elevation = {};

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}

TEST(MapValidatorTest, EmptyTemperatureFieldFails) {
    MapTilePayload p = make_valid_payload();
    p.temperature = {};

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}

TEST(MapValidatorTest, EmptyMoistureFieldFails) {
    MapTilePayload p = make_valid_payload();
    p.moisture = {};

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}

TEST(MapValidatorTest, EmptyBiomeFieldFails) {
    MapTilePayload p = make_valid_payload();
    p.biome = {};

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}

TEST(MapValidatorTest, FieldDataSizeMismatchFails) {
    MapTilePayload p = make_valid_payload();
    p.elevation.data.pop_back(); // now 3 elements, but w*h == 4

    MapValidator validator;
    const ValidationResult r = validator.validate(p);
    EXPECT_FALSE(r.ok);
}

TEST(MapValidatorTest, BiomeOrdinalOutOfRangeFails) {
    MapTilePayload p = make_valid_payload();
    p.biome.data[0] = 255; // far beyond ZoneType's range

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}

TEST(MapValidatorTest, EdgeLengthMismatchFails) {
    MapTilePayload p = make_valid_payload();
    p.edges[0].elevation = {10.0f}; // N should have w=2 samples, not 1

    MapValidator validator;
    const ValidationResult r = validator.validate(p);
    EXPECT_FALSE(r.ok);
}

// M276 (MAP16, 2026-07-10): the range check itself is automatic --
// src/MapValidator.cpp's max_valid is `static_cast<uint8_t>(ZoneType::empty)`,
// which tracks wherever `empty` lands as the enum grows, not a hardcoded
// literal -- but that automatic behavior is worth a dedicated regression
// test now that ZoneType has grown from 12 to 52 (M235), so a future
// accidental reordering (e.g. `empty` no longer last) fails loudly here
// instead of silently rejecting/accepting the wrong ordinals.
TEST(MapValidatorTest, AcceptsOneOfTheFortyNewM235BiomeValues) {
    MapTilePayload p = make_valid_payload();
    p.biome.data[0] = static_cast<std::uint8_t>(ZoneType::savanna);  // ordinal 11, was invalid pre-M235

    MapValidator validator;
    const ValidationResult r = validator.validate(p);
    for (const auto& e : r.errors) ADD_FAILURE() << "Unexpected error: " << e;
    EXPECT_TRUE(r.ok);
}

TEST(MapValidatorTest, AcceptsEmptyAtItsCurrentOrdinalFiftyOne) {
    MapTilePayload p = make_valid_payload();
    p.biome.data[0] = static_cast<std::uint8_t>(ZoneType::empty);

    MapValidator validator;
    ASSERT_EQ(static_cast<int>(ZoneType::empty), 51) << "MapValidator's max_valid tracks this value directly";
    EXPECT_TRUE(validator.validate(p).ok);
}

TEST(MapValidatorTest, RejectsOneOrdinalPastEmpty) {
    MapTilePayload p = make_valid_payload();
    p.biome.data[0] = static_cast<std::uint8_t>(ZoneType::empty) + 1;  // 52: one past the valid range

    MapValidator validator;
    EXPECT_FALSE(validator.validate(p).ok);
}
