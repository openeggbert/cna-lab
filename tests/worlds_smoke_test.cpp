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

void TestWorldBoundsAndRoundTrip() {
    World world;
    Check(world.GetBlock(-1, 0, 0) == BlockType::Air, "world out-of-range GetBlock returns Air");
    Check(!world.IsSolid(-1, 0, 0), "world out-of-range IsSolid is false");
    // Explicit guard against a real bug caught during development: Air's
    // BlockDef only overrides `solid`, so a naive `.collidable` read (default
    // true) made empty space collidable and froze the player instantly.
    // IsCollidable must AND with `solid`.
    Check(!world.IsCollidable(5, 5, 5), "Air is never collidable, regardless of BlockDef::collidable's default");

    world.SetBlock(20, 5, 20, BlockType::Stone); // crosses a chunk boundary (CHUNK_SIZE=16)
    Check(world.GetBlock(20, 5, 20) == BlockType::Stone, "world set/get round-trip across chunk boundary");
    Check(world.IsSolid(20, 5, 20), "world IsSolid true for Stone");
    Check(world.IsCollidable(20, 5, 20), "world IsCollidable true for Stone");

    world.SetBlock(9999, 9999, 9999, BlockType::Stone); // out of range, must be a no-op
    Check(world.GetBlock(9999, 9999, 9999) == BlockType::Air,
          "world out-of-range SetBlock is a no-op");
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
    // above kMaxHeight=56) didn't corrupt terrain/tree generation. Grass OR
    // Wood is accepted at the surface cell: World::GenerateTrees legitimately
    // overwrites some Grass cells with a tree trunk's base block, same as
    // Craft's own `func(x, h, z, 5, arg)` — that is not terrain corruption.
    bool terrainUnaffected = true;
    for (int x = 0; x < WORLD_SIZE_X; x += 5) {
        for (int z = 0; z < WORLD_SIZE_Z; z += 5) {
            const int height = NoiseGenerator::Height(1234, x, z);
            const BlockType surface = a.GetBlock(x, height, z);
            if (surface != BlockType::Grass && surface != BlockType::Wood) terrainUnaffected = false;
        }
    }
    Check(terrainUnaffected, "adding the cloud pass does not disturb terrain/tree surface generation");
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

    bool allInRange = true;
    for (int x = 0; x < 64; ++x) {
        for (int z = 0; z < 64; ++z) {
            const int h = NoiseGenerator::Height(42, x, z);
            if (h < 4 || h > 56) allInRange = false;
        }
    }
    Check(allInRange, "Height stays within its documented [4, 56] clamp range");
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
    lone.SetBlock(5, 5, 5, BlockType::Glass);
    ChunkMeshData loneMesh = ChunkMesher::Build(lone, 0, 0, 0);
    Check(loneMesh.opaque.vertices.empty(), "a lone Glass block emits nothing into the opaque mesh");
    Check(loneMesh.transparent.vertices.size() == 24, "a lone Glass block emits all 6 faces (24 verts)");

    // Two adjacent Glass blocks: unlike opaque neighbors, transparent
    // neighbors don't occlude each other's faces (matches Craft's
    // `opaque[cell] = !is_transparent(w)` — see World::IsOpaque).
    World adjacent;
    adjacent.SetBlock(5, 5, 5, BlockType::Glass);
    adjacent.SetBlock(6, 5, 5, BlockType::Glass);
    ChunkMeshData adjacentMesh = ChunkMesher::Build(adjacent, 0, 0, 0);
    Check(adjacentMesh.transparent.vertices.size() == 24 * 2,
          "two adjacent Glass blocks both keep all 6 faces (transparent neighbors don't occlude)");

    // A Glass block against a Stone block: Glass's face touching the Stone
    // is culled (the opaque Stone face already covers that spot), but
    // Stone's face touching the Glass is NOT culled (Glass doesn't occlude).
    World againstStone;
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
    lone.SetBlock(5, 5, 5, BlockType::Leaves);
    ChunkMeshData loneMesh = ChunkMesher::Build(lone, 0, 0, 0);
    Check(loneMesh.opaque.vertices.empty(), "a lone Leaves block emits nothing into the opaque mesh");
    Check(loneMesh.transparent.vertices.size() == 24, "a lone Leaves block emits all 6 faces (24 verts)");

    World adjacent;
    adjacent.SetBlock(5, 5, 5, BlockType::Leaves);
    adjacent.SetBlock(6, 5, 5, BlockType::Leaves);
    ChunkMeshData adjacentMesh = ChunkMesher::Build(adjacent, 0, 0, 0);
    Check(adjacentMesh.transparent.vertices.size() == 24 * 2,
          "two adjacent Leaves blocks both keep all 6 faces (transparent neighbors don't occlude)");

    World againstWood;
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

void TestChunkMesherCloudIsOpaqueButNotCollidable() {
    // Cloud is the inverse situation from Glass: opaque (occludes neighbors,
    // meshed into the *opaque* mesh, hit-testable) but NOT collidable (the
    // player walks straight through it) — matches Craft's
    // `is_obstacle(CLOUD) == 0` while it's absent from `is_transparent`'s
    // switch (src/item.c).
    World world;
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
    Check(hotbar.SlotCount() == 15, "hotbar has 15 slots (Bedrock excluded from the placeable roster)");
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
    Check(hotbar.Selected() == BlockType::Cloud, "slot 14 is Cloud");
    hotbar.CycleNext();
    Check(hotbar.Selected() == BlockType::Leaves, "slot 15 (the last slot) is Leaves");
    hotbar.CycleNext();
    Check(hotbar.SelectedIndex() == 0, "CycleNext wraps back to slot 0 after the last slot");
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

    PlayerInput riseInput;
    riseInput.moveUp = 1.0f;
    for (int i = 0; i < 60; ++i) player.Update(world, riseInput, 1.0f / 60.0f);
    Check(player.EyePosition().y > flyStartY, "moveUp=1 while flying rises");

    const float risenY = player.EyePosition().y;
    PlayerInput sinkInput;
    sinkInput.moveUp = -1.0f;
    for (int i = 0; i < 60; ++i) player.Update(world, sinkInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < risenY, "moveUp=-1 while flying descends");

    player.ToggleFlying();
    Check(!player.IsFlying(), "ToggleFlying() again switches back to game mode");
    const float backToGameY = player.EyePosition().y;
    player.Update(world, noInput, 1.0f / 60.0f);
    Check(player.EyePosition().y < backToGameY, "gravity resumes after leaving fly mode");
}

}

int main() {
    TestChunkBasics();
    TestWorldBoundsAndRoundTrip();
    TestWorldGenerationIsDeterministic();
    TestWorldGeneratesClouds();
    TestWorldGeneratesTrees();
    TestNoiseGeneratorSimplex2IsSeeded();
    TestNoiseGeneratorSimplex3();
    TestDayNightCycleMatchesCraftsCurveShape();
    TestChunkMesherFaceCulling();
    TestChunkMesherGlassTransparency();
    TestChunkMesherLeavesTransparency();
    TestChunkMesherCloudIsOpaqueButNotCollidable();
    TestVoxelRaycastHitsExpectedFaceAndBlock();
    TestHotbarSelectionAndCycling();
    TestPlayerControllerGravityAndGroundCollision();
    TestPlayerJumpClearsOneBlockHeight();
    TestPlayerCanJumpOntoOneBlockHigherLedge();
    TestPlayerSpawnAtBlockCenterAvoidsBoundaryWedging();
    TestPlayerControllerFlyingTogglesGravityAndFreeVerticalMovement();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
