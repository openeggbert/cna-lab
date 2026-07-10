// Plain-assert smoke test for CnaCraftWorlds (no CNA/GPU/display needed).
// Exercises Chunk/World/NoiseGenerator/ChunkMesher/VoxelRaycast/PlayerController
// end to end. Run via ctest, or directly as a binary.

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "CnaCraft/Core/Vec3.hpp"
#include "CnaCraft/Worlds/BlockType.hpp"
#include "CnaCraft/Worlds/Chunk.hpp"
#include "CnaCraft/Worlds/ChunkMesher.hpp"
#include "CnaCraft/Worlds/DayNightCycle.hpp"
#include "CnaCraft/Worlds/Hotbar.hpp"
#include "CnaCraft/Worlds/NoiseGenerator.hpp"
#include "CnaCraft/Worlds/PlayerController.hpp"
#include "CnaCraft/Worlds/Sign.hpp"
#include "CnaCraft/Worlds/VoxelRaycast.hpp"
#include "CnaCraft/Worlds/World.hpp"

using namespace CnaCraft::Core;
using namespace CnaCraft::Worlds;

namespace {

int g_failures = 0;

void Check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        ++g_failures;
    } else {
        std::printf("ok:   %s\n", label);
    }
}

// Test helper (plan.md §12.1 item 19): several older tests hand-build a
// flat floor/wall spanning the whole legacy WORLD_SIZE_X x WORLD_SIZE_Z
// region (a convention from before World streamed chunks on demand).
// Allocates that same WORLD_CHUNKS_X x WORLD_CHUNKS_Z column region so
// SetBlock calls across it succeed, same shape World::Generate's own
// legacy wrapper loops.
void AllocateAllLegacyColumns(World& world) {
    for (int cx = 0; cx < WORLD_CHUNKS_X; ++cx) {
        for (int cz = 0; cz < WORLD_CHUNKS_Z; ++cz) {
            world.AllocateColumn(cx, cz);
        }
    }
}

void TestChunkBasics() {
    Chunk chunk;
    Check(chunk.GetBlock(0, 0, 0) == BlockType::Air, "new chunk is all air");
    chunk.SetBlock(1, 2, 3, BlockType::Stone);
    Check(chunk.GetBlock(1, 2, 3) == BlockType::Stone, "chunk set/get round-trip");
    Check(chunk.GetBlock(2, 2, 3) == BlockType::Air, "chunk set/get does not bleed into neighbors");
    Check(chunk.GetBlock(-1, 0, 0) == BlockType::Air, "chunk out-of-bounds read returns Air");
    Check(chunk.IsDirty(), "chunk starts dirty");
    chunk.ClearDirty();
    Check(!chunk.IsDirty(), "ClearDirty clears the flag");
    chunk.SetBlock(0, 0, 0, BlockType::Dirt);
    Check(chunk.IsDirty(), "SetBlock re-dirties the chunk");
}

void TestChunkCoordOfHandlesNegativeCoordinates() {
    // plan.md §12.1 item 19: the streamed world extends in every direction
    // from spawn, so ChunkCoordOf must be a true floor-division, not plain
    // `/` (which truncates toward zero) -- matches Craft's own `chunked()`
    // (main.c) semantics for negative inputs.
    Check(ChunkCoordOf(0) == 0, "ChunkCoordOf(0) == 0");
    Check(ChunkCoordOf(15) == 0, "ChunkCoordOf(15) == 0 (last cell of chunk 0)");
    Check(ChunkCoordOf(16) == 1, "ChunkCoordOf(16) == 1 (first cell of chunk 1)");
    Check(ChunkCoordOf(-1) == -1, "ChunkCoordOf(-1) == -1, not 0 (plain truncating division would give 0)");
    Check(ChunkCoordOf(-16) == -1, "ChunkCoordOf(-16) == -1 (last cell of chunk -1)");
    Check(ChunkCoordOf(-17) == -2, "ChunkCoordOf(-17) == -2 (first cell of chunk -2)");

    Check(ChunkLocalCoordOf(0) == 0, "ChunkLocalCoordOf(0) == 0");
    Check(ChunkLocalCoordOf(15) == 15, "ChunkLocalCoordOf(15) == 15");
    Check(ChunkLocalCoordOf(16) == 0, "ChunkLocalCoordOf(16) == 0");
    Check(ChunkLocalCoordOf(-1) == 15, "ChunkLocalCoordOf(-1) == 15, not -1 (plain % would give -1)");
    Check(ChunkLocalCoordOf(-16) == 0, "ChunkLocalCoordOf(-16) == 0");
    Check(ChunkLocalCoordOf(-17) == 15, "ChunkLocalCoordOf(-17) == 15");
}

void TestColumnKeyPackingRoundTrips() {
    // plan.md §12.1 item 19: CnaCraftGame keys its own per-column
    // ChunkRenderer storage the same way World keys chunk columns, so this
    // round-trip must hold for negative coordinates too (the streamed
    // world extends in every direction from spawn).
    const int testCases[][2] = {{0, 0}, {5, -3}, {-1, -1}, {-17, 4}, {100, -100}};
    bool allRoundTrip = true;
    for (const auto& tc : testCases) {
        const ColumnKey key = World::PackColumnKey(tc[0], tc[1]);
        int cx = 0, cz = 0;
        World::UnpackColumnKey(key, cx, cz);
        if (cx != tc[0] || cz != tc[1]) allRoundTrip = false;
    }
    Check(allRoundTrip, "PackColumnKey/UnpackColumnKey round-trip correctly, including negative coordinates");
}

void TestColumnCopyRoundTrips() {
    // plan.md §12.1 item 19 phase 4: CopyColumn/AdoptColumnCopy/
    // InstallChunkCopy are the plain-copyable-data path a background
    // generation task's result crosses the thread boundary through
    // (TaskT<T>::getResultProperty() requires T copy-constructible, ruling
    // out a std::array<std::unique_ptr<Chunk>,N> result type).
    World source;
    source.GenerateColumn(2, -3, 1234);
    const auto copy = source.CopyColumn(2, -3);

    World dest;
    Check(!dest.IsColumnLoaded(2, -3), "sanity check: the destination column starts unloaded");
    dest.AdoptColumnCopy(2, -3, copy);
    Check(dest.IsColumnLoaded(2, -3), "AdoptColumnCopy loads the column");

    bool allBlocksMatch = true;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            for (int ly = 0; ly < WORLD_SIZE_Y; ++ly) {
                const int x = 2 * CHUNK_SIZE + lx, y = ly, z = -3 * CHUNK_SIZE + lz;
                if (source.GetBlock(x, y, z) != dest.GetBlock(x, y, z)) allBlocksMatch = false;
            }
        }
    }
    Check(allBlocksMatch, "every block in the copied column matches the source exactly");

    // InstallChunkCopy on its own -- a single chunk, not a whole column.
    World singleChunkDest;
    singleChunkDest.InstallChunkCopy(0, 1, 0, *source.TryChunkAt(2, 1, -3));
    Check(singleChunkDest.IsColumnLoaded(0, 0), "InstallChunkCopy allocates the target column if needed");
}

void TestWorldColumnLoadUnloadLifecycle() {
    // plan.md §12.1 item 19 phase 5: World::IsColumnLoaded/UnloadColumn are
    // the primitives CnaCraftGame's streaming loop is built on -- exercised
    // there only indirectly (via GenerateColumn/AdoptColumnCopy in
    // TestColumnCopyRoundTrips), so this test covers the plain
    // generate-then-unload lifecycle directly.
    World world;
    Check(!world.IsColumnLoaded(4, -2), "a fresh World has no columns loaded");

    world.GenerateColumn(4, -2, 1234);
    Check(world.IsColumnLoaded(4, -2), "GenerateColumn loads the column");
    Check(!world.IsColumnLoaded(4, -1), "a neighboring, never-generated column stays unloaded");

    const int x = 4 * CHUNK_SIZE + 3, z = -2 * CHUNK_SIZE + 3;
    world.SetBlock(x, 10, z, BlockType::Stone);
    Check(world.GetBlock(x, 10, z) == BlockType::Stone, "a loaded column's blocks are readable/writable");

    world.UnloadColumn(4, -2);
    Check(!world.IsColumnLoaded(4, -2), "UnloadColumn actually unloads the column");
    Check(world.GetBlock(x, 10, z) == BlockType::Air,
          "reading a cell in a now-unloaded column falls back to the graceful-degradation Air default");

    // Unloading an already-unloaded (or never-loaded) column is a safe no-op.
    world.UnloadColumn(4, -2);
    world.UnloadColumn(99, 99);
    Check(!world.IsColumnLoaded(4, -2), "double-unloading the same column stays a no-op, no crash");
}

