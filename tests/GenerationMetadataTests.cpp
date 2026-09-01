// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R129 (zone-metadata bug fix, NEXT.md §4) — GenerationMetadata's "zone"
// field must report ChunkContext::authored_zone (the flat WorldMap/
// WorldConfig-derived zone), not ctx.zone, which the M157 map-layer
// override may have replaced with an unrelated planet's own sampled
// biome. See ChunkContext::authored_zone's own doc comment
// (include/ChunkGenerator.hpp) for the full root-cause writeup.

#include <gtest/gtest.h>

#include "GenerationMetadata.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

using namespace MeshWorld;

// Direct unit test: construct a ChunkContext where zone/authored_zone
// deliberately disagree (simulating the real bug scenario -- a demo
// world whose flat config authored "city" but whose freshly auto-created
// hand-off planet's biome happens to sample as "deep_ocean" at that
// location) and confirm the metadata reports the AUTHORED value.
TEST(GenerationMetadataTests, ReportsAuthoredZoneNotOverriddenZone) {
    ChunkContext ctx;
    ctx.zone          = ZoneType::deep_ocean;  // simulates the M157 override
    ctx.authored_zone = ZoneType::city;        // simulates the flat WorldConfig zone
    ctx.region        = RegionType::small_house_block;

    const auto meta = GenerationMetadata::from_chunk_context(ctx, "test.generator");
    EXPECT_EQ(meta.zone, "city");
    EXPECT_NE(meta.zone, "deep_ocean");
}

// When no override ever applied (authored_zone == zone, the common case
// for every ChunkContext built without a MapPipeline attached, and every
// existing test fixture that only ever sets ctx.zone directly), the
// metadata's zone must still match zone exactly -- this fix must not
// change behavior for the vastly more common non-overridden case.
TEST(GenerationMetadataTests, ReportsZoneUnchangedWhenNoOverrideApplied) {
    ChunkContext ctx;
    ctx.zone          = ZoneType::city;
    ctx.authored_zone = ZoneType::city;
    ctx.region        = RegionType::park;

    const auto meta = GenerationMetadata::from_chunk_context(ctx, "test.generator");
    EXPECT_EQ(meta.zone, "city");
}

// Region is untouched by this fix -- only the zone field was ever
// reported wrong; region still comes straight from ctx.region.
TEST(GenerationMetadataTests, RegionStillReadDirectlyFromContext) {
    ChunkContext ctx;
    ctx.zone          = ZoneType::deep_ocean;
    ctx.authored_zone = ZoneType::city;
    ctx.region        = RegionType::shop_street;

    const auto meta = GenerationMetadata::from_chunk_context(ctx, "test.generator");
    EXPECT_EQ(meta.region, to_string(RegionType::shop_street));
}

// The serialized JSON itself must carry the authored zone, since to_json()
// is what actually ends up embedded in exported chunk XML (the real
// symptom NEXT.md §4 documents: `grep '"zone"' output/chunks/1_1.mc3.xml`).
TEST(GenerationMetadataTests, SerializedJsonCarriesAuthoredZone) {
    ChunkContext ctx;
    ctx.zone          = ZoneType::deep_ocean;
    ctx.authored_zone = ZoneType::city;
    ctx.region        = RegionType::small_house_block;

    const auto meta = GenerationMetadata::from_chunk_context(ctx, "test.generator");
    const std::string json = meta.to_json();
    EXPECT_NE(json.find("\"zone\": \"city\""), std::string::npos) << json;
    EXPECT_EQ(json.find("\"zone\": \"deep_ocean\""), std::string::npos) << json;
}
