// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP2 tests. M023/M024: MapPayloadCodec JSON round-trip + stable ordering.

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "Map/MapPayloadCodec.hpp"
#include "Map/MapTilePayload.hpp"

using namespace MeshWorld::Map;

namespace {

// A payload exercising every field: grids, multiple feature types with
// attributes, labels, and the four (incl. one empty) tile edges.
MapTilePayload make_sample() {
    MapTilePayload p;
    p.tile      = TileCoord{5, 17, 9};
    p.entropy   = 0xDEADBEEF12345678ULL;
    p.culture   = "nordic";
    p.generator = "planet-v1";

    p.elevation   = FieldGrid{2, 2, {0.0f, 100.5f, -3.25f, 9000.0f}};
    p.temperature = FieldGrid{2, 1, {12.5f, -40.0f}};
    p.moisture    = FieldGrid{1, 2, {0.1f, 0.9f}};
    p.biome       = BiomeGrid{2, 2, {1, 2, 3, 4}};
    p.zone_candidates = BiomeGrid{2, 2, {0, 3, 0, 4}};

    MapFeature river;
    river.type       = FeatureType::River;
    river.name       = "Long River";
    river.points     = {{1.0, 2.0}, {3.5, 4.25}};
    river.attributes = {{"flow", 3.5}, {"width", 12.0}};
    p.features.push_back(river);

    MapFeature city;
    city.type       = FeatureType::City;
    city.name       = "Aldburg";
    city.points     = {{10.0, 20.0}};
    city.attributes = {{"population_hint", 50000.0}};
    p.features.push_back(city);

    p.labels.push_back(PlaceLabel{"Aldburg", {{10.0, 20.0}}, "city"});
    p.labels.push_back(PlaceLabel{"Long River", {{2.0, 3.0}}, "river"});

    p.edges[0].elevation = {1.0f, 2.0f, 3.0f};
    p.edges[1].elevation = {4.0f};
    p.edges[2].elevation = {};  // empty edge survives the round-trip
    p.edges[3].elevation = {5.5f, 6.5f};

    // M107 — edge biome + river/road crossings.
    p.edges[0].biome     = {1, 2, 3};
    p.edges[0].crossings = {EdgeCrossing{EdgeCrossingType::River, 0.25f},
                            EdgeCrossing{EdgeCrossingType::Road, 0.75f}};
    // edges[1..3] leave biome/crossings empty — must also survive round-trip.
    return p;
}

} // namespace

// M024 — encode→decode→encode is byte-identical (canonical, order-stable).
TEST(MapPayloadCodecTest, RoundTripIsStable) {
    const MapTilePayload p  = make_sample();
    const std::string    j1 = MapPayloadCodec::encode(p);
    const MapTilePayload p2 = MapPayloadCodec::decode(j1);
    const std::string    j2 = MapPayloadCodec::encode(p2);
    EXPECT_EQ(j1, j2);
}

// Encoding the same payload twice yields identical text (stable key order).
TEST(MapPayloadCodecTest, EncodeIsDeterministic) {
    const MapTilePayload p = make_sample();
    EXPECT_EQ(MapPayloadCodec::encode(p), MapPayloadCodec::encode(p));
}

TEST(MapPayloadCodecTest, PreservesScalarFields) {
    const MapTilePayload p = make_sample();
    const MapTilePayload r = MapPayloadCodec::decode(MapPayloadCodec::encode(p));
    EXPECT_EQ(r.tile, p.tile);
    EXPECT_EQ(r.entropy, p.entropy);
    EXPECT_EQ(r.culture, "nordic");
    EXPECT_EQ(r.generator, "planet-v1");
}

TEST(MapPayloadCodecTest, PreservesGrids) {
    const MapTilePayload p = make_sample();
    const MapTilePayload r = MapPayloadCodec::decode(MapPayloadCodec::encode(p));

    ASSERT_EQ(r.elevation.w, 2);
    ASSERT_EQ(r.elevation.h, 2);
    EXPECT_FLOAT_EQ(r.elevation.at(0, 0), 0.0f);
    EXPECT_FLOAT_EQ(r.elevation.at(1, 1), 9000.0f);
    EXPECT_FLOAT_EQ(r.moisture.at(0, 1), 0.9f);

    ASSERT_EQ(r.biome.w, 2);
    EXPECT_EQ(r.biome.at(1, 0), 2);
    EXPECT_EQ(r.biome.at(1, 1), 4);

    ASSERT_EQ(r.zone_candidates.w, 2);
    ASSERT_EQ(r.zone_candidates.h, 2);
    EXPECT_EQ(r.zone_candidates.at(1, 0), 3);
    EXPECT_EQ(r.zone_candidates.at(1, 1), 4);
}