void TestChunkMesherBoundaryRemeshOnNeighborLoad() {
    // plan.md §12.1 item 19 phase 5 ("boundary-remesh correctness"): a
    // chunk's face at the edge of the currently-loaded region is exposed
    // (its neighbor column reads as Air -- the same graceful-degradation
    // default as any other unloaded column), but once that neighbor loads
    // with a solid block against the shared boundary, re-meshing must cull
    // that now-covered face -- this is the real behavior
    // MarkNeighborColumnsDirty (CnaCraftGame) exists to trigger.
    World world;
    world.AllocateColumn(0, 0);
    // Local x=15 is the chunk's own +X edge (CHUNK_SIZE=16); its neighbor
    // column (1,0) isn't loaded yet.
    world.SetBlock(15, 5, 5, BlockType::Stone);

    ChunkMeshData beforeNeighbor = ChunkMesher::Build(world, 0, 0, 0);
    Check(beforeNeighbor.opaque.vertices.size() == 24,
          "a boundary block with no loaded neighbor exposes all 6 faces (unloaded neighbor reads as Air)");

    world.AllocateColumn(1, 0);
    world.SetBlock(16, 5, 5, BlockType::Stone); // local x=0 of column (1,0), touching the boundary
    ChunkMeshData afterNeighbor = ChunkMesher::Build(world, 0, 0, 0);
    Check(afterNeighbor.opaque.vertices.size() == 20,
          "once the neighbor column loads with a solid block across the boundary, re-meshing culls the shared face "
          "(5 of 6 faces remain)");

    // The neighbor's own mesh, built independently, must show the same
    // mutual culling on its side of the boundary.
    ChunkMeshData neighborMesh = ChunkMesher::Build(world, 1 * CHUNK_SIZE, 0, 0);
    Check(neighborMesh.opaque.vertices.size() == 20,
          "the neighbor chunk's own mesh also culls its face touching the boundary (mutual occlusion)");
}

void TestWorldBoundsAndRoundTrip() {
    World world;
    Check(world.GetBlock(-1, 0, 0) == BlockType::Air, "world out-of-range GetBlock returns Air");
    Check(!world.IsSolid(-1, 0, 0), "world out-of-range IsSolid is false");
    // Explicit guard against a real bug caught during development: Air's
    // BlockDef only overrides `solid`, so a naive `.collidable` read (default
    // true) made empty space collidable and froze the player instantly.
    // IsCollidable must AND with `solid`.
    Check(!world.IsCollidable(5, 5, 5), "Air is never collidable, regardless of BlockDef::collidable's default");

    world.AllocateColumn(1, 1); // (20,*,20)/(21,*,20) below both fall in chunk-column (1,1)
    world.SetBlock(20, 5, 20, BlockType::Stone); // crosses a chunk boundary (CHUNK_SIZE=16)
    Check(world.GetBlock(20, 5, 20) == BlockType::Stone, "world set/get round-trip across chunk boundary");
    Check(world.IsSolid(20, 5, 20), "world IsSolid true for Stone");
    Check(world.IsCollidable(20, 5, 20), "world IsCollidable true for Stone");
    Check(world.IsBreakable(20, 5, 20), "world IsBreakable true for ordinary solid blocks like Stone");

    // Regression test (CRAFT_PARITY.md §2.5): Bedrock is a cna-craft-only
    // "world-boundary, not meant to be placed" block (Craft itself has no
    // such block) that must also not be breakable -- same defensive
    // solid-AND pattern as IsOpaque/IsCollidable (see the note above).
    world.SetBlock(21, 5, 20, BlockType::Bedrock);
    Check(world.IsSolid(21, 5, 20), "Bedrock is still solid (meshed/hit-testable)");
    Check(!world.IsBreakable(21, 5, 20), "Bedrock is not breakable, unlike ordinary solid blocks");
    Check(!world.IsBreakable(5, 5, 5), "Air is never breakable, regardless of BlockDef::breakable's default");

    world.SetBlock(9999, 9999, 9999, BlockType::Stone); // out of range, must be a no-op
    Check(world.GetBlock(9999, 9999, 9999) == BlockType::Air,
          "world out-of-range SetBlock is a no-op");
}

void TestWorldRecordsEditsForPersistence() {
    // CRAFT_PARITY.md §4.1/§4.2: plain SetBlock() must NOT record an edit
    // (used by world generation and by loading already-persisted edits back
    // -- neither should be re-persisted); only SetBlockAndRecordEdit()
    // (used by CnaCraftGame's player-driven break/place) should.
    World world;
    Check(world.RecordedEdits().empty(), "a fresh World has no recorded edits");

    world.AllocateColumn(0, 0); // (1,1,1)/(2,2,2)/(3,3,3) below all fall in chunk-column (0,0)
    world.SetBlock(1, 1, 1, BlockType::Stone);
    Check(world.RecordedEdits().empty(), "plain SetBlock does not record an edit");

    world.SetBlockAndRecordEdit(2, 2, 2, BlockType::Dirt);
    Check(world.RecordedEdits().size() == 1, "SetBlockAndRecordEdit records exactly one edit");
    Check(world.GetBlock(2, 2, 2) == BlockType::Dirt, "SetBlockAndRecordEdit still applies the block change");
    const BlockEdit& edit = world.RecordedEdits()[0];
    Check(edit.x == 2 && edit.y == 2 && edit.z == 2 && edit.type == BlockType::Dirt,
          "the recorded edit has the correct coordinates and type");

    world.SetBlockAndRecordEdit(3, 3, 3, BlockType::Sand);
    Check(world.RecordedEdits().size() == 2, "a second edit accumulates rather than replacing the first");

    world.ClearRecordedEdits();
    Check(world.RecordedEdits().empty(), "ClearRecordedEdits empties the recorded-edits list");

    world.SetBlockAndRecordEdit(9999, 9999, 9999, BlockType::Stone); // out of range
    Check(world.RecordedEdits().empty(),
          "an out-of-range SetBlockAndRecordEdit is a no-op, same as SetBlock, and records nothing");
}

void TestSignStore() {
    // CRAFT_PARITY.md §4.3: SignStore ports Craft's real SignList model
    // (src/sign.c) -- a separate overlay keyed on (x,y,z,face), not a block
    // type. Unique per (x,y,z,face) (matches Craft's own `sign_xyzface_idx`
    // index / `insert or replace` semantics); empty text removes a sign
    // instead of storing a blank one (matches Craft's own `set_sign` ->
    // `sign_list_remove` path for empty text).
    SignStore store;
    Check(store.Signs().empty(), "a fresh SignStore has no signs");

    store.PlaceSign(1, 2, 3, 4, "Hello");
    Check(store.Signs().size() == 1, "PlaceSign adds a new sign");
    Check(store.Signs()[0].x == 1 && store.Signs()[0].y == 2 && store.Signs()[0].z == 3 &&
              store.Signs()[0].face == 4 && store.Signs()[0].text == "Hello",
          "the placed sign has the correct coordinates, face, and text");

    store.PlaceSign(1, 2, 3, 4, "Updated");
    Check(store.Signs().size() == 1, "placing a sign at the same (x,y,z,face) replaces it, not duplicates it");
    Check(store.Signs()[0].text == "Updated", "the replaced sign has the new text");

    store.PlaceSign(1, 2, 3, 5, "Different face");
    Check(store.Signs().size() == 2, "a different face at the same (x,y,z) is a distinct sign");

    store.PlaceSign(1, 2, 3, 4, "");
    Check(store.Signs().size() == 1, "placing empty text removes the existing sign instead of storing a blank one");

    std::vector<Sign> loaded = {Sign{9, 9, 9, 0, "Loaded"}};
    store.ReplaceAll(loaded);
    Check(store.Signs().size() == 1 && store.Signs()[0].text == "Loaded",
          "ReplaceAll replaces the entire list at once, as used when loading from persistence");

    // RemoveAllAt (CRAFT_PARITY.md §4.3) ports Craft's own unset_sign
    // (src/main.c) -- called when the underlying block is broken, since a
    // sign can't outlive the block face it was attached to. Removes every
    // face at (x,y,z), leaving signs at other coordinates untouched.
    SignStore removeStore;
    removeStore.PlaceSign(5, 5, 5, 0, "Face 0");
    removeStore.PlaceSign(5, 5, 5, 1, "Face 1");
    removeStore.PlaceSign(6, 5, 5, 0, "Different cell");
    Check(removeStore.Signs().size() == 3, "three signs placed across two cells");

    Check(removeStore.RemoveAllAt(5, 5, 5), "RemoveAllAt returns true when it actually removed something");
    Check(removeStore.Signs().size() == 1, "RemoveAllAt removes every face at the targeted (x,y,z)");
    Check(removeStore.Signs()[0].x == 6, "the sign at a different (x,y,z) survives RemoveAllAt");

    Check(!removeStore.RemoveAllAt(5, 5, 5), "RemoveAllAt returns false when there was nothing to remove");

    // RemoveAllInColumn (plan.md §12.1 item 19) -- used when a chunk-column
    // unloads: removes every sign in that column regardless of y, unlike
    // RemoveAllAt which matches an exact (x,y,z).
    SignStore columnStore;
    columnStore.PlaceSign(5, 5, 5, 0, "Column zero, low");
    columnStore.PlaceSign(5, 40, 5, 0, "Column zero, high"); // same column, very different y
    columnStore.PlaceSign(20, 5, 20, 0, "Column one-one");
    Check(columnStore.Signs().size() == 3, "three signs placed, two sharing a column at different heights");

    Check(columnStore.RemoveAllInColumn(0, 0), "RemoveAllInColumn returns true when it actually removed something");
    Check(columnStore.Signs().size() == 1, "RemoveAllInColumn removes every sign in that column regardless of y");
    Check(columnStore.Signs()[0].x == 20, "the sign in a different column survives RemoveAllInColumn");

    Check(!columnStore.RemoveAllInColumn(0, 0), "RemoveAllInColumn returns false when there was nothing to remove");
}

