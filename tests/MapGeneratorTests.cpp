// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP4 tests. M050: MapGenerator interface usability + determinism contract.

#include <gtest/gtest.h>

#include <memory>

#include "Map/MapGenerator.hpp"

using namespace MeshWorld::Map;

namespace {

// Minimal concrete generator: tags the payload with the tile + entropy and
// records whether it saw a parent. Deterministic in its inputs.
class StubGenerator : public MapGenerator {
public:
    MapTilePayload generate(const TileCoord&      tile,
                            const MapTilePayload* parent,
                            std::uint64_t         entropy) const override {
        MapTilePayload p;
        p.tile      = tile;
        p.entropy   = entropy;
        p.generator = "stub";
        p.culture   = parent ? parent->culture : "root";
        return p;
    }
};

} // namespace

TEST(MapGeneratorTest, GeneratesThroughBasePointer) {
    std::unique_ptr<MapGenerator> gen = std::make_unique<StubGenerator>();
    const TileCoord tile{0, 0, 0};

    const MapTilePayload p = gen->generate(tile, nullptr, 12345u);
    EXPECT_EQ(p.tile, tile);
    EXPECT_EQ(p.entropy, 12345u);
    EXPECT_EQ(p.generator, "stub");
    EXPECT_EQ(p.culture, "root");  // null parent -> root
}

TEST(MapGeneratorTest, PassesParentThrough) {
    StubGenerator gen;
    MapTilePayload parent;
    parent.culture = "nordic";

    const MapTilePayload child = gen.generate(TileCoord{1, 1, 0}, &parent, 7u);
    EXPECT_EQ(child.culture, "nordic");  // inherited from parent
}

TEST(MapGeneratorTest, SameInputsAreDeterministic) {
    StubGenerator   gen;
    const TileCoord tile{3, 5, 2};
    const MapTilePayload a = gen.generate(tile, nullptr, 99u);
    const MapTilePayload b = gen.generate(tile, nullptr, 99u);
    EXPECT_EQ(a.tile, b.tile);
    EXPECT_EQ(a.entropy, b.entropy);
    EXPECT_EQ(a.generator, b.generator);
}
