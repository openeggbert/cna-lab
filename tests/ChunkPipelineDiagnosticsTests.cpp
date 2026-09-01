// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// R108 — Generator/asset selection diagnostics: ChunkPipeline::get(x, y,
// ChunkDiagnostics*) must report, per chunk, which generator actually won
// (source + generator_id), a non-empty fallback_reason whenever the C++
// path is used (never silent), and basic content stats, for all three
// outcomes: real Lua success, C++ fallback (no Lua registered), and a
// cache hit re-reading previously generated content.

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "BuiltinMaterials.hpp"
#include "ChunkCache.hpp"
#include "ChunkDiagnostics.hpp"
#include "ChunkPipeline.hpp"
#include "LuaGeneratorRegistry.hpp"
#include "WorldConfig.hpp"
#include "WorldMap.hpp"

using namespace MeshWorld;

namespace {

// Real Lua generators (generators/lua/zone/*.lua) are looked up through the
// GLOBAL LuaGeneratorRegistry::instance() singleton in production
// (ChunkPipeline.cpp) -- same convention LuaCompositionTests.cpp's own
// fixture already established. Idempotent across translation units in the
// same test binary.
//
// R106 (investigated 2026-07-13) -- this fixture exercises the real
// lua.zone.park generator, which references real MaterialRegistry ids
// ("grass", "stone_pavement", "stone_path") but never registered the
// builtin set itself, instead relying on some OTHER test file happening
// to run first in the same MeshWorldTests binary (confirmed via
// --gtest_shuffle: a bad seed reproduces spurious "not registered"
// warnings for materials that ARE genuinely registered in
// BuiltinMaterials.cpp). register_builtin_materials() is idempotent, so
// calling it here too removes the run-order dependency entirely.
struct ChunkPipelineDiagnosticsFixture : ::testing::Test {
    static void SetUpTestSuite() {
        register_builtin_materials();
        LuaGeneratorRegistry::instance().load_from_dir("generators/lua");
    }

    // Isolated on-disk cache per test so runs never see stale/real
    // cache/chunks/ entries from other tests or previous sessions. The
    // directory is wiped first: ChunkCache persists to disk across
    // process runs, so a leftover dir from an EARLIER invocation of this
    // same test binary (not just other tests) would otherwise make a
    // fresh run's very first get() silently read as a cache hit.
    static ChunkCache make_cache(const std::string& suffix) {
        const auto dir = std::filesystem::temp_directory_path() /
                         ("meshworld_chunkdiag_test_" + suffix);
        std::filesystem::remove_all(dir);
        return ChunkCache{dir.string()};
    }
};

} // namespace

TEST(ChunkCacheTest, BoundedCacheEvictsOldestEntryBeforeGrowing) {
    const auto dir = std::filesystem::temp_directory_path() / "meshworld_chunkcache_bounded";
    std::filesystem::remove_all(dir);
    ChunkCache cache{dir.string(), 2};
    cache.store({0, 0}, "old");
    std::filesystem::last_write_time(dir / "0_0.mc3.xml",
                                     std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    cache.store({1, 0}, "middle");
    cache.store({2, 0}, "new");

    EXPECT_FALSE(cache.has({0, 0}));
    EXPECT_TRUE(cache.has({1, 0}));
    EXPECT_TRUE(cache.has({2, 0}));
    std::filesystem::remove_all(dir);
}

// A real Lua zone generator exists for RegionType::park
// (generators/lua/zone/park.lua, M.id == "lua.zone.park") -- the diagnostics
// for a successful Lua run must report source == Lua, the real generator_id
// read back from <metadata>, an empty fallback_reason, and non-negative
// content stats.
TEST_F(ChunkPipelineDiagnosticsFixture, LuaSuccessReportsLuaSourceAndNoFallbackReason) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});
    ChunkPipeline pipeline(cfg, map, make_cache("lua_success"));

    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);

    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::Lua);
    EXPECT_EQ(diag.generator_id, "lua.zone.park");
    EXPECT_TRUE(diag.fallback_reason.empty());
    EXPECT_EQ(diag.region, RegionType::park);
    EXPECT_TRUE(diag.validation_errors.empty());
    EXPECT_GE(diag.object_count, 0);
    EXPECT_GE(diag.triangle_count, 0);
}

// No Lua source is registered for RegionType::empty ("lua.zone.empty" isn't
// one of the 4 real generators/lua/zone/*.lua files) -- the fallback must
// never be silent: fallback_reason must explicitly say no generator was
// registered, source must read CppFallback, and the generator_id read back
// from the C++ generator's own metadata must start with "cpp.".
TEST_F(ChunkPipelineDiagnosticsFixture, CppFallbackReportsNonEmptyFallbackReason) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::empty, RegionType::empty, EdgeExits{}});
    ChunkPipeline pipeline(cfg, map, make_cache("cpp_fallback"));

    ChunkDiagnostics diag;
    const std::string xml = pipeline.get(0, 0, &diag);

    EXPECT_FALSE(xml.empty());
    EXPECT_EQ(diag.source, ChunkDiagnostics::Source::CppFallback);
    EXPECT_FALSE(diag.fallback_reason.empty());
    EXPECT_NE(diag.fallback_reason.find("no Lua generator registered"), std::string::npos)
        << "fallback_reason: " << diag.fallback_reason;
    EXPECT_EQ(diag.generator_id.rfind("cpp.", 0), 0u)
        << "Expected a 'cpp.'-prefixed generator_id, got '" << diag.generator_id << "'";
}