void TestWorldGenerationIsDeterministic() {
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 7) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 7) {
            for (int y = 0; y < WORLD_SIZE_Y; y += 5) {
                if (a.GetBlock(x, y, z) != b.GetBlock(x, y, z)) allMatch = false;
            }
        }
    }
    Check(allMatch, "World::Generate is deterministic for a fixed seed");

    Check(a.GetBlock(0, 0, 0) == BlockType::Bedrock, "y=0 is always Bedrock");

    const int height = NoiseGenerator::Height(1234, 0, 0);
    Check(a.GetBlock(0, height, 0) == BlockType::Grass, "surface block at generated height is Grass");
    Check(a.GetBlock(0, height + 5, 0) == BlockType::Air, "blocks well above the surface are Air");
    Check(!a.IsSolid(0, WORLD_SIZE_Y - 1, 0), "world ceiling column is not solid");
}

void TestWorldGeneratesClouds() {
    // Craft's world.c places CLOUD blocks via a 3D Simplex density check
    // near its own world's ceiling (verified against the real checkout —
    // plan.md §11.1 corrects the earlier "caves/overhangs" citation, which
    // turned out to have no real Craft reference at all). This project
    // reuses the same frequency/threshold, in a band near this project's own
    // (much lower) ceiling instead.
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool anyCloudFound = false;
    bool allCloudsInBand = true;
    for (int x = 0; x < WORLD_SIZE_X && allCloudsInBand; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z && allCloudsInBand; ++z) {
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                if (a.GetBlock(x, y, z) == BlockType::Cloud) {
                    anyCloudFound = true;
                    if (y < 58 || y > 62) allCloudsInBand = false;
                }
            }
        }
    }
    Check(anyCloudFound, "World::Generate places at least one Cloud block somewhere in the world");
    Check(allCloudsInBand, "every generated Cloud block sits within the documented y=[58,62] ceiling band");

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 3) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 3) {
            for (int y = 58; y <= 62; ++y) {
                if (a.GetBlock(x, y, z) != b.GetBlock(x, y, z)) allMatch = false;
            }
        }
    }
    Check(allMatch, "cloud placement is deterministic for a fixed seed, same as terrain");

    // Clouds never overwrite terrain: nothing above kMaxHeight=56 should
    // already be non-Air before the cloud pass runs, but assert the
    // resulting invariant directly — every surface-height cell must still be
    // ordinary terrain surface, i.e. the cloud pass (band y=[58,62], far
    // above kMaxHeight=56) didn't corrupt terrain/tree generation. Grass,
    // Sand, or Wood is accepted at the surface cell: World::GenerateTrees
    // legitimately overwrites some Grass cells with a tree trunk's base
    // block (same as Craft's own `func(x, h, z, 5, arg)`), and low-elevation
    // "beach" columns legitimately surface as Sand instead of Grass
    // (CRAFT_PARITY.md §3.3) — neither is terrain corruption.
    bool terrainUnaffected = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 5) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 5) {
            const int height = NoiseGenerator::Height(1234, x, z);
            const BlockType surface = a.GetBlock(x, height, z);
            if (surface != BlockType::Grass && surface != BlockType::Wood && surface != BlockType::Sand) {
                terrainUnaffected = false;
            }
        }
    }
    Check(terrainUnaffected, "adding the cloud pass does not disturb terrain/tree/sand surface generation");
}

void TestWorldGeneratesSandAtLowElevation() {
    // Regression test (CRAFT_PARITY.md §3.3): BlockType::Sand was fully
    // defined (BlockDef, Hotbar roster) but World::Generate never actually
    // placed it anywhere -- confirmed dead code by the audit. Fixed by a
    // low-elevation "beach" surface rule (kSandMaxHeight=12 in World.cpp,
    // Craft's own literal t=12 sea-level threshold, world.c).
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool anySandFound = false;
    bool everySandColumnIsLowElevation = true;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(1234, x, z);
            if (a.GetBlock(x, height, z) == BlockType::Sand) {
                anySandFound = true;
                if (height > 12) everySandColumnIsLowElevation = false;
            }
        }
    }
    Check(anySandFound, "World::Generate places at least one Sand-surfaced column somewhere in the world");
    Check(everySandColumnIsLowElevation,
          "every Sand-surfaced column sits at or below the documented kSandMaxHeight=12 threshold");

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 2) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 2) {
            const int height = NoiseGenerator::Height(1234, x, z);
            if (a.GetBlock(x, height, z) != b.GetBlock(x, height, z)) allMatch = false;
        }
    }
    Check(allMatch, "sand-vs-grass surface selection is deterministic for a fixed seed");
}

void TestWorldGeneratesTrees() {
    // World::GenerateTrees ports Craft's own tree pass (verified against the
    // real checkout — `simplex2(x, z, 6, 0.5, 2) > 0.84` trigger on grass
    // columns, a Wood trunk plus a Leaves canopy blob).
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool anyWoodFound = false;
    bool anyLeavesFound = false;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                const BlockType block = a.GetBlock(x, y, z);
                if (block == BlockType::Wood) anyWoodFound = true;
                if (block == BlockType::Leaves) anyLeavesFound = true;
            }
        }
    }
    Check(anyWoodFound, "World::Generate places at least one tree trunk (Wood) somewhere in the world");
    Check(anyLeavesFound, "World::Generate places at least one Leaves canopy block somewhere in the world");

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 2) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 2) {
            for (int y = 0; y < WORLD_SIZE_Y; y += 3) {
                if (a.GetBlock(x, y, z) != b.GetBlock(x, y, z)) allMatch = false;
            }
        }
    }
    Check(allMatch, "tree placement is deterministic for a fixed seed, same as terrain/clouds");

    // Every tree trunk's base must sit exactly at its column's real terrain
    // surface height (Craft only grows trees on its own `w == 1` grass
    // columns, trunk starting at y=h) — scan for any Wood block whose y
    // doesn't match NoiseGenerator::Height(1234, x, z) for its (x, z).
    bool noFloatingTrunkBase = true;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(1234, x, z);
            // A trunk's base (its lowest Wood block) is always at y=height;
            // higher trunk segments (y=height+1..height+6) are also Wood but
            // are expected, not "floating" — only check the base cell itself
            // isn't Wood while some other Wood exists above the *actual*
            // surface for this column (which would mean a tree rooted in
            // midair). Bedrock/Stone/Dirt/Grass/Wood at y=height are all
            // valid; a mismatch would mean tree generation used the wrong
            // per-column height.
            if (a.GetBlock(x, height, z) == BlockType::Wood) {
                if (a.GetBlock(x, height - 1, z) == BlockType::Air) noFloatingTrunkBase = false;
            }
        }
    }
    Check(noFloatingTrunkBase, "every tree trunk's base sits on solid ground at its column's real surface height, not floating");
}

void TestWorldGeneratesTallGrass() {
    // World::GenerateGrassDecoration ports Craft's real tall-grass pass
    // (verified against the checkout — `simplex2(-x*0.1, z*0.1, 4, 0.8, 2)
    // > 0.6` trigger on grass columns, placed one cell above the surface).
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool anyTallGrassFound = false;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                if (a.GetBlock(x, y, z) == BlockType::TallGrass) anyTallGrassFound = true;
            }
        }
    }
    Check(anyTallGrassFound, "World::Generate places at least one TallGrass block somewhere in the world");

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 2) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 2) {
            const int height = NoiseGenerator::Height(1234, x, z);
            if (height + 1 < WORLD_SIZE_Y && a.GetBlock(x, height + 1, z) != b.GetBlock(x, height + 1, z)) {
                allMatch = false;
            }
        }
    }
    Check(allMatch, "tall grass placement is deterministic for a fixed seed, same as trees/clouds");

    // Every TallGrass block must sit exactly one cell above its column's
    // real terrain surface height (never floating, never buried).
    bool everyBladeSitsOnSurface = true;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(1234, x, z);
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                if (a.GetBlock(x, y, z) == BlockType::TallGrass && y != height + 1) {
                    everyBladeSitsOnSurface = false;
                }
            }
        }
    }
    Check(everyBladeSitsOnSurface, "every TallGrass block sits exactly one cell above its column's real surface height");
}

