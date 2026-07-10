#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Chunk.hpp"

namespace CnaCraft::Worlds {

// Fixed-size world (no infinite streaming — see plan.md §1/§9): a bounded
// grid of chunks generated once at startup.
constexpr int WORLD_CHUNKS_X = 8;
constexpr int WORLD_CHUNKS_Y = 4;
constexpr int WORLD_CHUNKS_Z = 8;

constexpr int WORLD_SIZE_X = WORLD_CHUNKS_X * CHUNK_SIZE;
constexpr int WORLD_SIZE_Y = WORLD_CHUNKS_Y * CHUNK_SIZE;
constexpr int WORLD_SIZE_Z = WORLD_CHUNKS_Z * CHUNK_SIZE;

// A single player-driven block edit, recorded for persistence
// (CRAFT_PARITY.md §4.1/§4.2) — see World::SetBlockAndRecordEdit.
struct BlockEdit {
    int x = 0, y = 0, z = 0;
    BlockType type = BlockType::Air;
};

class World {
public:
    World();

    // Fills every column with bedrock/stone/dirt/grass up to a
    // NoiseGenerator-derived height, air above. Deterministic per seed.
    void Generate(std::uint32_t seed);

    // World-space access. Out-of-bounds coordinates read as Air / are ignored
    // on write, so callers (mesher, raycast, collision) never need bounds
    // special-casing.
    [[nodiscard]] BlockType GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, BlockType type);
    [[nodiscard]] bool IsSolid(int x, int y, int z) const;

    // Whether this cell occludes a neighboring face (plan.md §11.2
    // "Transparency for glass") — true for ordinary solid blocks, false for
    // Air *and* for solid-but-transparent blocks like Glass. Used only by
    // ChunkMesher's neighbor-face check; collision (IsSolid) is unaffected.
    [[nodiscard]] bool IsOpaque(int x, int y, int z) const;

    // Whether this cell physically blocks the player (plan.md §11.2
    // "Clouds") — true for ordinary solid blocks, false for Air *and* for
    // solid-but-walk-through blocks like Cloud. Used only by
    // PlayerController::CollidesAt; meshing/occlusion (IsSolid/IsOpaque) are
    // unaffected — Craft's clouds still occlude neighbors and are still
    // hit-testable/breakable, they just don't stop the player.
    [[nodiscard]] bool IsCollidable(int x, int y, int z) const;

    // Whether this cell can be broken by the player (CRAFT_PARITY.md §2.5)
    // — true for ordinary solid blocks, false for Air *and* for
    // solid-but-permanent blocks like Bedrock. Used only by CnaCraftGame's
    // left-click handler; meshing/occlusion/collision are unaffected.
    [[nodiscard]] bool IsBreakable(int x, int y, int z) const;

    // Player-driven-edit persistence (CRAFT_PARITY.md §4.1/§4.2, ports
    // Craft's delta-storage model — a `block(p,q,x,y,z,w)` SQLite table
    // storing only edits over the regenerated procedural terrain; this
    // project's `Persistence::WorldStore` adapts that schema to a plain
    // `block(x,y,z,w)` table since cna-craft has no chunk (p,q) addressing
    // at the World level, unlike Craft's streamed-chunk model).
    //
    // Plain SetBlock() does NOT record an edit — it's used for both world
    // generation (which must never be persisted) and applying already-
    // loaded edits back (which would be redundant to re-record). Only
    // *player*-initiated changes (break/place in CnaCraftGame) should call
    // this instead.
    void SetBlockAndRecordEdit(int x, int y, int z, BlockType type);

    // Edits recorded since the last ClearRecordedEdits() call —
    // Persistence::WorldStore::SaveEdits() reads this to know what to
    // write, then clears it.
    [[nodiscard]] const std::vector<BlockEdit>& RecordedEdits() const { return recordedEdits_; }
    void ClearRecordedEdits() { recordedEdits_.clear(); }

    Chunk& ChunkAt(int cx, int cy, int cz);
    [[nodiscard]] const Chunk& ChunkAt(int cx, int cy, int cz) const;

    static bool ChunkInBounds(int cx, int cy, int cz);
    static bool InBounds(int x, int y, int z);

private:
    // Places simple Wood-trunk + Leaves-canopy trees on grass columns after
    // the main terrain pass (plan.md §11.1 "Trees and plants") — matches
    // Craft's own world.c tree pass (`simplex2(x, z, 6, 0.5, 2) > 0.84`
    // trigger, 7-tall trunk, a distance<11 spherical-ish canopy blob),
    // verified against the real checkout. Called before GenerateClouds so
    // the cloud pass's "only place over Air" guard never overwrites a tall
    // tree's canopy near the world ceiling.
    void GenerateTrees(std::uint32_t seed);

    // Places TallGrass plant blocks on grass columns (CRAFT_PARITY.md §3.7
    // "Non-cubic plant geometry") — matches Craft's own world.c grass-
    // decoration trigger (`simplex2(-x*0.1, z*0.1, 4, 0.8, 2) > 0.6`),
    // verified against the real checkout. Called after GenerateTrees (a
    // minor reordering vs. Craft's literal same-pass ordering — harmless
    // here since this project places TallGrass one cell above the surface,
    // not on it, so it never actually contends with a tree trunk cell for
    // the same position the way Craft's own single-Y-level model does).
    void GenerateGrassDecoration(std::uint32_t seed);

    // Places Flower plant blocks on grass columns (CRAFT_PARITY.md §3.7
    // follow-up) — matches Craft's own world.c flower-decoration trigger
    // (`simplex2(x*0.05, -z*0.05, 4, 0.8, 2) > 0.7`), verified against the
    // real checkout. Craft picks one of 7 flower colors via a second noise
    // sample; this project has a single representative Flower type, so that
    // second sample is skipped. Called after GenerateGrassDecoration with
    // the same "only place over Air" guard, so a column where both the
    // grass and flower triggers fire keeps whichever was placed first
    // rather than Craft's literal unconditional-overwrite order.
    void GenerateFlowers(std::uint32_t seed);

    // Places Cloud blocks in a thin band near the world ceiling via 3D
    // density noise (plan.md §11.2 "Clouds" backlog note) — matches Craft's
    // own world.c cloud pass (`simplex3(...) > 0.75`), not a cave/overhang
    // carve (see plan.md §11.1 correction: the real Craft checkout has no
    // cave-carving code).
    void GenerateClouds(std::uint32_t seed);

    [[nodiscard]] int ChunkIndex(int cx, int cy, int cz) const;

    std::vector<std::unique_ptr<Chunk>> chunks_;
    std::vector<BlockEdit> recordedEdits_;
};

}
