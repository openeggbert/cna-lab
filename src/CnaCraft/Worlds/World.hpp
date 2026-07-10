#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Chunk.hpp"

namespace CnaCraft::Worlds {

// Streamed world (plan.md §12.1 item 19, user decision 2026-07-10: pursue
// an unbounded world matching Craft's real chunk model, superseding the
// project's earlier fixed-size-grid prototype boundary). Chunks are loaded
// on demand, keyed by 2D chunk-column address (cx,cz) — matches Craft's own
// behavior of only ever streaming X/Z, never Y (its `Map` covers the full
// height range for whichever columns are loaded). WORLD_CHUNKS_Y (and the
// derived WORLD_SIZE_Y) stays a real, enforced Y bound; WORLD_CHUNKS_X/Z and
// WORLD_SIZE_X/Z below are now legacy/advisory constants only — they
// describe the region World::Generate(seed) fills for backward test/tooling
// compatibility, not a hard bound (an (x,z) far outside them is perfectly
// valid once its column is loaded).
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

// Packed (cx,cz) chunk-column address, used as the key for World's chunk
// storage. Two plain ints (not one 64-bit key) would work too, but a single
// hashable key keeps std::unordered_map usage simple.
using ColumnKey = std::int64_t;

class World {
public:
    World();

    // Legacy whole-region convenience wrapper: generates the same
    // WORLD_CHUNKS_X x WORLD_CHUNKS_Z column region this project's world
    // always covered before streaming existed, by looping GenerateColumn
    // below. Kept for backward test/tooling compatibility — CnaCraftGame
    // itself calls GenerateColumn directly, on demand, once chunk streaming
    // is wired up (a later phase of plan.md §12.1 item 19's redesign).
    void Generate(std::uint32_t seed);

    // Generates one chunk-column's terrain + decorations (bedrock/stone/
    // dirt/grass up to a NoiseGenerator-derived height, then trees/grass/
    // flowers/clouds), allocating it first if not already loaded.
    // Deterministic per (seed,cx,cz), with no dependency on any other
    // column already existing — matches Craft's own create_world exactly
    // (world.c), including a margin check that skips tree placement if its
    // canopy would cross this column's own local x/z boundary (ports
    // Craft's real `dx-4<0 ...` check) rather than reaching into a
    // neighboring column that may not be loaded yet.
    void GenerateColumn(int cx, int cz, std::uint32_t seed);

    // Allocates a column's chunks as empty (all-Air), if not already
    // loaded — idempotent. Distinct from GenerateColumn: this is pure
    // bookkeeping (no NoiseGenerator/terrain work at all), used internally
    // by GenerateColumn and directly by callers that just need scratch
    // chunk space (e.g. tests building a hand-crafted world).
    void AllocateColumn(int cx, int cz);

    // True if a chunk-column is currently loaded (has at least been
    // allocated — doesn't distinguish "generated terrain" from "empty
    // scratch space").
    [[nodiscard]] bool IsColumnLoaded(int cx, int cz) const;

    // Unloads a chunk-column, freeing its Chunk objects. No explicit save/
    // flush happens here — player edits already persist eagerly/
    // synchronously the instant they happen (Persistence::WorldStore), so
    // there is nothing left to flush by the time a column is far enough
    // away to unload.
    void UnloadColumn(int cx, int cz);

    // World-space access. Coordinates in a column that isn't loaded (or
    // outside the enforced Y range) read as Air / are ignored on write, so
    // callers (mesher, raycast, collision) never need bounds/load-state
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

    // Highest y at (x,z) where IsCollidable is true, or -1 if the column has
    // no collidable blocks at all (including if the column isn't loaded) —
    // ports Craft's own `highest_block` (main.c), used by PlayerController's
    // `y<0` floor-catch safety net (CRAFT_PARITY.md §1.8).
    [[nodiscard]] int HighestCollidableY(int x, int z) const;

    // Player-driven-edit persistence (CRAFT_PARITY.md §4.1/§4.2, ports
    // Craft's delta-storage model — a `block(p,q,x,y,z,w)` SQLite table
    // storing only edits over the regenerated procedural terrain; this
    // project's `Persistence::WorldStore` now matches that schema exactly,
    // plan.md §12.1 item 19).
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