void TestWorldGeneratesFlowers() {
    // World::GenerateFlowers ports Craft's real flower-decoration pass
    // (verified against the checkout — `simplex2(x*0.05, -z*0.05, 4, 0.8, 2)
    // > 0.7` trigger on grass columns, placed one cell above the surface;
    // Craft's second noise sample picking one of 7 flower colors is skipped
    // since this project has a single representative Flower type).
    World a, b;
    a.Generate(1234);
    b.Generate(1234);

    bool anyFlowerFound = false;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                if (a.GetBlock(x, y, z) == BlockType::Flower) anyFlowerFound = true;
            }
        }
    }
    Check(anyFlowerFound, "World::Generate places at least one Flower block somewhere in the world");

    bool allMatch = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 2) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 2) {
            const int height = NoiseGenerator::Height(1234, x, z);
            if (height + 1 < WORLD_SIZE_Y && a.GetBlock(x, height + 1, z) != b.GetBlock(x, height + 1, z)) {
                allMatch = false;
            }
        }
    }
    Check(allMatch, "flower placement is deterministic for a fixed seed, same as tall grass/trees/clouds");

    bool everyFlowerSitsOnSurface = true;
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(1234, x, z);
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                if (a.GetBlock(x, y, z) == BlockType::Flower && y != height + 1) {
                    everyFlowerSitsOnSurface = false;
                }
            }
        }
    }
    Check(everyFlowerSitsOnSurface, "every Flower block sits exactly one cell above its column's real surface height");
}

void TestNoiseGeneratorSimplex2IsSeeded() {
    Check(NoiseGenerator::Height(1234, 5, 5) == NoiseGenerator::Height(1234, 5, 5),
          "NoiseGenerator::Height is a pure function of (seed, x, z)");

    bool anyDifferent = false;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            if (NoiseGenerator::Height(1111, x, z) != NoiseGenerator::Height(2222, x, z)) {
                anyDifferent = true;
            }
        }
    }
    Check(anyDifferent, "different seeds produce different terrain (unlike Craft's fixed permutation table)");

    // Craft's real formula (re-ported 2026-07-10, user decision): h = f*mh
    // where f,g are each nominally in [0,1] (Simplex2's own normalization),
    // mh = g*32+16 in [16,48], so h is nominally in [0,48] before the
    // h<=12->12 sea-level clamp -- i.e. [12,48] after. A generous tolerance
    // (not the exact mathematical bound) avoids flakiness from rare
    // simplex-noise float overshoot slightly outside [0,1]; the lower bound
    // is exact and unconditional (the sea-level clamp is unconditional
    // code, not a probabilistic property of the noise).
    bool allInRange = true;
    for (int x = 0; x < 64; ++x) {
        for (int z = 0; z < 64; ++z) {
            const int h = NoiseGenerator::Height(42, x, z);
            if (h < 12 || h > 60) allInRange = false;
        }
    }
    Check(allInRange, "Height stays within a sane range around its documented [12, 48] formula bounds");
}

void TestNoiseGeneratorSimplex3() {
    Check(NoiseGenerator::Simplex3(1234, 1.0f, 2.0f, 3.0f, 3, 0.5f, 2.0f) ==
              NoiseGenerator::Simplex3(1234, 1.0f, 2.0f, 3.0f, 3, 0.5f, 2.0f),
          "NoiseGenerator::Simplex3 is a pure function of (seed, x, y, z, octaves, persistence, lacunarity)");

    bool anyDifferentBySeed = false;
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y) {
            const float a = NoiseGenerator::Simplex3(1111, static_cast<float>(x) * 0.1f,
                                                       static_cast<float>(y) * 0.1f, 0.5f, 3, 0.5f, 2.0f);
            const float b = NoiseGenerator::Simplex3(2222, static_cast<float>(x) * 0.1f,
                                                       static_cast<float>(y) * 0.1f, 0.5f, 3, 0.5f, 2.0f);
            if (std::abs(a - b) > 0.001f) anyDifferentBySeed = true;
        }
    }
    Check(anyDifferentBySeed, "different seeds produce different 3D density noise, same as Simplex2/Height");

    bool anyDifferentByZ = false;
    const float atZ0 = NoiseGenerator::Simplex3(42, 5.0f, 5.0f, 0.0f, 3, 0.5f, 2.0f);
    for (int z = 1; z < 16; ++z) {
        const float atZ = NoiseGenerator::Simplex3(42, 5.0f, 5.0f, static_cast<float>(z) * 0.3f, 3, 0.5f, 2.0f);
        if (std::abs(atZ - atZ0) > 0.001f) anyDifferentByZ = true;
    }
    Check(anyDifferentByZ, "Simplex3 actually varies along the third (z/y-in-world) axis, unlike Simplex2");

    bool allInRange = true;
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 32; ++y) {
            for (int z = 0; z < 8; ++z) {
                const float n = NoiseGenerator::Simplex3(7, static_cast<float>(x) * 0.05f,
                                                           static_cast<float>(y) * 0.05f,
                                                           static_cast<float>(z) * 0.05f, 3, 0.5f, 2.0f);
                if (n < -0.5f || n > 1.5f) allInRange = false;
            }
        }
    }
    Check(allInRange, "Simplex3 output stays in a sane range around its documented [0, 1] normalization");
}

void TestDayNightCycleMatchesCraftsCurveShape() {
    constexpr float kDay = kDefaultDayLengthSeconds;

    Check(ComputeDaylight(0.0f, kDay) < 0.01f, "midnight (t=0) is full night");
    Check(std::abs(ComputeDaylight(kDay * 0.25f, kDay) - 0.5f) < 0.01f,
          "dawn transition midpoint (t=0.25) is half daylight");
    Check(ComputeDaylight(kDay * 0.5f, kDay) > 0.99f, "midday (t=0.5) is full daylight");
    Check(std::abs(ComputeDaylight(kDay * 0.85f, kDay) - 0.5f) < 0.01f,
          "dusk transition midpoint (t=0.85) is half daylight");
    Check(ComputeDaylight(kDay * 0.999f, kDay) < 0.01f, "just before midnight (t~1) is full night again");

    // Wraps for any elapsed time, not just the first cycle (callers feed
    // GameTime::TotalGameTime directly, which only grows).
    const float firstCycleDawn = ComputeDaylight(kDay * 0.25f, kDay);
    const float fifthCycleDawn = ComputeDaylight(kDay * 4.25f, kDay);
    Check(std::abs(firstCycleDawn - fifthCycleDawn) < 0.001f,
          "the curve wraps identically on later cycles (pure function of elapsed time mod day length)");

    Check(std::abs(ComputeDaylight(100.0f, 0.0f) - 0.5f) < 0.001f,
          "a zero/invalid day length falls back to a fixed mid-level daylight instead of dividing by zero");
}

void TestChunkMesherFaceCulling() {
    World world;
    world.AllocateColumn(0, 0);
    // A fully-air chunk should produce no geometry at all.
    ChunkMeshData empty = ChunkMesher::Build(world, 0, 0, 0);
    Check(empty.opaque.vertices.empty() && empty.opaque.indices.empty(), "all-air chunk meshes to nothing");

    // A single solid block surrounded by air exposes all 6 faces.
    world.SetBlock(5, 5, 5, BlockType::Stone);
    ChunkMeshData single = ChunkMesher::Build(world, 0, 0, 0);
    Check(single.opaque.vertices.size() == 24, "single exposed block emits 6 faces * 4 verts = 24 vertices");
    Check(single.opaque.indices.size() == 36, "single exposed block emits 6 faces * 6 indices = 36 indices");
    Check(single.transparent.vertices.empty(), "an opaque block emits nothing into the transparent mesh");

    // Burying that block on all 6 sides must remove all its faces from the mesh.
    world.SetBlock(6, 5, 5, BlockType::Stone);
    world.SetBlock(4, 5, 5, BlockType::Stone);
    world.SetBlock(5, 6, 5, BlockType::Stone);
    world.SetBlock(5, 4, 5, BlockType::Stone);
    world.SetBlock(5, 5, 6, BlockType::Stone);
    world.SetBlock(5, 5, 4, BlockType::Stone);
    ChunkMeshData buried = ChunkMesher::Build(world, 0, 0, 0);
    // 7 solid blocks total, the center one fully occluded: 6 blocks * 5 exposed faces
    // each (the face pointing at the center block is occluded) = 30 faces.
    Check(buried.opaque.vertices.size() == 30 * 4, "surrounding a block on all sides hides its faces");
}

