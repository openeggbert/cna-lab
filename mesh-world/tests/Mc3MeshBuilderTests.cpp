// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#include <gtest/gtest.h>
#include "Mc3MeshBuilder.hpp"

using namespace MeshWorld;

static constexpr const char* MINIMAL_BOX_XML = R"(<?xml version="1.0"?>
<mc3 version="0.1" model="test">
  <objects>
    <box id="b1" material="brick_red" position="0 0 0" size="2 2 2"/>
  </objects>
</mc3>)";

static constexpr const char* MULTI_SHAPE_XML = R"(<?xml version="1.0"?>
<mc3 version="0.1" model="test2">
  <objects>
    <plane  id="ground" material="grass_park"  position="32 0 32" size="64 64"/>
    <box    id="wall"   material="brick_red"   position="10 1 10" size="1 2 1"/>
    <cylinder id="lamp" material="metal_lamp"  position="5 0 5"   radius="0.1" height="4"/>
  </objects>
</mc3>)";

// T263: build() on a real park-like chunk XML produces non-empty MeshList with triangles
TEST(Mc3MeshBuilderTests, T263_BuildProducesNonEmptyMeshList) {
    Mc3MeshBuilder builder;
    MeshList list = builder.build(MULTI_SHAPE_XML);

    EXPECT_FALSE(list.empty());
    EXPECT_GE(list.primitive_count(), 3);
    EXPECT_GT(list.total_triangles(), 0);
}

// A single box must produce 12 triangles (6 faces × 2).
TEST(Mc3MeshBuilderTests, BoxHasTwelveTriangles) {
    Mc3MeshBuilder builder;
    MeshList list = builder.build(MINIMAL_BOX_XML);

    ASSERT_EQ(list.primitive_count(), 1);
    EXPECT_EQ(list.primitives[0].triangle_count(), 12);
    EXPECT_EQ(list.primitives[0].object_id, "b1");
    EXPECT_EQ(list.primitives[0].material_id, "brick_red");
}

// A plane must produce exactly 2 triangles.
TEST(Mc3MeshBuilderTests, PlaneHasTwoTriangles) {
    const char* xml = R"(<mc3 version="0.1" model="t">
  <objects>
    <plane id="g" material="grass" position="0 0 0" size="10 10"/>
  </objects>
</mc3>)";
    Mc3MeshBuilder builder;
    MeshList list = builder.build(xml);

    ASSERT_EQ(list.primitive_count(), 1);
    EXPECT_EQ(list.primitives[0].triangle_count(), 2);
}

// A 16-segment cylinder must have 16*2 (sides) + 16 (top) + 16 (bottom) = 64 triangles.
TEST(Mc3MeshBuilderTests, CylinderTriangleCount) {
    const char* xml = R"(<mc3 version="0.1" model="t">
  <objects>
    <cylinder id="c1" material="metal" position="0 0 0" radius="1" height="2"/>
  </objects>
</mc3>)";
    Mc3MeshBuilder builder;
    builder.cylinder_segments = 16;
    MeshList list = builder.build(xml);

    ASSERT_EQ(list.primitive_count(), 1);
    // 16 side quads (2 tri each) + top fan (16 tri) + bottom fan (16 tri) = 64
    EXPECT_EQ(list.primitives[0].triangle_count(), 64);
}

// Malformed XML returns an empty MeshList (no crash).
TEST(Mc3MeshBuilderTests, MalformedXmlReturnsEmpty) {
    Mc3MeshBuilder builder;
    MeshList list = builder.build("this is not xml");
    EXPECT_TRUE(list.empty());
}

// <instance> elements are skipped — they have no geometry.
TEST(Mc3MeshBuilderTests, InstanceElementsSkipped) {
    const char* xml = R"(<mc3 version="0.1" model="t">
  <objects>
    <instance id="inst1" ref="some_model" position="0 0 0"/>
    <box id="b1" material="wood" position="0 0 0" size="1 1 1"/>
  </objects>
</mc3>)";
    Mc3MeshBuilder builder;
    MeshList list = builder.build(xml);
    // Only the box; instance is skipped.
    EXPECT_EQ(list.primitive_count(), 1);
}