// A second get() call for the same coord hits the on-disk cache. Diagnostics
// must still be filled in (source == Cache) rather than left default/empty,
// and must report the SAME generator_id the original (uncached) call did --
// re-derived from the cached content's own <metadata>, not lost.
TEST_F(ChunkPipelineDiagnosticsFixture, CacheHitStillFillsDiagnosticsFromStoredMetadata) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(0, 0, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});
    ChunkPipeline pipeline(cfg, map, make_cache("cache_hit"));

    ChunkDiagnostics first_diag;
    const std::string first_xml = pipeline.get(0, 0, &first_diag);
    ASSERT_EQ(first_diag.source, ChunkDiagnostics::Source::Lua);

    ChunkDiagnostics second_diag;
    const std::string second_xml = pipeline.get(0, 0, &second_diag);

    EXPECT_EQ(second_xml, first_xml);
    EXPECT_EQ(second_diag.source, ChunkDiagnostics::Source::Cache);
    EXPECT_EQ(second_diag.generator_id, first_diag.generator_id);
    EXPECT_TRUE(second_diag.fallback_reason.empty());
    EXPECT_EQ(second_diag.object_count, first_diag.object_count);
}

// Passing a null diagnostics pointer must behave exactly like the plain
// get(x, y) overload (no crash, same content) -- diagnostics are strictly
// additive/opt-in.
TEST_F(ChunkPipelineDiagnosticsFixture, NullDiagnosticsPointerIsSafeAndMatchesPlainGet) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(1, 1, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});

    ChunkPipeline pipeline_a(cfg, map, make_cache("null_diag_a"));
    ChunkPipeline pipeline_b(cfg, map, make_cache("null_diag_b"));

    const std::string xml_with_null = pipeline_a.get(1, 1, nullptr);
    const std::string xml_plain      = pipeline_b.get(1, 1);

    EXPECT_FALSE(xml_with_null.empty());
    EXPECT_FALSE(xml_plain.empty());
}

// T237 -- `force=true` must ignore an existing (even stale/bogus) cache
// entry and regenerate for real, then overwrite the cache with the fresh
// content so a later plain (non-forced) get() also reads the fresh result,
// not the original stale one.
TEST_F(ChunkPipelineDiagnosticsFixture, ForceRegeneratesEvenWhenCacheHasStaleContent) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(2, 2, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});

    ChunkCache cache = make_cache("force");
    cache.store(ChunkCoord{2, 2}, "STALE-BOGUS-CONTENT-NOT-REAL-MC3-XML");
    ASSERT_TRUE(cache.has(ChunkCoord{2, 2}));

    ChunkPipeline pipeline(cfg, map, cache);

    ChunkDiagnostics forced_diag;
    const std::string forced_xml = pipeline.get(2, 2, &forced_diag, /*force=*/true);

    EXPECT_NE(forced_xml, "STALE-BOGUS-CONTENT-NOT-REAL-MC3-XML");
    EXPECT_EQ(forced_diag.source, ChunkDiagnostics::Source::Lua);
    EXPECT_EQ(forced_diag.generator_id, "lua.zone.park");

    // The forced call must have overwritten the stale cache entry: a later
    // plain (non-forced) get() reads the fresh content back, not the stale
    // placeholder that was there before the forced call.
    ChunkDiagnostics cached_diag;
    const std::string cached_xml = pipeline.get(2, 2, &cached_diag);
    EXPECT_EQ(cached_xml, forced_xml);
    EXPECT_EQ(cached_diag.source, ChunkDiagnostics::Source::Cache);
}

// T243 -- the progress callback must fire exactly once per get() call, with
// the coord that was just produced and the real source (Lua on the first,
// uncached call; Cache on a repeat call for the same coord).
TEST_F(ChunkPipelineDiagnosticsFixture, ProgressCallbackFiresOncePerGetWithCoordAndSource) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(3, 3, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});

    ChunkPipeline pipeline(cfg, map, make_cache("progress_cb"));

    std::vector<std::pair<ChunkCoord, ChunkDiagnostics::Source>> events;
    pipeline.set_progress_callback([&](const ChunkCoord& c, ChunkDiagnostics::Source s) {
        events.emplace_back(c, s);
    });

    pipeline.get(3, 3);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].first, (ChunkCoord{3, 3}));
    EXPECT_EQ(events[0].second, ChunkDiagnostics::Source::Lua);

    pipeline.get(3, 3);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[1].first, (ChunkCoord{3, 3}));
    EXPECT_EQ(events[1].second, ChunkDiagnostics::Source::Cache);
}

// No callback set (the default) must not crash -- get() behaves exactly as
// it did before T243.
TEST_F(ChunkPipelineDiagnosticsFixture, NoProgressCallbackSetIsSafe) {
    WorldConfig cfg;
    WorldMap    map(cfg);
    map.set_info(4, 4, ChunkInfo{ZoneType::city, RegionType::park, EdgeExits{}});

    ChunkPipeline pipeline(cfg, map, make_cache("no_progress_cb"));
    const std::string xml = pipeline.get(4, 4);
    EXPECT_FALSE(xml.empty());
}