void TestChunkMesherGlassTransparency() {
    // A lone Glass block surrounded by air: goes into the transparent mesh,
    // all 6 faces exposed, same as an opaque block would be.
    World lone;
    lone.AllocateColumn(0, 0);
    lone.SetBlock(5, 5, 5, BlockType::Glass);
    ChunkMeshData loneMesh = ChunkMesher::Build(lone, 0, 0, 0);
    Check(loneMesh.opaque.vertices.empty(), "a lone Glass block emits nothing into the opaque mesh");
    Check(loneMesh.transparent.vertices.size() == 24, "a lone Glass block emits all 6 faces (24 verts)");

    // Two adjacent Glass blocks: unlike opaque neighbors, transparent
    // neighbors don't occlude each other's faces (matches Craft's
    // `opaque[cell] = !is_transparent(w)` — see World::IsOpaque).
    World adjacent;
    adjacent.AllocateColumn(0, 0);
    adjacent.SetBlock(5, 5, 5, BlockType::Glass);
    adjacent.SetBlock(6, 5, 5, BlockType::Glass);
    ChunkMeshData adjacentMesh = ChunkMesher::Build(adjacent, 0, 0, 0);
    Check(adjacentMesh.transparent.vertices.size() == 24 * 2,
          "two adjacent Glass blocks both keep all 6 faces (transparent neighbors don't occlude)");

    // A Glass block against a Stone block: Glass's face touching the Stone
    // is culled (the opaque Stone face already covers that spot), but
    // Stone's face touching the Glass is NOT culled (Glass doesn't occlude).
    World againstStone;
    againstStone.AllocateColumn(0, 0);
    againstStone.SetBlock(5, 5, 5, BlockType::Glass);
    againstStone.SetBlock(6, 5, 5, BlockType::Stone);
    ChunkMeshData mixedMesh = ChunkMesher::Build(againstStone, 0, 0, 0);
    Check(mixedMesh.transparent.vertices.size() == 20,
          "Glass next to Stone loses only its one face touching the Stone (5 of 6 faces, 20 verts)");
    Check(mixedMesh.opaque.vertices.size() == 24,
          "Stone next to Glass keeps all 6 faces (Glass doesn't occlude Stone's face)");

    // Collision must be unaffected: Glass is transparent but still solid/collidable.
    Check(againstStone.IsSolid(5, 5, 5), "Glass still occupies space (meshed/hit-testable)");
    Check(againstStone.IsCollidable(5, 5, 5), "Glass is still collidable despite being transparent");
    Check(!againstStone.IsOpaque(5, 5, 5), "Glass is not opaque (doesn't occlude neighbor faces)");
    Check(againstStone.IsOpaque(6, 5, 5), "Stone is opaque");
}

void TestChunkMesherLeavesTransparency() {
    // Leaves is transparent like Glass (plan.md §11.1 "Trees and plants" —
    // World::GenerateTrees), so it follows the exact same occlusion rules as
    // TestChunkMesherGlassTransparency above, just with a different
    // BlockType/tile index.
    World lone;
    lone.AllocateColumn(0, 0);
    lone.SetBlock(5, 5, 5, BlockType::Leaves);
    ChunkMeshData loneMesh = ChunkMesher::Build(lone, 0, 0, 0);
    Check(loneMesh.opaque.vertices.empty(), "a lone Leaves block emits nothing into the opaque mesh");
    Check(loneMesh.transparent.vertices.size() == 24, "a lone Leaves block emits all 6 faces (24 verts)");

    World adjacent;
    adjacent.AllocateColumn(0, 0);
    adjacent.SetBlock(5, 5, 5, BlockType::Leaves);
    adjacent.SetBlock(6, 5, 5, BlockType::Leaves);
    ChunkMeshData adjacentMesh = ChunkMesher::Build(adjacent, 0, 0, 0);
    Check(adjacentMesh.transparent.vertices.size() == 24 * 2,
          "two adjacent Leaves blocks both keep all 6 faces (transparent neighbors don't occlude)");

    World againstWood;
    againstWood.AllocateColumn(0, 0);
    againstWood.SetBlock(5, 5, 5, BlockType::Leaves);
    againstWood.SetBlock(6, 5, 5, BlockType::Wood);
    ChunkMeshData mixedMesh = ChunkMesher::Build(againstWood, 0, 0, 0);
    Check(mixedMesh.transparent.vertices.size() == 20,
          "Leaves next to Wood loses only its one face touching the Wood (5 of 6 faces, 20 verts)");
    Check(mixedMesh.opaque.vertices.size() == 24,
          "Wood next to Leaves keeps all 6 faces (Leaves doesn't occlude Wood's face)");

    Check(againstWood.IsSolid(5, 5, 5), "Leaves still occupies space (meshed/hit-testable)");
    Check(againstWood.IsCollidable(5, 5, 5), "Leaves is still collidable (unlike Cloud)");
    Check(!againstWood.IsOpaque(5, 5, 5), "Leaves is not opaque (doesn't occlude neighbor faces)");
}

void TestChunkMesherPlantBillboard() {
    // TallGrass (CRAFT_PARITY.md §3.7 "Non-cubic plant geometry") emits a
    // 4-quad cross billboard (ChunkMesher::EmitPlant), not 6 cube faces —
    // ports Craft's make_plant. Unlike cube blocks, plants are never
    // face-culled against neighbors: they always emit all 4 quads.
    World lone;
    lone.AllocateColumn(0, 0);
    lone.SetBlock(5, 5, 5, BlockType::TallGrass);
    ChunkMeshData loneMesh = ChunkMesher::Build(lone, 0, 0, 0);
    Check(loneMesh.opaque.vertices.empty(), "a lone TallGrass block emits nothing into the opaque mesh");
    Check(loneMesh.transparent.vertices.size() == 16,
          "a lone TallGrass block emits 4 quads * 4 verts = 16 vertices (cross billboard, not a cube)");
    Check(loneMesh.transparent.indices.size() == 24, "4 quads * 6 indices = 24 indices");

    // Surrounding a plant on all 6 sides must NOT reduce its face count —
    // plants are never occlusion-culled, unlike cube blocks.
    World surrounded;
    surrounded.AllocateColumn(0, 0);
    surrounded.SetBlock(5, 5, 5, BlockType::TallGrass);
    surrounded.SetBlock(6, 5, 5, BlockType::Stone);
    surrounded.SetBlock(4, 5, 5, BlockType::Stone);
    surrounded.SetBlock(5, 6, 5, BlockType::Stone);
    surrounded.SetBlock(5, 4, 5, BlockType::Stone);
    surrounded.SetBlock(5, 5, 6, BlockType::Stone);
    surrounded.SetBlock(5, 5, 4, BlockType::Stone);
    ChunkMeshData surroundedMesh = ChunkMesher::Build(surrounded, 0, 0, 0);
    Check(surroundedMesh.transparent.vertices.size() == 16,
          "a fully-surrounded TallGrass block still emits all 4 quads (plants are never face-culled)");

    // Collision/occlusion/breakability rules (CRAFT_PARITY.md §3.7): plants
    // are solid (meshed/hit-testable/breakable) but NOT collidable (the
    // player walks through them) and NOT opaque (don't occlude neighbors) —
    // same non-collidable pattern as Cloud, opposite reasoning.
    Check(surrounded.IsSolid(5, 5, 5), "TallGrass occupies space (meshed/hit-testable)");
    Check(surrounded.IsBreakable(5, 5, 5), "TallGrass is breakable");
    Check(!surrounded.IsCollidable(5, 5, 5), "TallGrass is not collidable (the player walks through it)");
    Check(!surrounded.IsOpaque(5, 5, 5), "TallGrass does not occlude neighboring faces");
    Check(surrounded.IsOpaque(6, 5, 5), "a normal Stone neighbor is still opaque (unaffected by the plant)");
}

void TestChunkMesherFlowerUsesSamePlantPath() {
    // Flower (CRAFT_PARITY.md §3.7 follow-up) reuses the exact same
    // EmitPlant path as TallGrass -- just a lighter check that it's wired
    // up correctly, not a full re-test of the cross-billboard shape
    // (already covered by TestChunkMesherPlantBillboard above).
    World world;
    world.AllocateColumn(0, 0);
    world.SetBlock(5, 5, 5, BlockType::Flower);
    ChunkMeshData mesh = ChunkMesher::Build(world, 0, 0, 0);
    Check(mesh.opaque.vertices.empty(), "a lone Flower block emits nothing into the opaque mesh");
    Check(mesh.transparent.vertices.size() == 16, "a lone Flower block emits a 4-quad cross billboard, not a cube");
    Check(world.IsSolid(5, 5, 5), "Flower occupies space (meshed/hit-testable)");
    Check(world.IsBreakable(5, 5, 5), "Flower is breakable");
    Check(!world.IsCollidable(5, 5, 5), "Flower is not collidable (the player walks through it)");
    Check(!world.IsOpaque(5, 5, 5), "Flower does not occlude neighboring faces");
}

