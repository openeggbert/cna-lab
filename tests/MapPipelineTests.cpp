// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
//
// MAP7 tests. M117: child elevation at tile boundaries matches parent edge
// samples. M119: sibling generation order independence. Also covers M104-M106
// and M113 end-to-end.

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "LuaGeneratorRegistry.hpp"
#include "Map/MapPipeline.hpp"
#include "Map/PlanetConstants.hpp"
#include "MapValidator.hpp"
#include "PlanetWorld.hpp"
#include "WorldConfig.hpp"
#include "ZoneType.hpp"
#include "generators/map/PlanetGenerator.hpp"

using namespace MeshWorld;
using namespace MeshWorld::Map;

namespace {

// Scratch directory under the system temp for test worlds.
std::string tmp_dir(const std::string& suffix) {
    return (std::filesystem::temp_directory_path() / ("meshworld_pipe_test_" + suffix)).string();
}

PlanetParams make_params() {
    PlanetParams p;
    p.planet_size_m  = 22585000.0;
    p.continents_min = 5;
    p.continents_max = 12;
    p.sea_level_m    = 0.0;
    p.equator_temp_c = 30.0;
    p.pole_temp_c    = -20.0;
    return p;
}

// Absolute difference between two floats.
float absdiff(float a, float b) { return std::abs(a - b); }

// M220 — opens `path` as a brand-new, independent SQLite connection (not
// reusing anything MapPipeline/MapTileStore already has open) and runs
// PRAGMA integrity_check, the standard SQLite structural-soundness check.
// Returns the single "ok" row's text, or the first problem line found.
std::string sqlite_integrity_check(const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        const std::string err = db ? sqlite3_errmsg(db) : "cannot open";
        if (db) sqlite3_close(db);
        return "OPEN FAILED: " + err;
    }
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, nullptr);
    std::string result = "no rows";
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

// Bilinear interpolation of a FieldGrid at fractional position (fx, fy).
// Mirrors ChildGenerator's private helper of the same name/shape.
float bilinear(const FieldGrid& g, double fx, double fy) {
    fx = std::max(0.0, std::min(static_cast<double>(g.w - 1), fx));
    fy = std::max(0.0, std::min(static_cast<double>(g.h - 1), fy));
    const int x0 = static_cast<int>(fx), y0 = static_cast<int>(fy);
    const int x1 = std::min(x0 + 1, g.w - 1), y1 = std::min(y0 + 1, g.h - 1);
    const double tx = fx - x0, ty = fy - y0;
    const double v00 = g.at(x0, y0), v10 = g.at(x1, y0);
    const double v01 = g.at(x0, y1), v11 = g.at(x1, y1);
    return static_cast<float>(v00 + (v10 - v00) * tx + (v01 - v00) * ty
                              + (v11 - v10 - v01 + v00) * tx * ty);
}

// M097 — MapPipeline looks up Lua map generators through the process-wide
// LuaGeneratorRegistry::instance() singleton (mirrors ChunkPipeline). This
// guard registers a source for the test's scope and always unregisters it
// on destruction, so other tests in the same binary never see it — the
// registry has no per-test reset otherwise.
// 2026-07-12: saves/restores whatever was already registered under `id`
// (rather than blindly erasing on destruction) -- MAP18 added real,
// ambient, disk-loaded generators for almost every map level, so a guard
// built on an id that happens to collide with one of those would otherwise
// permanently delete the real production registration for the rest of the
// test binary's run.
class ScopedLuaMapGenerator {
public:
    ScopedLuaMapGenerator(std::string id, const std::string& source) : id_(std::move(id)) {
        auto& registry = LuaGeneratorRegistry::instance();
        had_previous_   = registry.has(id_);
        if (had_previous_) previous_source_ = registry.get(id_);
        registry.register_source(id_, source);
    }
    ~ScopedLuaMapGenerator() {
        auto& registry = LuaGeneratorRegistry::instance();
        if (had_previous_) registry.register_source(id_, previous_source_);
        else registry.unregister_source(id_);
    }

private:
    std::string id_;
    bool had_previous_ = false;
    std::string previous_source_;
};

// 2026-07-12 -- MAP18 gave almost every map level (all but level 4, as of
// this writing) a real, ambient, disk-loaded Lua generator, registered at
// binary startup under "lua.map.child.level<N>.default". MapPipeline's
// level-aware lookup always prefers a real registration over a test's own
// generic-fallback guard (by design), so any test wanting to exercise the
// "no level-specific generator is registered" fallback path can no longer
// safely assume that's true for a given N just because no test registers
// one -- real content may already claim it. This RAII helper temporarily
// force-removes whatever is registered under `id` (real or not) for the
// scope of the test, restoring it exactly on destruction, so the fallback
// mechanism itself can still be tested in isolation regardless of how much
// real Lua content the repo has accumulated.
class ScopedLuaMapGeneratorRemoved {
public:
    explicit ScopedLuaMapGeneratorRemoved(std::string id) : id_(std::move(id)) {
        auto& registry = LuaGeneratorRegistry::instance();
        had_previous_   = registry.has(id_);
        if (had_previous_) previous_source_ = registry.get(id_);
        registry.unregister_source(id_);
    }
    ~ScopedLuaMapGeneratorRemoved() {
        if (had_previous_) LuaGeneratorRegistry::instance().register_source(id_, previous_source_);
    }

private:
    std::string id_;
    bool had_previous_ = false;
    std::string previous_source_;
};

} // namespace

// --- Basic pipeline smoke test ---

