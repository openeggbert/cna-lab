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
    TestNoiseGeneratorSimplex2IsSeeded();
    TestChunkMesherFaceCulling();
    TestChunkMesherGlassTransparency();
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
