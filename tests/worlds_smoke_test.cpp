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

    world.SetBlock(20, 5, 20, BlockType::Stone); // crosses a chunk boundary (CHUNK_SIZE=16)
    Check(world.GetBlock(20, 5, 20) == BlockType::Stone, "world set/get round-trip across chunk boundary");
    Check(world.IsSolid(20, 5, 20), "world IsSolid true for Stone");

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

    // Collision must be unaffected: Glass is transparent but still solid.
    Check(againstStone.IsSolid(5, 5, 5), "Glass is still solid/collidable despite being transparent");
    Check(!againstStone.IsOpaque(5, 5, 5), "Glass is not opaque (doesn't occlude neighbor faces)");
    Check(againstStone.IsOpaque(6, 5, 5), "Stone is opaque");
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
    Check(hotbar.SlotCount() == 13, "hotbar has 13 slots (Bedrock excluded from the placeable roster)");
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
    Check(hotbar.Selected() == BlockType::Glass, "slot 13 (the last slot) is Glass");
    hotbar.CycleNext();
    Check(hotbar.SelectedIndex() == 0, "CycleNext wraps back to slot 0 after the last slot");
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
    TestNoiseGeneratorSimplex2IsSeeded();
    TestChunkMesherFaceCulling();
    TestChunkMesherGlassTransparency();
    TestVoxelRaycastHitsExpectedFaceAndBlock();
    TestHotbarSelectionAndCycling();
    TestPlayerControllerGravityAndGroundCollision();
    TestPlayerControllerFlyingTogglesGravityAndFreeVerticalMovement();

    if (g_failures == 0) {
        std::printf("\nAll checks passed.\n");
        return 0;
    }
    std::fprintf(stderr, "\n%d check(s) FAILED.\n", g_failures);
    return 1;
}