TEST(MapPipelineTest, GetLevel0ProducesValidPayload) {
    auto world = PlanetWorld::create_new(tmp_dir("level0"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{0, 0, 0});
    EXPECT_EQ(p.tile, (TileCoord{0, 0, 0}));
    EXPECT_FALSE(p.elevation.empty());
    EXPECT_EQ(p.elevation.w, 64);
    EXPECT_EQ(p.elevation.h, 64);
    // MAP18 (2026-07-10) registered a real level-0 generator
    // (generators/lua/map/planet.lua), so this is now the real production
    // path, not the old "planet" C++-fallback marker.
    EXPECT_EQ(p.generator, "lua.map.planet.default");
}

TEST(MapPipelineTest, GetLevel1ProducesValidPayload) {
    auto world = PlanetWorld::create_new(tmp_dir("level1"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{1, 0, 0});
    EXPECT_FALSE(p.elevation.empty());
    // MAP18 (2026-07-10) registered a real level-1 generator
    // (generators/lua/map/level1.lua), so this is now the real production
    // path, not the old "child" C++-fallback marker.
    EXPECT_EQ(p.generator, "lua.map.child.level1.default");
}

// M060 — ChildGenerator (via the C++ fallback path) populates moisture too,
// not just PlanetGenerator at level 0; this is what silences the
// "moisture field is empty" MapValidator log seen before M060.
TEST(MapPipelineTest, ChildLevelMoistureFieldIsPopulated) {
    auto world = PlanetWorld::create_new(tmp_dir("level1_moisture"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{1, 0, 0});
    ASSERT_FALSE(p.moisture.empty());
    EXPECT_EQ(p.moisture.w, p.elevation.w);
    EXPECT_EQ(p.moisture.h, p.elevation.h);
    for (auto m : p.moisture.data) {
        EXPECT_GE(m, 0.0f);
        EXPECT_LE(m, 1.0f);
    }
}

// M113 — second get() returns the cached result (no DB re-read measurable by
// checking the payload is byte-for-byte the same object via value equality).
TEST(MapPipelineTest, CacheReturnsSamePayload) {
    auto world = PlanetWorld::create_new(tmp_dir("cache"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload a = pipe.get(TileCoord{1, 1, 0});
    const MapTilePayload b = pipe.get(TileCoord{1, 1, 0});
    ASSERT_EQ(a.elevation.data.size(), b.elevation.data.size());
    for (std::size_t i = 0; i < a.elevation.data.size(); ++i)
        EXPECT_EQ(a.elevation.data[i], b.elevation.data[i]) << "index=" << i;
}

// M218 — the tile cache is genuinely LRU (a hit refreshes recency), not just
// insertion-order (FIFO) capped. named_levels={} (nothing persisted) so an
// evicted tile MUST fully regenerate on its next get() -- no DB fallback to
// mask whether it was actually evicted; a fresh generation (root or child,
// measured separately) reliably takes >1ms, a cache hit a handful of
// microseconds -- a >100x gap, never observed to overlap.
//
// cache_capacity=2, root + one child each: get(child1) generates root (cache
// = [root]) then child1 (cache = [root, child1], now at capacity). get(child2)
// looks up its parent (root) first -- a HIT -- before child2 itself needs
// inserting past capacity, forcing exactly one eviction:
//
//   with the M218 fix (hit refreshes recency): root's hit moves it to the
//     MRU end, so child1 (never re-touched) is the one evicted.
//   without it (plain FIFO): root -- inserted first, never reordered on a
//     hit -- is always the eviction candidate instead, regardless of any
//     child's own recency.
//
// Either way something gets evicted; checking root's own speed afterward is
// what actually distinguishes the two cases, so that's what this asserts
// (well under the observed fresh-generation floor, well over the observed
// cache-hit ceiling -- see the trial run in this task's commit message).
TEST(MapPipelineTest, CacheHitOnParentProtectsItFromEviction) {
    auto world = PlanetWorld::create_new(tmp_dir("lru"));
    MapPipeline pipe(world, make_params(), std::set<int>{}, /*cache_capacity=*/2);

    const TileCoord root{0, 0, 0};
    const TileCoord child1{1, 0, 0};
    const TileCoord child2{1, 1, 0};

    pipe.get(child1);  // generates root, then child1 -- cache full at capacity 2
    pipe.get(child2);  // root: HIT (parent lookup); child2: forces one eviction

    const auto start   = std::chrono::steady_clock::now();
    pipe.get(root);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_LT(elapsed, std::chrono::microseconds(500))
        << "root should have survived (protected by child2's parent-lookup hit), "
           "not been evicted and forced to fully regenerate";
}

// M219 (MAP14) — a long simulated traverse (far more distinct tiles than
// cache_capacity holds) must never let the in-memory cache grow past that
// capacity at ANY point along the way, not just when checked once at the
// end. named_levels left at its default (everything persisted) -- a real
// traverse persists too, so an evicted-then-revisited tile's DB fallback is
// part of what this is meant to simulate, not something to route around.
TEST(MapPipelineTest, LongTraverseNeverExceedsCacheCapacity) {
    auto world = PlanetWorld::create_new(tmp_dir("long_traverse"));
    constexpr std::size_t kCapacity = 16;
    MapPipeline pipe(world, make_params(), std::nullopt, kCapacity);

    // Level 6 spans 64x64 -- 4096 distinct tiles, comfortably enough for a
    // 200-tile traverse with no coordinate ever repeating.
    constexpr int kTraverseLength = 200;
    for (int i = 0; i < kTraverseLength; ++i) {
        pipe.get(TileCoord{6, i % 64, i / 64});
        ASSERT_LE(pipe.cache_size(), kCapacity)
            << "cache exceeded its capacity after " << (i + 1) << " tile(s) visited";
    }
}

// M113 — payload survives re-open (was actually persisted to DB, not just cached).
TEST(MapPipelineTest, PayloadPersistedAndReloadable) {
    const std::string dir = tmp_dir("persist");
    {
        auto world = PlanetWorld::create_new(dir);
        MapPipeline pipe(world, make_params());
        pipe.get(TileCoord{1, 0, 1});  // generate + store
    }
    {
        auto world2 = PlanetWorld::open_existing(dir);
        MapPipeline pipe2(world2, make_params());
        // A fresh pipeline with no cache: must load from DB.
        const MapTilePayload p = pipe2.get(TileCoord{1, 0, 1});
        EXPECT_FALSE(p.elevation.empty());
        EXPECT_EQ(p.tile, (TileCoord{1, 0, 1}));
    }
}

// M117 — child's boundary elevation samples match the parent's edge descriptors
// (no fBm at boundary because fade=0 there).
TEST(MapPipelineTest, ChildBoundaryMatchesParentEdge) {
    auto world = PlanetWorld::create_new(tmp_dir("m117"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload parent = pipe.get(TileCoord{0, 0, 0});
    // Child (1,0,0) occupies the top-left (cx=0, cy=0) quadrant of the parent.
    const MapTilePayload child  = pipe.get(TileCoord{1, 0, 0});

    ASSERT_EQ(child.elevation.w, parent.elevation.w);
    const int W = child.elevation.w;
    const int H = child.elevation.h;

    // Child's N edge (gy=0): should equal bilinear interpolation of parent at
    // the top edge of the cx=0, cy=0 quadrant (parent row 0, columns 0..32).
    // Since fade=0 at gy=0, child.at(gx,0) == bilinear(parent, gx*32/63, 0).
    // parent.at(x,0) == parent.edges[0][x] by construction (M066), and
    // bilinear at integer coords returns the exact grid value.
    // We test with a tolerance of 1e-3 to absorb float rounding.
    for (int gx = 0; gx < W; ++gx) {
        const float child_n = child.elevation.at(gx, 0);
        // Parent position for this child cell (cy=0, gx).
        const double pgx_f = 0 * 32.0 + gx * (32.0 / (W - 1));
        const int    px0   = static_cast<int>(pgx_f);
        const int    px1   = std::min(px0 + 1, W - 1);
        const double tx    = pgx_f - px0;
        const float  expected = static_cast<float>(
            parent.elevation.at(px0, 0) * (1.0 - tx) +
            parent.elevation.at(px1, 0) * tx);
        EXPECT_NEAR(child_n, expected, 1e-3f)
            << "N boundary mismatch at gx=" << gx;
    }

    // Child's W edge (gx=0): fade=0, so values equal bilinear(parent, 0, gy*32/63).
    for (int gy = 0; gy < H; ++gy) {
        const float child_w = child.elevation.at(0, gy);
        const double pgy_f  = 0 * 32.0 + gy * (32.0 / (H - 1));
        const int    py0    = static_cast<int>(pgy_f);
        const int    py1    = std::min(py0 + 1, H - 1);
        const double ty     = pgy_f - py0;
        const float  expected = static_cast<float>(
            parent.elevation.at(0, py0) * (1.0 - ty) +
            parent.elevation.at(0, py1) * ty);
        EXPECT_NEAR(child_w, expected, 1e-3f)
            << "W boundary mismatch at gy=" << gy;
    }
}

// M119 — sibling generation order independence: the shared internal boundary
// between sibling (1,0,0) and (1,1,0) has identical elevation regardless of
// which sibling is generated first.
TEST(MapPipelineTest, SiblingSharedEdgeOrderIndependent) {
    // World A: generate left sibling (cx=0) first, then right (cx=1).
    const std::string dir_a = tmp_dir("sib_a");
    std::vector<float> left_right_edge_A, right_left_edge_A;
    {
        auto world = PlanetWorld::create_new(dir_a);
        MapPipeline pipe(world, make_params());
        const MapTilePayload left  = pipe.get(TileCoord{1, 0, 0});  // cx=0
        const MapTilePayload right = pipe.get(TileCoord{1, 1, 0});  // cx=1
        // Left's E edge (gx=63 column) vs Right's W edge (gx=0 column).
        for (int gy = 0; gy < left.elevation.h; ++gy) {
            left_right_edge_A.push_back(left.elevation.at(left.elevation.w - 1, gy));
            right_left_edge_A.push_back(right.elevation.at(0, gy));
        }
    }

    // World B: generate right sibling first, then left — same entropy → same parent.
    const std::string dir_b = tmp_dir("sib_b");
    // We need the same world_entropy so tiles are identical. Use the same
    // entropy trick: create fresh world, but test the invariant within one world.
    // In the same pipeline, generation order must not matter.
    std::vector<float> left_right_edge_B, right_left_edge_B;
    {
        auto world = PlanetWorld::create_new(dir_b);
        MapPipeline pipe(world, make_params());
        const MapTilePayload right = pipe.get(TileCoord{1, 1, 0});  // right first
        const MapTilePayload left  = pipe.get(TileCoord{1, 0, 0});  // then left
        for (int gy = 0; gy < left.elevation.h; ++gy) {
            left_right_edge_B.push_back(left.elevation.at(left.elevation.w - 1, gy));
            right_left_edge_B.push_back(right.elevation.at(0, gy));
        }
    }

    // Within each world: left's R edge == right's L edge (shared boundary coherence).
    ASSERT_EQ(left_right_edge_A.size(), right_left_edge_A.size());
    for (std::size_t i = 0; i < left_right_edge_A.size(); ++i) {
        EXPECT_NEAR(left_right_edge_A[i], right_left_edge_A[i], 1e-4f)
            << "World A sibling boundary mismatch at gy=" << i;
        EXPECT_NEAR(left_right_edge_B[i], right_left_edge_B[i], 1e-4f)
            << "World B sibling boundary mismatch at gy=" << i;
    }
}

// M221 (MAP14) — the concurrency version of M119's check above: real threads
// (not just varied sequential order) racing to generate all 4 level-1
// siblings -- and their shared level-0 parent -- against the SAME on-disk
// world must still leave every shared internal boundary identical. Each
// thread owns its own MapPipeline (mirrors WorldStreamer's own
// one-MapPipeline-per-worker-thread design -- see MapTileFetchQueue.hpp's
// doc comment for why sharing one across threads isn't safe), so the risk
// under test is real contention on the same map_level{N}.db files via
// MapTileStore's WAL + busy-timeout + construction mutex (§5 #22 in
// NEXT.md), not shared in-memory state -- "parallel tile generation" in
// this codebase always means this shape, never one MapPipeline shared
// across threads.
TEST(MapPipelineTest, ConcurrentGenerationPreservesSiblingBoundaryConsistency) {
    const std::string dir = tmp_dir("concurrent_boundary");
    std::filesystem::remove_all(dir);
    PlanetWorld::create_new(dir);

    // 6 threads, each repeatedly requesting all 4 level-1 siblings (and so
    // their shared level-0 parent) through an independent MapPipeline
    // against the same world directory -- maximum contention on both the
    // shared parent row and the 4 children's own rows.
    constexpr int kThreads    = 6;
    constexpr int kIterations = 8;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&dir] {
            PlanetWorld  world = PlanetWorld::open_existing(dir);
            MapPipeline  pipe(world, make_params());
            for (int i = 0; i < kIterations; ++i) {
                pipe.get(TileCoord{1, 0, 0});
                pipe.get(TileCoord{1, 1, 0});
                pipe.get(TileCoord{1, 0, 1});
                pipe.get(TileCoord{1, 1, 1});
            }
        });
    }
    for (auto& w : workers) w.join();

    // Settled state: reload everything fresh through one more independent
    // MapPipeline/PlanetWorld (not reusing any of the racing threads' own
    // in-memory caches).
    PlanetWorld    verify_world = PlanetWorld::open_existing(dir);
    MapPipeline    verify_pipe(verify_world, make_params());
    const MapTilePayload tl = verify_pipe.get(TileCoord{1, 0, 0});  // cx=0, cy=0
    const MapTilePayload tr = verify_pipe.get(TileCoord{1, 1, 0});  // cx=1, cy=0
    const MapTilePayload bl = verify_pipe.get(TileCoord{1, 0, 1});  // cx=0, cy=1
    const MapTilePayload br = verify_pipe.get(TileCoord{1, 1, 1});  // cx=1, cy=1

    const MapValidator validator;
    for (const auto& [name, tile] :
         {std::pair{"tl", tl}, {"tr", tr}, {"bl", bl}, {"br", br}}) {
        const ValidationResult vr = validator.validate(tile);
        EXPECT_TRUE(vr.ok) << name << ": " << (vr.errors.empty() ? "" : vr.errors.front());
    }

    // TL/TR and BL/BR share vertical boundaries (left's E edge == right's W edge).
    ASSERT_EQ(tl.elevation.h, tr.elevation.h);
    for (int gy = 0; gy < tl.elevation.h; ++gy) {
        EXPECT_NEAR(tl.elevation.at(tl.elevation.w - 1, gy), tr.elevation.at(0, gy), 1e-4f)
            << "TL/TR boundary mismatch at gy=" << gy;
        EXPECT_NEAR(bl.elevation.at(bl.elevation.w - 1, gy), br.elevation.at(0, gy), 1e-4f)
            << "BL/BR boundary mismatch at gy=" << gy;
    }
    // TL/BL and TR/BR share horizontal boundaries (top's S edge == bottom's N edge).
    ASSERT_EQ(tl.elevation.w, bl.elevation.w);
    for (int gx = 0; gx < tl.elevation.w; ++gx) {
        EXPECT_NEAR(tl.elevation.at(gx, tl.elevation.h - 1), bl.elevation.at(gx, 0), 1e-4f)
            << "TL/BL boundary mismatch at gx=" << gx;
        EXPECT_NEAR(tr.elevation.at(gx, tr.elevation.h - 1), br.elevation.at(gx, 0), 1e-4f)
            << "TR/BR boundary mismatch at gx=" << gx;
    }
}

// M222 (MAP14) — deletes a persisted intermediate tile directly from its
// map_level{N}.db row (leaving its parent and an already-generated child
// untouched), then confirms MapPipeline::get() regenerates it identically
// to what was there before deletion. Generation is fully deterministic
// given the world's own persisted entropy + parent chain -- this project's
// "non-reproducible worlds" rule is about time-seeding a NEW world, never
// about re-deriving a fixed seed; it says nothing about an EXISTING world's
// own tiles, which must always regenerate the same way from that world's
// own already-persisted entropy. Also confirms the child generated against
// the ORIGINAL tile still boundary-matches the REGENERATED one afterward
// (M117-style check, reusing the bilinear() helper).
TEST(MapPipelineTest, RegeneratingADeletedTileIsConsistentWithBeforeDeletion) {
    const std::string dir = tmp_dir("regen_after_delete");
    std::filesystem::remove_all(dir);
    auto world = PlanetWorld::create_new(dir);

    const TileCoord target{1, 0, 0};
    const TileCoord child{2, 0, 0};  // one of target's own children (cx=0, cy=0 quadrant)

    MapTilePayload before;
    MapTilePayload child_before;
    {
        MapPipeline pipe(world, make_params());
        before       = pipe.get(target);
        child_before = pipe.get(child);
    }
    ASSERT_FALSE(before.elevation.empty());
    ASSERT_FALSE(child_before.elevation.empty());

    // Delete just `target`'s row directly -- its parent (level 0) and its
    // own child (level 2, already generated above) stay persisted untouched.
    {
        const std::string path = dir + "/map_level" + std::to_string(target.level) + ".db";
        sqlite3*           db  = nullptr;
        ASSERT_EQ(sqlite3_open(path.c_str(), &db), SQLITE_OK);
        sqlite3_stmt* stmt = nullptr;
        ASSERT_EQ(sqlite3_prepare_v2(db, "DELETE FROM tile WHERE x = ? AND y = ?;", -1, &stmt, nullptr),
                  SQLITE_OK);
        sqlite3_bind_int64(stmt, 1, target.x);
        sqlite3_bind_int64(stmt, 2, target.y);
        ASSERT_EQ(sqlite3_step(stmt), SQLITE_DONE);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    PlanetWorld reopened = PlanetWorld::open_existing(dir);
    ASSERT_FALSE(reopened.tile_store(target.level).has(target)) << "delete didn't actually remove the row";

    // A completely fresh MapPipeline (no cache baggage) regenerates it.
    MapPipeline           fresh_pipe(reopened, make_params());
    const MapTilePayload  after = fresh_pipe.get(target);

    ASSERT_EQ(after.elevation.data.size(), before.elevation.data.size());
    for (std::size_t i = 0; i < after.elevation.data.size(); ++i)
        EXPECT_FLOAT_EQ(after.elevation.data[i], before.elevation.data[i]) << "elevation index=" << i;
    ASSERT_EQ(after.biome.data.size(), before.biome.data.size());
    for (std::size_t i = 0; i < after.biome.data.size(); ++i)
        EXPECT_EQ(after.biome.data[i], before.biome.data[i]) << "biome index=" << i;

    // The child generated against the ORIGINAL tile still boundary-matches
    // the REGENERATED one -- consistency survives the round-trip.
    ASSERT_EQ(child_before.elevation.w, after.elevation.w);
    for (int gx = 0; gx < child_before.elevation.w; ++gx) {
        const double pgx_f = gx * (32.0 / (child_before.elevation.w - 1));
        EXPECT_NEAR(child_before.elevation.at(gx, 0), bilinear(after.elevation, pgx_f, 0.0), 1e-3f)
            << "N boundary mismatch at gx=" << gx << " after regeneration";
    }
}

// Green-light gate criterion 3 (plan.md MAP0) — generalizes M117/M119 from a single
// hop/level to a REAL multi-level descent chain (root -> MAX_LEVEL), the depth a
// player's LOD zoom would actually traverse. At every hop, the child's N/W boundary
// must match bilinear interpolation of its own immediate parent (a local, per-hop
// invariant that composes transitively — no need to compare against level 0 directly).
TEST(MapPipelineTest, FullChainBoundaryContinuityLevel0ToMax) {
    auto world = PlanetWorld::create_new(tmp_dir("full_chain"));
    MapPipeline pipe(world, make_params());

    TileCoord cur{0, 0, 0};
    MapTilePayload parent = pipe.get(cur);

    for (int level = 0; level < MAX_LEVEL; ++level) {
        const TileCoord child_coord = cur.child(0, 0);
        const MapTilePayload child = pipe.get(child_coord);

        ASSERT_EQ(child.elevation.w, parent.elevation.w) << "level " << level;
        const int W = child.elevation.w;
        const int H = child.elevation.h;

        // N edge (gy=0, top-left quadrant): parent position = gx*(32/(W-1)), 0.
        for (int gx = 0; gx < W; ++gx) {
            const double pgx_f = gx * (32.0 / (W - 1));
            EXPECT_NEAR(child.elevation.at(gx, 0), bilinear(parent.elevation, pgx_f, 0.0), 1e-3f)
                << "level " << level << " elevation N boundary mismatch at gx=" << gx;
        }
        // W edge (gx=0): parent position = 0, gy*(32/(H-1)).
        for (int gy = 0; gy < H; ++gy) {
            const double pgy_f = gy * (32.0 / (H - 1));
            EXPECT_NEAR(child.elevation.at(0, gy), bilinear(parent.elevation, 0.0, pgy_f), 1e-3f)
                << "level " << level << " elevation W boundary mismatch at gy=" << gy;
        }
        // Temperature is NOT checked the same way: unlike elevation (which the
        // child computes as bilinear(parent) + fade*detail, degenerating to an
        // exact parent lookup at fade=0), the child recomputes temperature from
        // scratch via its own world-space wy (see ChildGenerator.cpp), not by
        // interpolating the parent's temperature field. The parent's lat_factor
        // is sampled at cell-center index positions, while elevation's boundary
        // convention samples at edge positions — the two don't line up, so
        // comparing child boundary temperature against bilinear(parent
        // temperature) at the same edge indices produces a spurious ~0.1-0.4°C
        // mismatch that reflects an indexing-convention difference, not a bug.
        // (Verified empirically while writing this test.) Temperature is only
        // guaranteed to converge toward the same value as resolution increases,
        // not to match exactly at any given boundary sample.
        //
        // Biome is reclassified from scratch every level (not inherited) — see
        // PlanetGenerator/ChildGenerator both calling BiomeClassifier::classify
        // per-cell. It is continuous only insofar as elevation/temperature are
        // (checked above); a strict equality assertion here would overclaim an
        // independent biome-continuity guarantee the algorithm does not provide,
        // and could spuriously fail near a classifier threshold on a sub-tolerance
        // elevation delta. Just check the ordinal is in valid range.
        for (int gx = 0; gx < W; ++gx)
            EXPECT_LE(child.biome.at(gx, 0), static_cast<std::uint8_t>(ZoneType::empty));

        cur    = child_coord;
        parent = child;
    }
}

// Green-light gate criterion 3, continued — sibling coherence at a non-trivial depth
// (M119 only checks level 1). Same structure as M119: two independently-created
// worlds, opposite generation order, checking each world's own left/right shared
// boundary is internally consistent regardless of order.
TEST(MapPipelineTest, SiblingSharedEdgeOrderIndependentDeepLevel) {
    constexpr int     LEVEL  = 8;
    constexpr int64_t BASE_X = 36, BASE_Y = 91;  // BASE_X even -> guaranteed siblings

    const std::string dir_a = tmp_dir("deep_sib_a");
    std::vector<float> left_right_edge_A, right_left_edge_A;
    {
        auto world = PlanetWorld::create_new(dir_a);
        MapPipeline pipe(world, make_params());
        const MapTilePayload left  = pipe.get(TileCoord{LEVEL, BASE_X, BASE_Y});
        const MapTilePayload right = pipe.get(TileCoord{LEVEL, BASE_X + 1, BASE_Y});
        for (int gy = 0; gy < left.elevation.h; ++gy) {
            left_right_edge_A.push_back(left.elevation.at(left.elevation.w - 1, gy));
            right_left_edge_A.push_back(right.elevation.at(0, gy));
        }
    }

    const std::string dir_b = tmp_dir("deep_sib_b");
    std::vector<float> left_right_edge_B, right_left_edge_B;
    {
        auto world = PlanetWorld::create_new(dir_b);
        MapPipeline pipe(world, make_params());
        const MapTilePayload right = pipe.get(TileCoord{LEVEL, BASE_X + 1, BASE_Y});  // right first
        const MapTilePayload left  = pipe.get(TileCoord{LEVEL, BASE_X, BASE_Y});
        for (int gy = 0; gy < left.elevation.h; ++gy) {
            left_right_edge_B.push_back(left.elevation.at(left.elevation.w - 1, gy));
            right_left_edge_B.push_back(right.elevation.at(0, gy));
        }
    }

    ASSERT_EQ(left_right_edge_A.size(), right_left_edge_A.size());
    for (std::size_t i = 0; i < left_right_edge_A.size(); ++i) {
        EXPECT_NEAR(left_right_edge_A[i], right_left_edge_A[i], 1e-4f)
            << "World A deep sibling boundary mismatch at gy=" << i;
        EXPECT_NEAR(left_right_edge_B[i], right_left_edge_B[i], 1e-4f)
            << "World B deep sibling boundary mismatch at gy=" << i;
    }
}

// Recursive descent generates all ancestor levels automatically.
TEST(MapPipelineTest, RecursiveDescentGeneratesAncestors) {
    auto world = PlanetWorld::create_new(tmp_dir("recurse"));
    MapPipeline pipe(world, make_params());

    // Requesting level 3 should auto-generate levels 0, 1, 2.
    const MapTilePayload p3 = pipe.get(TileCoord{3, 3, 2});
    EXPECT_FALSE(p3.elevation.empty());

    // All ancestors should now be in the DB.
    for (int lvl = 0; lvl <= 3; ++lvl) {
        // The ancestor coord at each level:
        const TileCoord anc{lvl,
                            TileCoord{3, 3, 2}.x >> (3 - lvl),
                            TileCoord{3, 3, 2}.y >> (3 - lvl)};
        EXPECT_TRUE(world.tile_store(lvl).has(anc))
            << "ancestor " << lvl << " not persisted";
    }
}

// --- M114/M120: only named levels persist ---

// M120 — descent through a deep tile only persists the named ancestor
// levels; the non-named intermediate levels are never written to their
// tile_store at all.
TEST(MapPipelineTest, OnlyNamedLevelsPersist) {
    auto world = PlanetWorld::create_new(tmp_dir("m114_named"));
    const std::set<int> named = {0, 2, 4};
    MapPipeline pipe(world, make_params(), named);

    const TileCoord deep{4, 3, 2};
    const MapTilePayload p = pipe.get(deep);
    EXPECT_FALSE(p.elevation.empty());

    for (int lvl : {0, 2, 4}) {
        const TileCoord anc{lvl, deep.x >> (4 - lvl), deep.y >> (4 - lvl)};
        EXPECT_TRUE(world.tile_store(lvl).has(anc)) << "named level " << lvl << " should be persisted";
    }
    for (int lvl : {1, 3}) {
        const TileCoord anc{lvl, deep.x >> (4 - lvl), deep.y >> (4 - lvl)};
        EXPECT_FALSE(world.tile_store(lvl).has(anc))
            << "non-named level " << lvl << " should NOT be persisted";
    }
}

// With no named_levels argument, behavior is unchanged from before M114:
// every level persists (this is just RecursiveDescentGeneratesAncestors
// re-asserted using the 3-arg constructor path explicitly, for clarity).
TEST(MapPipelineTest, DefaultConstructorStillPersistsEveryLevel) {
    auto world = PlanetWorld::create_new(tmp_dir("m114_default"));
    MapPipeline pipe(world, make_params());  // named_levels defaults to std::nullopt

    const TileCoord deep{3, 3, 2};
    pipe.get(deep);
    for (int lvl = 0; lvl <= 3; ++lvl) {
        const TileCoord anc{lvl, deep.x >> (3 - lvl), deep.y >> (3 - lvl)};
        EXPECT_TRUE(world.tile_store(lvl).has(anc)) << "level " << lvl << " not persisted";
    }
}

// M114 — a non-named level is regenerated deterministically (same world
// entropy + same persisted named ancestor) rather than merely "different but
// still valid": two independent MapPipeline instances (fresh in-memory
// cache each, same on-disk world) must return byte-identical elevation for
// the same non-named tile.
TEST(MapPipelineTest, NonNamedLevelRegeneratesIdenticallyAcrossPipelines) {
    const std::string dir = tmp_dir("m114_recompute");
    const std::set<int> named = {0, 2};

    MapTilePayload first;
    {
        auto world = PlanetWorld::create_new(dir);
        MapPipeline pipe(world, make_params(), named);
        first = pipe.get(TileCoord{1, 0, 0});  // level 1: not named
    }
    {
        // Confirm it really wasn't persisted.
        auto world = PlanetWorld::open_existing(dir);
        EXPECT_FALSE(world.tile_store(1).has(TileCoord{1, 0, 0}));
    }

    MapTilePayload second;
    {
        auto world = PlanetWorld::open_existing(dir);  // same entropy, fresh cache
        MapPipeline pipe(world, make_params(), named);
        second = pipe.get(TileCoord{1, 0, 0});
    }

    ASSERT_EQ(first.elevation.data.size(), second.elevation.data.size());
    for (std::size_t i = 0; i < first.elevation.data.size(); ++i)
        EXPECT_FLOAT_EQ(first.elevation.data[i], second.elevation.data[i]) << "index=" << i;
}

// --- M097: Lua-first, C++-fallback ---

// A registered "lua.map.planet.default" is used instead of PlanetGenerator.
TEST(MapPipelineTest, UsesLuaGeneratorForLevel0WhenRegistered) {
    constexpr const char* LUA_SRC = R"lua(
local M = {}
M.id = "lua.map.planet.default"
function M.generate(ctx, map)
    map:setBiomeField(2, 2, {10, 10, 10, 10}, {20, 20, 20, 20}, {0.5, 0.5, 0.5, 0.5})
    map:setMetadata(M.id, "nordic")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.planet.default", LUA_SRC);

    auto world = PlanetWorld::create_new(tmp_dir("lua_l0"));
    MapPipeline pipe(world, make_params());
    const MapTilePayload p = pipe.get(TileCoord{0, 0, 0});

    EXPECT_EQ(p.generator, "lua.map.planet.default");
    EXPECT_EQ(p.culture, "nordic");
    EXPECT_EQ(p.elevation.w, 2);
    EXPECT_EQ(p.elevation.h, 2);
}

// A Lua generator error falls back to PlanetGenerator, not a crash/empty payload.
TEST(MapPipelineTest, FallsBackToCppWhenLuaGeneratorErrors) {
    constexpr const char* BROKEN_LUA_SRC = R"lua(
local M = {}
M.id = "lua.map.planet.default"
function M.generate(ctx, map)
    error("intentional failure")
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.planet.default", BROKEN_LUA_SRC);

    auto world = PlanetWorld::create_new(tmp_dir("lua_l0_broken"));
    MapPipeline pipe(world, make_params());
    const MapTilePayload p = pipe.get(TileCoord{0, 0, 0});

    EXPECT_EQ(p.generator, "planet");  // C++ fallback, unaffected by the error
    EXPECT_EQ(p.elevation.w, 64);
    EXPECT_EQ(p.elevation.h, 64);
}

// A Lua generator that runs but leaves fields empty also falls back to C++
// (mirrors ChunkPipeline treating empty output as failure).
TEST(MapPipelineTest, FallsBackToCppWhenLuaGeneratorProducesEmptyFields) {
    constexpr const char* NOOP_LUA_SRC = R"lua(
local M = {}
M.id = "lua.map.planet.default"
function M.generate(ctx, map)
    -- intentionally does not call map:setBiomeField
end
return M
)lua";
    ScopedLuaMapGenerator guard("lua.map.planet.default", NOOP_LUA_SRC);

    auto world = PlanetWorld::create_new(tmp_dir("lua_l0_empty"));
    MapPipeline pipe(world, make_params());
    const MapTilePayload p = pipe.get(TileCoord{0, 0, 0});

    EXPECT_EQ(p.generator, "planet");
    EXPECT_FALSE(p.elevation.empty());
}

// M108 — a Lua child generator that deliberately ignores ctx.parent/ctx.edges
// (constant elevation everywhere) still ends up with a correct N/W boundary,
// because MapBuilder::setBiomeField() enforces it as a fixed constraint. This
// exercises the enforcement through the real pipeline, not just MapBuilder
// in isolation: proves "must consume ctx.edges" holds even for a generator
// that never looks at ctx.edges at all.
TEST(MapPipelineTest, LuaChildBoundaryIsConstrainedEvenWhenScriptIgnoresParent) {
    constexpr const char* DUMB_CHILD_LUA = R"lua(
local M = {}
M.id = "lua.map.child.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do
        elev[i]  = 12345.0
        temp[i]  = 15.0
        moist[i] = 0.4
    end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "test")
end
return M
)lua";
    // Level 1 already has a real, ambient generator
    // ("lua.map.child.level1.default", MAP18) that would otherwise always
    // win over this test's own generic-id guard below.
    ScopedLuaMapGeneratorRemoved removed_ambient("lua.map.child.level1.default");
    ScopedLuaMapGenerator guard("lua.map.child.default", DUMB_CHILD_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("m108_lua_child"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload parent = pipe.get(TileCoord{0, 0, 0});
    const MapTilePayload child  = pipe.get(TileCoord{1, 0, 0});  // cx=0, cy=0

    ASSERT_EQ(child.generator, "lua.map.child.default")
        << "test is only meaningful if the Lua path actually ran (not a C++ fallback)";
    ASSERT_EQ(child.elevation.w, 4);
    ASSERT_EQ(child.elevation.h, 4);
    const int W = child.elevation.w;
    const int H = child.elevation.h;

    // Interior is untouched (the script's constant, since it isn't a boundary cell).
    EXPECT_FLOAT_EQ(child.elevation.at(2, 2), 12345.0f);

    // N boundary must match bilinear(parent) at the cx=0,cy=0 quadrant's top
    // edge — same formula as ChildBoundaryMatchesParentEdge — NOT the
    // script's 12345 constant.
    for (int gx = 0; gx < W; ++gx) {
        const double pgx_f = gx * (32.0 / (W - 1));
        const int    px0   = static_cast<int>(pgx_f);
        const int    px1   = std::min(px0 + 1, parent.elevation.w - 1);
        const double tx    = pgx_f - px0;
        const float  expected = static_cast<float>(
            parent.elevation.at(px0, 0) * (1.0 - tx) + parent.elevation.at(px1, 0) * tx);
        EXPECT_NEAR(child.elevation.at(gx, 0), expected, 1e-3f)
            << "N boundary mismatch at gx=" << gx;
        EXPECT_NE(child.elevation.at(gx, 0), 12345.0f)
            << "N boundary at gx=" << gx << " was left as the script's unconstrained value";
    }

    // W boundary, same idea.
    for (int gy = 0; gy < H; ++gy) {
        const double pgy_f = gy * (32.0 / (H - 1));
        const int    py0   = static_cast<int>(pgy_f);
        const int    py1   = std::min(py0 + 1, parent.elevation.h - 1);
        const double ty    = pgy_f - py0;
        const float  expected = static_cast<float>(
            parent.elevation.at(0, py0) * (1.0 - ty) + parent.elevation.at(0, py1) * ty);
        EXPECT_NEAR(child.elevation.at(0, gy), expected, 1e-3f)
            << "W boundary mismatch at gy=" << gy;
        EXPECT_NE(child.elevation.at(0, gy), 12345.0f)
            << "W boundary at gy=" << gy << " was left as the script's unconstrained value";
    }
}

// M115 — a level-specific Lua generator id ("lua.map.child.level3.default")
// is tried first for that exact level and does NOT apply to other levels;
// a level with no level-specific registration falls back to the generic
// "lua.map.child.default" id, unchanged from before this task.
TEST(MapPipelineTest, LevelSpecificLuaGeneratorOnlyAppliesToItsOwnLevel) {
    constexpr const char* LEVEL3_LUA = R"lua(
local M = {}
M.id = "lua.map.child.level3.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 777.0; temp[i] = 10.0; moist[i] = 0.3 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "test")
end
return M
)lua";
    constexpr const char* GENERIC_CHILD_LUA = R"lua(
local M = {}
M.id = "lua.map.child.default"
function M.generate(ctx, map)
    local n = 4
    local elev, temp, moist = {}, {}, {}
    for i = 1, n * n do elev[i] = 111.0; temp[i] = 10.0; moist[i] = 0.3 end
    map:setBiomeField(n, n, elev, temp, moist)
    map:setMetadata(M.id, "test")
end
return M
)lua";
    // Levels 1 and 2 already have real, ambient generators (MAP18) that
    // would otherwise always win over the generic_guard fallback below.
    ScopedLuaMapGeneratorRemoved removed_level1("lua.map.child.level1.default");
    ScopedLuaMapGeneratorRemoved removed_level2("lua.map.child.level2.default");
    ScopedLuaMapGenerator level3_guard("lua.map.child.level3.default", LEVEL3_LUA);
    ScopedLuaMapGenerator generic_guard("lua.map.child.default", GENERIC_CHILD_LUA);

    auto world = PlanetWorld::create_new(tmp_dir("m115_level_aware"));
    MapPipeline pipe(world, make_params());

    // Level 1 and 2: no level-specific id registered -> generic fallback used.
    EXPECT_EQ(pipe.get(TileCoord{1, 0, 0}).generator, "lua.map.child.default");
    EXPECT_EQ(pipe.get(TileCoord{2, 0, 0}).generator, "lua.map.child.default");
    // Level 3: level-specific id IS registered -> used instead of the generic one.
    EXPECT_EQ(pipe.get(TileCoord{3, 0, 0}).generator, "lua.map.child.level3.default");
    // Level 4: back to the generic fallback (no level-4-specific id registered).
    EXPECT_EQ(pipe.get(TileCoord{4, 0, 0}).generator, "lua.map.child.default");
}

// M115 — the real generators/lua/map/continent.lua, registered under its
// own declared id (mirrors what ContentPackLoader/MeshWorldPack do in
// production), is picked up by MapPipeline ONLY at level 3 end-to-end —
// not a hand-written inline script like LevelSpecificLuaGeneratorOnlyAppliesToItsOwnLevel above.
TEST(MapPipelineTest, RealContinentLuaAppliesOnlyAtLevel3) {
    std::ifstream ifs("generators/lua/map/continent.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string source = ss.str();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/continent.lua not found or empty";

    // Level 2 already has a real, ambient generator (level2.lua, MAP18)
    // that would otherwise mask the "no override" check below.
    ScopedLuaMapGeneratorRemoved removed_level2("lua.map.child.level2.default");
    ScopedLuaMapGenerator guard("lua.map.child.level3.default", source);

    auto world = PlanetWorld::create_new(tmp_dir("m115_real_continent"));
    MapPipeline pipe(world, make_params());

    EXPECT_EQ(pipe.get(TileCoord{2, 0, 0}).generator, "child");   // no lvl-2 id -> C++ fallback
    const MapTilePayload lvl3 = pipe.get(TileCoord{3, 0, 0});
    EXPECT_EQ(lvl3.generator, "lua.map.child.level3.default");
    EXPECT_FALSE(lvl3.elevation.empty());
    EXPECT_EQ(pipe.get(TileCoord{4, 0, 0}).generator, "child");   // no lvl-4 id -> C++ fallback
}

// M116 — same end-to-end check as M115's, for the real region.lua at level 7.
TEST(MapPipelineTest, RealRegionLuaAppliesOnlyAtLevel7) {
    std::ifstream ifs("generators/lua/map/region.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string source = ss.str();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/region.lua not found or empty";

    // Levels 6 and 8 already have real, ambient generators
    // (trunk_network.lua, level8.lua, MAP18) that would otherwise mask the
    // "no override" checks below.
    ScopedLuaMapGeneratorRemoved removed_level6("lua.map.child.level6.default");
    ScopedLuaMapGeneratorRemoved removed_level8("lua.map.child.level8.default");
    ScopedLuaMapGenerator guard("lua.map.child.level7.default", source);

    auto world = PlanetWorld::create_new(tmp_dir("m116_real_region"));
    MapPipeline pipe(world, make_params());

    EXPECT_EQ(pipe.get(TileCoord{6, 0, 0}).generator, "child");   // no lvl-6 id -> C++ fallback
    const MapTilePayload lvl7 = pipe.get(TileCoord{7, 0, 0});
    EXPECT_EQ(lvl7.generator, "lua.map.child.level7.default");
    EXPECT_FALSE(lvl7.elevation.empty());
    EXPECT_EQ(pipe.get(TileCoord{8, 0, 0}).generator, "child");   // no lvl-8 id -> C++ fallback
}

// M146 — same end-to-end check as M115's/M116's, for the real country.lua
// at level 5.
TEST(MapPipelineTest, RealCountryLuaAppliesOnlyAtLevel5) {
    std::ifstream ifs("generators/lua/map/country.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string source = ss.str();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/country.lua not found or empty";

    // Level 6 already has a real, ambient generator (trunk_network.lua,
    // MAP18) that would otherwise mask the "no override" check below.
    // Level 4 remains genuinely uncovered by any real generator today.
    ScopedLuaMapGeneratorRemoved removed_level6("lua.map.child.level6.default");
    ScopedLuaMapGenerator guard("lua.map.child.level5.default", source);

    auto world = PlanetWorld::create_new(tmp_dir("m146_real_country"));
    MapPipeline pipe(world, make_params());

    EXPECT_EQ(pipe.get(TileCoord{4, 0, 0}).generator, "child");   // no lvl-4 id -> C++ fallback
    const MapTilePayload lvl5 = pipe.get(TileCoord{5, 0, 0});
    EXPECT_EQ(lvl5.generator, "lua.map.child.level5.default");
    EXPECT_FALSE(lvl5.elevation.empty());
    EXPECT_EQ(pipe.get(TileCoord{6, 0, 0}).generator, "child");   // no lvl-6 id -> C++ fallback
}

// M147 — same end-to-end check as M146's, for the real metro.lua at level 9.
TEST(MapPipelineTest, RealMetroLuaAppliesOnlyAtLevel9) {
    std::ifstream ifs("generators/lua/map/metro.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string source = ss.str();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/metro.lua not found or empty";

    // Levels 8 and 10 already have real, ambient generators (level8.lua,
    // level10.lua, MAP18) that would otherwise mask the "no override"
    // checks below.
    ScopedLuaMapGeneratorRemoved removed_level8("lua.map.child.level8.default");
    ScopedLuaMapGeneratorRemoved removed_level10("lua.map.child.level10.default");
    ScopedLuaMapGenerator guard("lua.map.child.level9.default", source);

    auto world = PlanetWorld::create_new(tmp_dir("m147_real_metro"));
    MapPipeline pipe(world, make_params());

    EXPECT_EQ(pipe.get(TileCoord{8, 0, 0}).generator, "child");   // no lvl-8 id -> C++ fallback
    const MapTilePayload lvl9 = pipe.get(TileCoord{9, 0, 0});
    EXPECT_EQ(lvl9.generator, "lua.map.child.level9.default");
    EXPECT_FALSE(lvl9.elevation.empty());
    EXPECT_EQ(pipe.get(TileCoord{10, 0, 0}).generator, "child"); // no lvl-10 id -> C++ fallback
}

// M153 — same end-to-end check as M146's/M147's, for the real city.lua at
// level 12.
TEST(MapPipelineTest, RealCityLuaAppliesOnlyAtLevel12) {
    std::ifstream ifs("generators/lua/map/city.lua");
    std::ostringstream ss;
    ss << ifs.rdbuf();
    const std::string source = ss.str();
    ASSERT_FALSE(source.empty()) << "generators/lua/map/city.lua not found or empty";

    // Levels 11 and 13 already have real, ambient generators (district.lua,
    // level13.lua, MAP18) that would otherwise mask the "no override"
    // checks below.
    ScopedLuaMapGeneratorRemoved removed_level11("lua.map.child.level11.default");
    ScopedLuaMapGeneratorRemoved removed_level13("lua.map.child.level13.default");
    ScopedLuaMapGenerator guard("lua.map.child.level12.default", source);

    auto world = PlanetWorld::create_new(tmp_dir("m153_real_city"));
    MapPipeline pipe(world, make_params());

    // R105 supplies a dedicated native DistrictGenerator when Lua is absent.
    EXPECT_EQ(pipe.get(TileCoord{11, 0, 0}).generator, "cpp.map.district");
    const MapTilePayload lvl12 = pipe.get(TileCoord{12, 0, 0});
    EXPECT_EQ(lvl12.generator, "lua.map.child.level12.default");
    EXPECT_FALSE(lvl12.elevation.empty());
    EXPECT_EQ(pipe.get(TileCoord{13, 0, 0}).generator, "child"); // no lvl-13 id -> C++ fallback
}

// With no "lua.map.child.default"/level-specific id registered for level 1,
// it falls back to ChildGenerator, exactly as before this task. Level 1 now
// has a real, ambient generator (level1.lua, MAP18), so it must be
// temporarily removed for this test to still exercise the "nothing
// registered at all" path it's named for.
TEST(MapPipelineTest, Level1StillUsesCppChildGeneratorWithNoLuaRegistered) {
    ScopedLuaMapGeneratorRemoved removed_level1("lua.map.child.level1.default");

    auto world = PlanetWorld::create_new(tmp_dir("lua_l1_absent"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{1, 0, 0});
    EXPECT_EQ(p.generator, "child");
}

// R105 — level 12 (city.lua's own level) must fall back to the dedicated
// CityGenerator, not the generic ChildGenerator, when no Lua override is
// registered — see plan.md's R105 entry for why this level got its own
// native C++ generator instead of the generic fallback every other level
// still uses.
TEST(MapPipelineTest, Level12UsesCppCityGeneratorWithNoLuaRegistered) {
    ScopedLuaMapGeneratorRemoved removed_level12("lua.map.child.level12.default");

    auto world = PlanetWorld::create_new(tmp_dir("lua_l12_absent"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{12, 0, 0});
    EXPECT_EQ(p.generator, "cpp.map.city");
}

// R105 — level 11 (district.lua's own level) must fall back to the
// dedicated DistrictGenerator, not the generic ChildGenerator, when no Lua
// override is registered.
TEST(MapPipelineTest, Level11UsesCppDistrictGeneratorWithNoLuaRegistered) {
    ScopedLuaMapGeneratorRemoved removed_level11("lua.map.child.level11.default");

    auto world = PlanetWorld::create_new(tmp_dir("lua_l11_absent"));
    MapPipeline pipe(world, make_params());

    const MapTilePayload p = pipe.get(TileCoord{11, 0, 0});
    EXPECT_EQ(p.generator, "cpp.map.district");
}

// M220 (MAP14) — repeatedly descending to MAX_LEVEL at many different
// locations against the same world must never leave any map_level{N}.db
// file structurally corrupt, and every generated tile must still be
// loadable afterward. Distinct from M219 (that's about bounded memory/
// handle *counts*; this is about the on-disk SQLite files themselves
// surviving heavy write traffic intact) and from M222 (regenerating a
// *deleted* tile correctly -- this test never deletes anything).
//
// Locations are deterministic but spread out (two different odd
// multipliers mod the level-18 span, not sequential x/y) so distinct
// descents exercise different rows/pages across every level's DB file,
// not just adjacent ones -- and, per this file's own non-reproducible-
// world rule, never std::random_device (which would make a failure
// unreproducible).
TEST(MapPipelineTest, ManyDeepDescentsLeaveEveryLevelDbUncorrupted) {
    const std::string world_dir = tmp_dir("deep_zoom_stress");
    // M226 investigation found this test's own timing had been measured
    // inaccurately during development because this directory was reused,
    // uncleaned, across repeated manual runs (§5 #3e's already-documented
    // tmp_dir() caveat) -- explicit cleanup here makes a fresh generation
    // actually happen (and be measured accurately) every single run, not
    // just the first one ever on a given machine.
    std::filesystem::remove_all(world_dir);
    auto        world = PlanetWorld::create_new(world_dir);
    MapPipeline pipe(world, make_params());

    constexpr int     kNumLocations = 20;
    const std::int64_t span         = std::int64_t{1} << MAX_LEVEL;

    std::vector<TileCoord> visited;
    visited.reserve(kNumLocations);
    for (int i = 0; i < kNumLocations; ++i) {
        const std::int64_t x = (static_cast<std::int64_t>(i) * 92821) % span;
        const std::int64_t y = (static_cast<std::int64_t>(i) * 63247) % span;
        const TileCoord     tile{MAX_LEVEL, x, y};
        ASSERT_FALSE(pipe.get(tile).elevation.empty());
        visited.push_back(tile);
    }

    // Every level from 0 to MAX_LEVEL now has a map_level{N}.db file (the
    // descents above generated the full ancestor chain down to each
    // location). Check each one fresh -- an independent, read-only
    // connection, not anything MapPipeline/MapTileStore already has open.
    for (int level = 0; level <= MAX_LEVEL; ++level) {
        const std::string path = world_dir + "/map_level" + std::to_string(level) + ".db";
        EXPECT_EQ(sqlite_integrity_check(path), "ok") << "level " << level << " (" << path << ")";
    }

    // Every originally-visited MAX_LEVEL tile is still loadable, via a
    // completely fresh PlanetWorld/MapPipeline (its own empty in-memory
    // cache, so this only ever reads from disk).
    PlanetWorld verify_world = PlanetWorld::open_existing(world_dir);
    MapPipeline verify_pipe(verify_world, make_params());
    for (const TileCoord& tile : visited) {
        const MapTilePayload p = verify_pipe.get(tile);
        EXPECT_FALSE(p.elevation.empty())
            << "tile L" << tile.level << " (" << tile.x << "," << tile.y << ") failed to reload";
    }
}
