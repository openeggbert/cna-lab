// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// T159-T160: Demo world export and MC3 validation tests.

#include <gtest/gtest.h>
#include "WorldConfig.hpp"
#include "WorldMap.hpp"
#include "ChunkPipeline.hpp"
#include "ChunkCache.hpp"
#include "MC3Validator.hpp"
#include "BuiltinMaterials.hpp"
#include "BuiltinStyles.hpp"
#include <filesystem>

// T159 — all 400 chunks generate without crash
TEST(DemoWorldTests, AllChunksGenerateWithoutCrash) {
    MeshWorld::register_builtin_materials();
    // G11 fix (2026-07-11): this generates real chunks end-to-end, including
    // the 3 generators that consume StyleRegistry.
    MeshWorld::register_builtin_styles();

    MeshWorld::WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"))
        << "Failed to load examples/world.json — run tests from project root";

    MeshWorld::WorldMap   map(cfg);
    MeshWorld::ChunkCache cache;
    MeshWorld::ChunkPipeline pipeline(cfg, map, std::move(cache));

    int generated = 0;
    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            std::string xml;
            ASSERT_NO_THROW(xml = pipeline.get(x, y))
                << "Chunk (" << x << "," << y << ") threw an exception";
            EXPECT_FALSE(xml.empty())
                << "Chunk (" << x << "," << y << ") returned empty XML";
            ++generated;
        }
    }
    EXPECT_EQ(generated, cfg.grid_w * cfg.grid_h);
}

// T160 — all 400 chunks pass MC3Validator
TEST(DemoWorldTests, AllChunksPassMC3Validator) {
    MeshWorld::register_builtin_materials();
    MeshWorld::register_builtin_styles();

    MeshWorld::WorldConfig cfg;
    ASSERT_TRUE(cfg.load_from_file("examples/world.json"));

    MeshWorld::WorldMap   map(cfg);
    MeshWorld::ChunkCache cache;
    MeshWorld::ChunkPipeline pipeline(cfg, map, std::move(cache));
    MeshWorld::MC3Validator  validator;

    int failures = 0;
    for (int y = 0; y < cfg.grid_h; ++y) {
        for (int x = 0; x < cfg.grid_w; ++x) {
            std::string xml = pipeline.get(x, y);
            auto result = validator.validate(xml, static_cast<float>(cfg.chunk_size_m));
            if (!result.ok) {
                ++failures;
                for (const auto& e : result.errors)
                    ADD_FAILURE() << "Chunk (" << x << "," << y << "): " << e;
                if (failures >= 5) {
                    ADD_FAILURE() << "(stopping after 5 failures)";
                    goto done;
                }
            }
        }
    }
    done:
    EXPECT_EQ(failures, 0) << failures << " chunk(s) failed validation";
}