    // Unchecked accessor — UB if (cx,cz)'s column isn't loaded or cy is out
    // of [0, WORLD_CHUNKS_Y). For callers that already know the chunk
    // exists (World's own generation code; CnaCraftGame's dirty-chunk
    // rebuild loop, which only ever visits columns World::Generate already
    // populated). Prefer TryChunkAt when that isn't guaranteed.
    Chunk& ChunkAt(int cx, int cy, int cz);
    [[nodiscard]] const Chunk& ChunkAt(int cx, int cy, int cz) const;

    // Checked accessor — nullptr if (cx,cz)'s column isn't loaded or cy is
    // out of [0, WORLD_CHUNKS_Y). Used by GetBlock/SetBlock and any caller
    // that can't assume a chunk exists.
    [[nodiscard]] Chunk* TryChunkAt(int cx, int cy, int cz);
    [[nodiscard]] const Chunk* TryChunkAt(int cx, int cy, int cz) const;

    // Read-only access to the currently-loaded columns, for callers that
    // need to iterate all of them (CnaCraftGame's chunk-rebuild and
    // distance-based unload passes). Exposes the internal container type
    // directly -- a reasonable simplicity/YAGNI tradeoff at this project's
    // scale rather than building a bespoke iterator abstraction.
    [[nodiscard]] const std::unordered_map<ColumnKey, std::array<std::unique_ptr<Chunk>, WORLD_CHUNKS_Y>>&
    LoadedColumns() const {
        return columns_;
    }

    // Packs/unpacks a (cx,cz) chunk-column address into/from the ColumnKey
    // used by LoadedColumns() above -- exposed so callers outside World
    // (CnaCraftGame keys its own per-column ChunkRenderer storage the same
    // way) can look up/iterate using identical keys.
    static ColumnKey PackColumnKey(int cx, int cz);
    static void UnpackColumnKey(ColumnKey key, int& cx, int& cz);

    static bool InBounds(int x, int y, int z);

private:
    // Generates the base terrain (bedrock/stone/dirt-or-sand/grass-or-sand
    // up to a NoiseGenerator-derived height, air above) for one
    // already-allocated column.
    void GenerateTerrainColumn(int cx, int cz, std::uint32_t seed);

    // Places simple Wood-trunk + Leaves-canopy trees on grass columns
    // within one already-allocated column (plan.md §11.1 "Trees and
    // plants") — matches Craft's own world.c tree pass (`simplex2(x, z, 6,
    // 0.5, 2) > 0.84` trigger, 7-tall trunk, a distance<11 spherical-ish
    // canopy blob), verified against the real checkout, plus (plan.md
    // §12.1 item 19) a ported margin check skipping any tree whose canopy
    // would cross this column's own local x/z boundary — see
    // GenerateColumn's doc comment.
    void GenerateTreesColumn(int cx, int cz, std::uint32_t seed);

    // Places TallGrass plant blocks on grass columns (CRAFT_PARITY.md §3.7
    // "Non-cubic plant geometry") — matches Craft's own world.c grass-
    // decoration trigger (`simplex2(-x*0.1, z*0.1, 4, 0.8, 2) > 0.6`),
    // verified against the real checkout. Purely per-column, no neighbor
    // reach at all.
    void GenerateGrassDecorationColumn(int cx, int cz, std::uint32_t seed);

    // Places Flower plant blocks on grass columns (CRAFT_PARITY.md §3.7
    // follow-up) — matches Craft's own world.c flower-decoration trigger
    // (`simplex2(x*0.05, -z*0.05, 4, 0.8, 2) > 0.7`), verified against the
    // real checkout. Craft picks one of 7 flower colors via a second noise
    // sample; this project has a single representative Flower type, so that
    // second sample is skipped. Purely per-column, no neighbor reach.
    void GenerateFlowersColumn(int cx, int cz, std::uint32_t seed);

    // Places Cloud blocks in a thin band near the world ceiling via 3D
    // density noise (plan.md §11.2 "Clouds" backlog note) — matches Craft's
    // own world.c cloud pass (`simplex3(...) > 0.75`). Purely per-column.
    void GenerateCloudsColumn(int cx, int cz, std::uint32_t seed);

    std::unordered_map<ColumnKey, std::array<std::unique_ptr<Chunk>, WORLD_CHUNKS_Y>> columns_;
    std::vector<BlockEdit> recordedEdits_;
};

}