TEST(MapPayloadCodecTest, PreservesFeaturesLabelsEdges) {
    const MapTilePayload p = make_sample();
    const MapTilePayload r = MapPayloadCodec::decode(MapPayloadCodec::encode(p));

    ASSERT_EQ(r.features.size(), 2u);
    EXPECT_EQ(r.features[0].type, FeatureType::River);
    EXPECT_EQ(r.features[0].name, "Long River");
    ASSERT_EQ(r.features[0].points.size(), 2u);
    EXPECT_DOUBLE_EQ(r.features[0].points[1][0], 3.5);
    EXPECT_DOUBLE_EQ(r.features[0].points[1][1], 4.25);
    EXPECT_DOUBLE_EQ(r.features[0].attributes.at("width"), 12.0);
    EXPECT_EQ(r.features[1].type, FeatureType::City);

    ASSERT_EQ(r.labels.size(), 2u);
    EXPECT_EQ(r.labels[0].name, "Aldburg");
    EXPECT_EQ(r.labels[0].kind, "city");
    EXPECT_DOUBLE_EQ(r.labels[0].pos[1], 20.0);

    ASSERT_EQ(r.edges[0].elevation.size(), 3u);
    EXPECT_FLOAT_EQ(r.edges[0].elevation[2], 3.0f);
    EXPECT_TRUE(r.edges[2].elevation.empty());
    ASSERT_EQ(r.edges[3].elevation.size(), 2u);
    EXPECT_FLOAT_EQ(r.edges[3].elevation[1], 6.5f);
}

// M107 — edge biome + river/road crossings survive the round-trip.
TEST(MapPayloadCodecTest, PreservesEdgeBiomeAndCrossings) {
    const MapTilePayload p = make_sample();
    const MapTilePayload r = MapPayloadCodec::decode(MapPayloadCodec::encode(p));

    ASSERT_EQ(r.edges[0].biome.size(), 3u);
    EXPECT_EQ(r.edges[0].biome[1], 2);

    ASSERT_EQ(r.edges[0].crossings.size(), 2u);
    EXPECT_EQ(r.edges[0].crossings[0].type, EdgeCrossingType::River);
    EXPECT_FLOAT_EQ(r.edges[0].crossings[0].position, 0.25f);
    EXPECT_EQ(r.edges[0].crossings[1].type, EdgeCrossingType::Road);
    EXPECT_FLOAT_EQ(r.edges[0].crossings[1].position, 0.75f);

    EXPECT_TRUE(r.edges[1].biome.empty());
    EXPECT_TRUE(r.edges[1].crossings.empty());
}

// M107 — tiles persisted before M107 lack "biome"/"crossings" keys on their
// edge objects; decoding must default them to empty, not throw, so existing
// worlds keep loading unchanged.
TEST(MapPayloadCodecTest, DecodesPreM107EdgesWithoutBiomeOrCrossings) {
    const MapTilePayload p       = make_sample();
    const std::string    encoded = MapPayloadCodec::encode(p);

    nlohmann::json j = nlohmann::json::parse(encoded);
    for (auto& edge : j.at("edges")) {
        edge.erase("biome");
        edge.erase("crossings");
    }

    const MapTilePayload r = MapPayloadCodec::decode(j.dump());
    EXPECT_TRUE(r.edges[0].biome.empty());
    EXPECT_TRUE(r.edges[0].crossings.empty());
    ASSERT_EQ(r.edges[0].elevation.size(), 3u);
    EXPECT_FLOAT_EQ(r.edges[0].elevation[2], 3.0f);
}

TEST(MapPayloadCodecTest, DecodesPreM156PayloadsWithoutZoneCandidates) {
    const MapTilePayload p       = make_sample();
    const std::string    encoded = MapPayloadCodec::encode(p);

    nlohmann::json j = nlohmann::json::parse(encoded);
    j.erase("zone_candidates");

    const MapTilePayload r = MapPayloadCodec::decode(j.dump());
    EXPECT_TRUE(r.zone_candidates.empty());
    // Everything else still decodes fine.
    ASSERT_EQ(r.biome.w, 2);
    EXPECT_EQ(r.biome.at(1, 1), 4);
}

// Malformed JSON / missing fields surface as exceptions, not silent defaults.
TEST(MapPayloadCodecTest, DecodeRejectsMalformedInput) {
    EXPECT_ANY_THROW(MapPayloadCodec::decode("not json"));
    EXPECT_ANY_THROW(MapPayloadCodec::decode("{}"));
}