void TestChunkMesherCloudIsOpaqueButNotCollidable() {
    // Cloud is the inverse situation from Glass: opaque (occludes neighbors,
    // meshed into the *opaque* mesh, hit-testable) but NOT collidable (the
    // player walks straight through it) — matches Craft's
    // `is_obstacle(CLOUD) == 0` while it's absent from `is_transparent`'s
    // switch (src/item.c).
    World world;
    world.AllocateColumn(0, 0);
    world.SetBlock(5, 5, 5, BlockType::Cloud);
    world.SetBlock(6, 5, 5, BlockType::Stone);
    ChunkMeshData mesh = ChunkMesher::Build(world, 0, 0, 0);

    Check(mesh.transparent.vertices.empty(), "Cloud emits nothing into the transparent mesh");
    Check(mesh.opaque.vertices.size() == 20 + 20,
          "Cloud meshes/occludes exactly like a normal opaque block (5 faces each, touching faces mutually culled)");

    Check(world.IsSolid(5, 5, 5), "Cloud occupies space (meshed/hit-testable)");
    Check(world.IsOpaque(5, 5, 5), "Cloud is opaque (occludes neighbor faces, unlike Glass)");
    Check(!world.IsCollidable(5, 5, 5), "Cloud is not collidable (the player walks through it)");
    Check(world.IsCollidable(6, 5, 5), "a normal Stone neighbor is still collidable");
}

void TestVoxelRaycastHitsExpectedFaceAndBlock() {
    World world;
    world.AllocateColumn(0, 0);
    world.SetBlock(5, 5, 5, BlockType::Stone);

    // Looking straight down the -Z axis at the block from in front of it (+Z side).
    auto hit = VoxelRaycast::Cast(world, Vec3f{5.5f, 5.5f, 10.0f}, Vec3f{0.0f, 0.0f, -1.0f}, 20.0f);
    Check(hit.has_value(), "raycast toward a placed block hits it");
    if (hit) {
        Check(hit->x == 5 && hit->y == 5 && hit->z == 5, "raycast hit reports the correct block coordinate");
        Check(hit->nx == 0 && hit->ny == 0 && hit->nz == 1, "raycast hit reports the +Z face normal");
    }

    auto miss = VoxelRaycast::Cast(world, Vec3f{0.5f, 100.0f, 0.5f}, Vec3f{0.0f, 1.0f, 0.0f}, 20.0f);
    Check(!miss.has_value(), "raycast pointed away from any geometry misses");
}

void TestHotbarSelectionAndCycling() {
    Hotbar hotbar;
    Check(hotbar.SlotCount() == 16, "hotbar has 16 slots (Bedrock and Cloud excluded from the placeable roster)");
    Check(hotbar.SelectedIndex() == 0, "hotbar starts on slot 0");
    Check(hotbar.Selected() == BlockType::Grass, "hotbar starts selecting slot 1's block (Grass)");

    hotbar.SelectSlot(4);
    Check(hotbar.SelectedIndex() == 3, "SelectSlot(4) selects 0-based index 3");
    Check(hotbar.Selected() == BlockType::Stone, "slot 4 is Stone");

    hotbar.SelectSlot(0);
    Check(hotbar.SelectedIndex() == 3, "SelectSlot(0) is out of range and ignored");
    hotbar.SelectSlot(99);
    Check(hotbar.SelectedIndex() == 3, "SelectSlot(99) is out of range and ignored");

    hotbar.CycleNext();
    Check(hotbar.SelectedIndex() == 4, "CycleNext advances to the next slot");

    hotbar.SelectSlot(12);
    Check(hotbar.Selected() == BlockType::Snow, "slot 12 (beyond the 9 direct number keys) is Snow");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::Glass, "slot 13 is Glass");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::Leaves, "slot 14 is Leaves");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::TallGrass, "slot 15 is TallGrass");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::Flower, "slot 16 (the last slot) is Flower");
    hotbar.CycleNext();
    Check(hotbar.SelectedIndex() == 0, "CycleNext wraps back to slot 0 after the last slot");

    // Regression test (CRAFT_PARITY.md §2.1): Craft's E/R key pair cycles
    // forward/backward through the whole roster; CyclePrev was missing
    // until this session (only CycleNext/E existed).
    hotbar.CyclePrev();
    Check(hotbar.Selected() == BlockType::Flower,
          "CyclePrev from slot 0 wraps backward to the last slot (Flower)");
    hotbar.CyclePrev();
    Check(hotbar.Selected() == BlockType::TallGrass, "CyclePrev steps back one slot at a time");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::Flower, "CycleNext after CyclePrev returns to the same slot");

    // Regression test (CRAFT_PARITY.md §2.7): middle-click "eyedropper",
    // ports Craft's on_middle_click (linear-scan items[] for the targeted
    // block, select that slot if found).
    hotbar.SelectSlot(1);
    Check(hotbar.SelectByBlockType(BlockType::Cobblestone), "SelectByBlockType finds a slot holding that type");
    Check(hotbar.Selected() == BlockType::Cobblestone, "SelectByBlockType actually selects the matching slot");
    Check(!hotbar.SelectByBlockType(BlockType::Bedrock),
          "SelectByBlockType returns false for a type not in the roster (Bedrock)");
    Check(hotbar.Selected() == BlockType::Cobblestone,
          "a failed SelectByBlockType leaves the current selection unchanged, matching Craft's on_middle_click");
    Check(!hotbar.SelectByBlockType(BlockType::Air), "SelectByBlockType returns false for Air");
}

void TestPlayerSpawnAtBlockCenterAvoidsBoundaryWedging() {
    // Regression test for a real user-reported bug: CnaCraftGame used to
    // spawn the player at an *integer* world coordinate, exactly on the
    // boundary between two block columns. The player's 0.6-wide hitbox
    // (kPlayerHalfWidth=0.3) straddles that boundary equally on both sides.
    // With Simplex noise's steeper local height changes (§11.1), a
    // neighboring column right next to spawn can be much taller than the
    // spawn column, permanently wedging the player against it: unable to
    // reach its own column's true floor, unable to move in any direction,
    // and unable to jump clear of it either. Fixed by spawning at block
    // *center* (integer + 0.5) instead, which keeps the hitbox fully inside
    // its own column. Reproduced here with a hand-built flat floor plus one
    // much taller neighboring column, rather than the real generated
    // terrain, so this test stays deterministic even if the noise
    // implementation changes again.
    World world;
    AllocateAllLegacyColumns(world);
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y <= 3; ++y) world.SetBlock(x, y, z, BlockType::Stone);
        }
    }
    for (int y = 4; y <= 7; ++y) world.SetBlock(65, y, 64, BlockType::Stone); // tall neighbor at x=65

    PlayerController player(Vec3f{64.5f, 10.0f, 64.5f}); // block-center, not 64.0f/64.0f
    PlayerInput noInput;
    for (int i = 0; i < 300; ++i) player.Update(world, noInput, 1.0f / 60.0f);
    Check(std::abs(player.EyePosition().y - (4.0f + 1.7f)) < 0.05f,
          "block-center spawn settles on its own column's floor, unaffected by a much taller neighbor");
    Check(player.IsGrounded(), "block-center spawn next to a tall neighbor still registers as grounded");

    PlayerInput moveAway;
    moveAway.moveForward = 1.0f; // yaw=0 => -Z, perpendicular to the tall neighbor at x=65
    const float startZ = player.EyePosition().z;
    for (int i = 0; i < 60; ++i) player.Update(world, moveAway, 1.0f / 60.0f);
    Check(player.EyePosition().z < startZ - 1.0f,
          "block-center spawn next to a tall neighbor can still move freely");

    PlayerInput jump;
    jump.jumpPressed = true;
    const float preJumpY = player.EyePosition().y;
    player.Update(world, jump, 1.0f / 60.0f);
    Check(player.EyePosition().y > preJumpY, "block-center spawn next to a tall neighbor can still jump");
}

void TestPlayerControllerGravityAndGroundCollision() {
    World world;
    AllocateAllLegacyColumns(world);
    // A flat floor of stone at y=0..3, air above.
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y <= 3; ++y) {
                world.SetBlock(x, y, z, BlockType::Stone);
            }
        }
    }

    PlayerController player(Vec3f{static_cast<float>(WORLD_SIZE_X) / 2.0f, 10.0f,
                                   static_cast<float>(WORLD_SIZE_Z) / 2.0f});

    PlayerInput input;
    bool landed = false;
    for (int i = 0; i < 300 && !landed; ++i) {
        player.Update(world, input, 1.0f / 60.0f);
        if (std::abs(player.EyePosition().y - (4.0f + 1.7f)) < 0.05f) landed = true;
    }
    Check(landed, "player falls under gravity and comes to rest on top of the floor (y=4)");
}

