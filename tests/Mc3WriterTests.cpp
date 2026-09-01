// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T321-T322: MC3Writer sphere() and cone() primitive helpers.

#include <gtest/gtest.h>
#include "MC3Writer.hpp"
#include "ChunkGenerator.hpp"
#include "ZoneType.hpp"
#include "RegionType.hpp"

namespace {

MeshWorld::ChunkContext make_ctx() {
    MeshWorld::ChunkContext ctx;
    ctx.seed         = 42;
    ctx.zone         = MeshWorld::ZoneType::city;
    ctx.region       = MeshWorld::RegionType::park;
    ctx.chunk_size_m = 64.0f;
    ctx.coord.x      = 0;
    ctx.coord.y      = 0;
    return ctx;
}

bool has(const std::string& xml, const std::string& needle) {
    return xml.find(needle) != std::string::npos;
}

} // namespace

// T321 — sphere() emits a <sphere> element with the requested radius.
TEST(Mc3WriterTests, SphereEmitsSphereElement) {
    MeshWorld::MC3Writer w(make_ctx());
    w.sphere("ball", 10.0f, 20.0f, 1.5f, "stone_light");
    auto xml = w.build();

    EXPECT_TRUE(has(xml, "<sphere"))        << xml;
    EXPECT_TRUE(has(xml, "radius=\"1.5\"")) << xml;
}

// T321 — y is base elevation: sphere center sits radius above y.
TEST(Mc3WriterTests, SphereCenteredAtBasePlusRadius) {
    MeshWorld::MC3Writer w(make_ctx());
    w.sphere("ball", 10.0f, 20.0f, 1.5f, "stone_light", /*y=*/0.0f);
    auto xml = w.build();

    // base y=0, radius 1.5 → center = (10, 1.5, 20)
    EXPECT_TRUE(has(xml, "position=\"10 1.5 20\"")) << xml;
}

// T322 — cone() emits a <cone> element with the requested radius and height.
TEST(Mc3WriterTests, ConeEmitsConeElement) {
    MeshWorld::MC3Writer w(make_ctx());
    w.cone("spike", 30.0f, 40.0f, 2.0f, 4.0f, "foliage_pine");
    auto xml = w.build();

    EXPECT_TRUE(has(xml, "<cone"))          << xml;
    EXPECT_TRUE(has(xml, "radius=\"2\""))   << xml;
    EXPECT_TRUE(has(xml, "height=\"4\""))   << xml;
}

// T322 — y is base elevation: cone center sits height/2 above y (mirrors cylinder).
TEST(Mc3WriterTests, ConeCenteredAtBasePlusHalfHeight) {
    MeshWorld::MC3Writer w(make_ctx());
    w.cone("spike", 30.0f, 40.0f, 2.0f, 4.0f, "foliage_pine", /*y=*/0.0f);
    auto xml = w.build();

    // base y=0, height 4 → center = (30, 2, 40)
    EXPECT_TRUE(has(xml, "position=\"30 2 40\"")) << xml;
}

// Both helpers can coexist in one chunk document.
TEST(Mc3WriterTests, SphereAndConeTogether) {
    MeshWorld::MC3Writer w(make_ctx());
    w.ground("grass_park");
    w.sphere("ball",  5.0f,  5.0f, 1.0f, "stone_light");
    w.cone("spike", 20.0f, 20.0f, 1.5f, 3.0f, "foliage_pine");
    auto xml = w.build();

    EXPECT_TRUE(has(xml, "<sphere")) << xml;
    EXPECT_TRUE(has(xml, "<cone"))   << xml;
}