void TestPlayerControllerDiagonalMovementNotFaster() {
    // Regression test (CRAFT_PARITY.md §1.5): moving with two keys held at
    // once (e.g. forward+right) must not move faster than a single key,
    // matching Craft's own get_motion_vector (always a unit motion vector
    // regardless of how many movement keys are held).
    World world; // empty/air, no obstacles -- pure kinematics test

    PlayerController straight(Vec3f{50.0f, 30.0f, 50.0f});
    straight.ToggleFlying(); // fly mode: no gravity, so only horizontal input matters
    PlayerInput straightInput;
    straightInput.moveForward = 1.0f;
    for (int i = 0; i < 60; ++i) straight.Update(world, straightInput, 1.0f / 60.0f);

    PlayerController diagonal(Vec3f{50.0f, 30.0f, 50.0f});
    diagonal.ToggleFlying();
    PlayerInput diagonalInput;
    diagonalInput.moveForward = 1.0f;
    diagonalInput.moveRight = 1.0f;
    for (int i = 0; i < 60; ++i) diagonal.Update(world, diagonalInput, 1.0f / 60.0f);

    const Vec3f straightPos = straight.EyePosition();
    const Vec3f diagonalPos = diagonal.EyePosition();
    const float straightDist = std::sqrt((straightPos.x - 50.0f) * (straightPos.x - 50.0f) +
                                          (straightPos.z - 50.0f) * (straightPos.z - 50.0f));
    const float diagonalDist = std::sqrt((diagonalPos.x - 50.0f) * (diagonalPos.x - 50.0f) +
                                          (diagonalPos.z - 50.0f) * (diagonalPos.z - 50.0f));
    Check(std::abs(straightDist - diagonalDist) < 0.05f,
          "moving diagonally (two keys held) covers the same distance as moving straight (one key), "
          "not sqrt(2)x farther");
}

void TestPlayerControllerFallSpeedIsClampedToTerminalVelocity() {
    // Regression test (CRAFT_PARITY.md §1.8): a long fall must not
    // accelerate forever -- Craft clamps dy to -250 units/s (`main.c`,
    // `dy = MAX(dy, -250)`), ported as PlayerController's kTerminalVelocity.
    World world; // empty/air, no floor -- keep falling
    PlayerController player(Vec3f{50.0f, 5000.0f, 50.0f});
    PlayerInput noInput;
    // Fall for long enough that an unclamped v=g*t would clearly exceed 250
    // (25 * 15s = 375 with no clamp).
    for (int i = 0; i < 60 * 15; ++i) player.Update(world, noInput, 1.0f / 60.0f);
    const float y0 = player.EyePosition().y;
    player.Update(world, noInput, 1.0f / 60.0f);
    const float y1 = player.EyePosition().y;
    const float fallSpeed = (y0 - y1) / (1.0f / 60.0f);
    Check(fallSpeed <= 250.5f, "fall speed is clamped to Craft's terminal velocity (250 units/s), not unbounded");
}

void TestPlayerControllerSubsteppingPreventsTunnelingThroughThinWall() {
    // Regression test (CRAFT_PARITY.md §1.7): without substepping, a single
    // large-dt movement update (e.g. a severe dropped/hitched frame) could
    // place the player's entire displacement past a thin (1-block) wall in
    // one step, since collision only ever tested the destination position,
    // not the swept path between start and destination. Craft explicitly
    // substeps movement resolution to prevent exactly this
    // (`step = MAX(8, estimate)`, main.c) -- ported to PlayerController.
    World world;
    AllocateAllLegacyColumns(world);
    // Flat floor so the player doesn't fall while testing horizontal
    // movement.
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y <= 3; ++y) world.SetBlock(x, y, z, BlockType::Stone);
        }
    }
    // A single-block-thick wall at x=20, spanning well above the floor.
    constexpr int kWallX = 20;
    for (int z = 45; z <= 55; ++z) {
        for (int y = 4; y <= 10; ++y) world.SetBlock(kWallX, y, z, BlockType::Stone);
    }

    PlayerController player(Vec3f{15.0f, 4.0f, 50.0f}); // on the floor, west of the wall
    PlayerInput input;
    input.moveForward = 1.0f;

    // Rotate to face +X (forwardX = sin(yaw), so yaw = +90 degrees) with a
    // zero-dt update first -- yaw updates unconditionally regardless of dt.
    PlayerInput turnInput;
    turnInput.lookDeltaYaw = 1.57079632679f;
    player.Update(world, turnInput, 0.0f);

    // A single Update call with a huge dt (2 real seconds): naive unclamped
    // displacement would be moveSpeed(4.5) * dt(2.0) = 9.0 blocks -- more
    // than enough to jump clean over the 1-block-thick wall 5 blocks away
    // in one unsubstepped step.
    player.Update(world, input, 2.0f);

    const float finalX = player.EyePosition().x;
    Check(finalX < static_cast<float>(kWallX),
          "a huge single-frame dt does not tunnel the player through a thin wall");
}

void TestPlayerJumpClearsOneBlockHeight() {
    // Regression test for a real user-reported bug: kJumpSpeed=7 against
    // kGravity=25 gives a max jump height of v^2/(2g) = 49/50 = 0.98 blocks
    // -- mathematically just short of clearing a full 1-block step, the
    // single most basic Craft-like traversal move. Fixed by matching
    // Craft's own jump speed of 8 (src/main.c: `dy = 8`), giving 64/50=1.28
    // blocks. Measures the jump arc directly (no horizontal movement or
    // collision involved) so it precisely discriminates the two values,
    // rather than depending on movement-timing specifics that could mask
    // the difference (as the companion ledge-traversal test below did).
    World world;
    AllocateAllLegacyColumns(world);
    constexpr int kFloorTopY = 3; // solid y=0..3, floor surface at y=4
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y <= kFloorTopY; ++y) world.SetBlock(x, y, z, BlockType::Stone);
        }
    }

    PlayerController player(Vec3f{static_cast<float>(WORLD_SIZE_X) / 2.0f, 10.0f,
                                   static_cast<float>(WORLD_SIZE_Z) / 2.0f});
    PlayerInput noInput;
    for (int i = 0; i < 120; ++i) player.Update(world, noInput, 1.0f / 60.0f);
    const float floorFeetY = player.EyePosition().y - 1.7f;
    Check(std::abs(floorFeetY - 4.0f) < 0.05f, "player settles on the floor before jumping");

    PlayerInput jump;
    jump.jumpPressed = true;
    float peakFeetY = floorFeetY;
    for (int i = 0; i < 90; ++i) {
        player.Update(world, jump, 1.0f / 60.0f);
        jump.jumpPressed = false; // one tap, like a real player
        const float feetY = player.EyePosition().y - 1.7f;
        if (feetY > peakFeetY) peakFeetY = feetY;
    }
    Check(peakFeetY - floorFeetY > 1.0f,
          "a single jump clears a full 1-block height (feet rise more than 1.0 blocks)");
}

void TestPlayerCanJumpOntoOneBlockHigherLedge() {
    // Companion integration check: confirms a jump can actually carry the
    // player onto a real 1-block-higher ledge while moving, on top of the
    // precise apex-height measurement above.
    World world;
    AllocateAllLegacyColumns(world);
    constexpr int kFloorTopY = 3; // solid y=0..3, main floor surface at y=4
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = 0; y <= kFloorTopY; ++y) world.SetBlock(x, y, z, BlockType::Stone);
        }
    }
    const int spawnX = WORLD_SIZE_X / 2;
    const int spawnZ = WORLD_SIZE_Z / 2;
    const int ledgeStartZ = spawnZ - 3; // a few blocks ahead (yaw=0 moves toward -Z)
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z <= ledgeStartZ; ++z) {
            world.SetBlock(x, kFloorTopY + 1, z, BlockType::Stone); // one block higher, surface at y=5
        }
    }

    PlayerController player(Vec3f{static_cast<float>(spawnX) + 0.5f, 10.0f, static_cast<float>(spawnZ) + 0.5f});
    PlayerInput noInput;
    for (int i = 0; i < 120; ++i) player.Update(world, noInput, 1.0f / 60.0f);
    Check(std::abs(player.EyePosition().y - (4.0f + 1.7f)) < 0.05f,
          "player settles on the main floor before attempting the ledge");

    PlayerInput jumpMove;
    jumpMove.moveForward = 1.0f; // yaw=0 => -Z, straight toward the ledge
    bool landedOnLedge = false;
    for (int i = 0; i < 180 && !landedOnLedge; ++i) {
        jumpMove.jumpPressed = player.IsGrounded(); // tap Space each time grounded, like a real player
        player.Update(world, jumpMove, 1.0f / 60.0f);
        if (player.IsGrounded() && std::abs(player.EyePosition().y - (5.0f + 1.7f)) < 0.05f) {
            landedOnLedge = true;
        }
    }
    Check(landedOnLedge, "player can jump up onto a 1-block-higher ledge while moving forward");
}

void TestPlayerControllerIntersectsBlockGuardsPlacement() {
    // Regression test (CRAFT_PARITY.md §2.6): ports Craft's
    // player_intersects_block check used by on_right_click to reject a
    // placement that would overlap the player's own AABB.
    PlayerController player(Vec3f{50.0f, 30.0f, 50.0f}); // feet at (50, 30, 50)

    Check(player.IntersectsBlock(50, 30, 50), "the player's own feet cell intersects their AABB");
    Check(player.IntersectsBlock(50, 31, 50), "a cell at head height also intersects (kPlayerHeight=1.8)");
    Check(!player.IntersectsBlock(50, 33, 50), "a cell well above the player's head does not intersect");
    Check(!player.IntersectsBlock(55, 30, 50), "a cell far away horizontally does not intersect");
    Check(!player.IntersectsBlock(50, 25, 50), "a cell well below the player's feet does not intersect");
}

void TestPlayerControllerFlyingTogglesGravityAndFreeVerticalMovement() {
    World world; // no terrain at all — plenty of open air in every direction
    PlayerController player(Vec3f{static_cast<float>(WORLD_SIZE_X) / 2.0f, 32.0f,
                                   static_cast<float>(WORLD_SIZE_Z) / 2.0f});

    Check(!player.IsFlying(), "player starts in game mode, not flying");

    PlayerInput noInput;
    const float startY = player.EyePosition().y;
    player.Update(world, noInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < startY, "sanity check: gravity pulls the player down in game mode");

    player.ToggleFlying();
    Check(player.IsFlying(), "ToggleFlying() switches to fly mode");

    const float flyStartY = player.EyePosition().y;
    PlayerInput hover;
    for (int i = 0; i < 60; ++i) player.Update(world, hover, 1.0f / 60.0f);
    Check(std::abs(player.EyePosition().y - flyStartY) < 0.01f,
          "flying with no vertical input does not fall (no gravity while flying)");

    // Craft's flying has no dedicated descend key at all (CRAFT_PARITY.md
    // §1.6, user decision 2026-07-10: match Craft's real pitch-coupled
    // flight) -- Space (jumpPressed) always forces a full-speed ascend
    // regardless of look direction or movement input.
    PlayerInput riseInput;
    riseInput.jumpPressed = true;
    for (int i = 0; i < 60; ++i) player.Update(world, riseInput, 1.0f / 60.0f);
    Check(player.EyePosition().y > flyStartY,
          "Space forces a full ascend while flying, matching Craft's own vy=1 override");

    const float risenY = player.EyePosition().y;
    // Descending requires looking down and moving forward (or looking up
    // and moving backward) -- there is no dedicated key, matching Craft's
    // own get_motion_vector flying branch exactly.
    PlayerInput lookDown;
    lookDown.lookDeltaPitch = -1.0f; // radians, well within the +-pi/2 clamp
    player.Update(world, lookDown, 1.0f / 60.0f);
    PlayerInput descendInput;
    descendInput.moveForward = 1.0f;
    for (int i = 0; i < 60; ++i) player.Update(world, descendInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < risenY,
          "moving forward while looking down descends while flying (no dedicated descend key, matches Craft)");

    player.ToggleFlying();
    Check(!player.IsFlying(), "ToggleFlying() again switches back to game mode");
    const float backToGameY = player.EyePosition().y;
    player.Update(world, noInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < backToGameY, "gravity resumes after leaving fly mode");

    // Pure strafing (no forward/back input) has no vertical component even
    // while pitched -- matches Craft's own `if (sx) { if (!sz) y = 0; ...}`
    // special case (a fresh PlayerController to isolate this from the
    // accumulated pitch/position state above).
    PlayerController strafer(Vec3f{static_cast<float>(WORLD_SIZE_X) / 2.0f, 32.0f,
                                    static_cast<float>(WORLD_SIZE_Z) / 2.0f});
    strafer.ToggleFlying();
    PlayerInput straferLookDown;
    straferLookDown.lookDeltaPitch = -1.0f;
    strafer.Update(world, straferLookDown, 1.0f / 60.0f);
    const float strafeStartY = strafer.EyePosition().y;
    PlayerInput strafeInput;
    strafeInput.moveRight = 1.0f;
    for (int i = 0; i < 60; ++i) strafer.Update(world, strafeInput, 1.0f / 60.0f);
    Check(std::abs(strafer.EyePosition().y - strafeStartY) < 0.01f,
          "pure strafing while flying has no vertical component even while pitched down, matching Craft");
}

void TestPlayerControllerFloorCatchSafetyNet() {
    // CRAFT_PARITY.md §1.8: ports Craft's own
    // `if (s->y < 0) s->y = highest_block(s->x, s->z) + 2` (main.c) -- a
    // last-resort recovery if the player ever ends up below the world.
    World world;
    world.Generate(1234);
    const int testX = WORLD_SIZE_X / 2;
    const int testZ = WORLD_SIZE_Z / 2;
    const int expectedHighest = world.HighestCollidableY(testX, testZ);
    Check(expectedHighest >= 0, "sanity check: generated terrain has a collidable column at the test coordinates");

    PlayerController player(
        Vec3f{static_cast<float>(testX) + 0.5f, -5.0f, static_cast<float>(testZ) + 0.5f});
    Check(player.EyePosition().y < 0.0f, "sanity check: the player starts below the world");

    PlayerInput noInput;
    player.Update(world, noInput, 1.0f / 60.0f);
    Check(std::abs(player.EyePosition().y - (static_cast<float>(expectedHighest + 2) + 1.7f)) < 0.1f,
          "falling below y=0 snaps the player to 2 blocks above the highest collidable block, matching Craft");
}

void TestPlayerControllerFloorCatchSkipsUnloadedColumn() {
    // plan.md §12.1 item 19: HighestCollidableY's -1 now means both "loaded
    // but genuinely no ground" and "column not loaded at all" -- the
    // floor-catch must not treat the latter as "snap to y=1", or a player
    // falling below an as-yet-unstreamed column would get teleported into
    // open air instead of just continuing to fall until real terrain loads.
    World world; // no columns allocated at all -- (500,*,500)'s column is unloaded
    PlayerController player(Vec3f{500.5f, -5.0f, 500.5f});
    Check(player.EyePosition().y < 0.0f, "sanity check: the player starts below the world");

    PlayerInput noInput;
    player.Update(world, noInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < 0.0f,
          "the floor-catch does not fire over an unloaded column -- the player keeps falling, not snapped to y=1");
}

}

int main() {
    TestChunkBasics();
    TestChunkCoordOfHandlesNegativeCoordinates();
    TestColumnKeyPackingRoundTrips();
    TestColumnCopyRoundTrips();
    TestWorldColumnLoadUnloadLifecycle();
    TestWorldBoundsAndRoundTrip();
    TestWorldRecordsEditsForPersistence();
    TestSignStore();
    TestWorldGenerationIsDeterministic();
    TestWorldGeneratesClouds();
    TestWorldGeneratesSandAtLowElevation();
    TestWorldGeneratesTrees();
    TestWorldGeneratesTallGrass();
    TestWorldGeneratesFlowers();
    TestNoiseGeneratorSimplex2IsSeeded();
    TestNoiseGeneratorSimplex3();
    TestDayNightCycleMatchesCraftsCurveShape();
    TestChunkMesherFaceCulling();
    TestChunkMesherGlassTransparency();
    TestChunkMesherLeavesTransparency();
    TestChunkMesherPlantBillboard();
    TestChunkMesherFlowerUsesSamePlantPath();
    TestChunkMesherCloudIsOpaqueButNotCollidable();
    TestChunkMesherBoundaryRemeshOnNeighborLoad();
    TestVoxelRaycastHitsExpectedFaceAndBlock();
    TestHotbarSelectionAndCycling();
    TestPlayerControllerGravityAndGroundCollision();
    TestPlayerControllerDiagonalMovementNotFaster();
    TestPlayerControllerFallSpeedIsClampedToTerminalVelocity();
    TestPlayerControllerSubsteppingPreventsTunnelingThroughThinWall();
    TestPlayerJumpClearsOneBlockHeight();
    TestPlayerCanJumpOntoOneBlockHigherLedge();
    TestPlayerSpawnAtBlockCenterAvoidsBoundaryWedging();
    TestPlayerControllerIntersectsBlockGuardsPlacement();
    TestPlayerControllerFlyingTogglesGravityAndFreeVerticalMovement();
    TestPlayerControllerFloorCatchSafetyNet();
    TestPlayerControllerFloorCatchSkipsUnloadedColumn();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
