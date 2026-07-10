#include "World.hpp"

#include "NoiseGenerator.hpp"

namespace CnaCraft::Worlds {

World::World() {
    chunks_.resize(static_cast<std::size_t>(WORLD_CHUNKS_X) * WORLD_CHUNKS_Y * WORLD_CHUNKS_Z);
    for (auto& chunk : chunks_) {
        chunk = std::make_unique<Chunk>();
    }
}

bool World::ChunkInBounds(int cx, int cy, int cz) {
    return cx >= 0 && cx < WORLD_CHUNKS_X &&
           cy >= 0 && cy < WORLD_CHUNKS_Y &&
           cz >= 0 && cz < WORLD_CHUNKS_Z;
}

bool World::InBounds(int x, int y, int z) {
    return x >= 0 && x < WORLD_SIZE_X &&
           y >= 0 && y < WORLD_SIZE_Y &&
           z >= 0 && z < WORLD_SIZE_Z;
}

int World::ChunkIndex(int cx, int cy, int cz) const {
    return cx + WORLD_CHUNKS_X * (cy + WORLD_CHUNKS_Y * cz);
}

Chunk& World::ChunkAt(int cx, int cy, int cz) {
    return *chunks_[static_cast<std::size_t>(ChunkIndex(cx, cy, cz))];
}

const Chunk& World::ChunkAt(int cx, int cy, int cz) const {
    return *chunks_[static_cast<std::size_t>(ChunkIndex(cx, cy, cz))];
}

BlockType World::GetBlock(int x, int y, int z) const {
    if (!InBounds(x, y, z)) return BlockType::Air;
    const int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
    const int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
    return ChunkAt(cx, cy, cz).GetBlock(lx, ly, lz);
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    if (!InBounds(x, y, z)) return;
    const int cx = x / CHUNK_SIZE, cy = y / CHUNK_SIZE, cz = z / CHUNK_SIZE;
    const int lx = x % CHUNK_SIZE, ly = y % CHUNK_SIZE, lz = z % CHUNK_SIZE;
    ChunkAt(cx, cy, cz).SetBlock(lx, ly, lz, type);

    // A block sitting on a chunk boundary affects the neighbor chunk's face
    // culling too (plan.md §0/§4 — Craft's "one-block neighbor overlap").
    if (lx == 0 && cx > 0) ChunkAt(cx - 1, cy, cz).MarkDirty();
    if (lx == CHUNK_SIZE - 1 && cx < WORLD_CHUNKS_X - 1) ChunkAt(cx + 1, cy, cz).MarkDirty();
    if (ly == 0 && cy > 0) ChunkAt(cx, cy - 1, cz).MarkDirty();
    if (ly == CHUNK_SIZE - 1 && cy < WORLD_CHUNKS_Y - 1) ChunkAt(cx, cy + 1, cz).MarkDirty();
    if (lz == 0 && cz > 0) ChunkAt(cx, cy, cz - 1).MarkDirty();
    if (lz == CHUNK_SIZE - 1 && cz < WORLD_CHUNKS_Z - 1) ChunkAt(cx, cy, cz + 1).MarkDirty();
}

bool World::IsSolid(int x, int y, int z) const {
    return GetBlockDef(GetBlock(x, y, z)).solid;
}

bool World::IsCollidable(int x, int y, int z) const {
    // AND'd with `solid` (not just `.collidable` alone) so Air can never be
    // collidable regardless of the field's default value — same defensive
    // pattern as IsOpaque's `solid && !transparent` below. Bug caught here
    // during development: BlockDef::collidable defaults to true, and Air's
    // fallback initializer only overrides `solid`, so a bare `.collidable`
    // read made empty space collidable, freezing the player instantly.
    const BlockDef def = GetBlockDef(GetBlock(x, y, z));
    return def.solid && def.collidable;
}

bool World::IsOpaque(int x, int y, int z) const {
    const BlockDef def = GetBlockDef(GetBlock(x, y, z));
    return def.solid && !def.transparent;
}

bool World::IsBreakable(int x, int y, int z) const {
    const BlockDef def = GetBlockDef(GetBlock(x, y, z));
    return def.solid && def.breakable;
}

namespace {
// Craft's create_world (world.c) uses a beach rule: `if (h <= t) { h = t;
// w = SAND; }` with t=12 -- low-elevation columns become an entirely
// sand-colored column (Craft has no separate stone/dirt/bedrock layering at
// all; every column is one uniform block type). cna-craft's own layered
// terrain (Bedrock/Stone/Dirt/Grass per column, not part of any Craft
// citation -- an independent design already in place before this session)
// doesn't have an equivalent to replace wholesale, so it's adapted as a
// low-elevation *surface* rule instead: the same "low ground is sandy"
// idea, applied only to the Dirt/Grass layers, leaving Bedrock/Stone
// underneath unchanged. kSandMaxHeight now reuses Craft's literal t=12
// directly (previously scaled down to 10 for this project's old,
// much-smaller height range — no longer needed now that
// NoiseGenerator::Height itself ports Craft's real height formula and its
// own h<=12 clamp, per user decision 2026-07-10; see NoiseGenerator.cpp).
constexpr int kSandMaxHeight = 12;
}

void World::Generate(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(seed, x, z);
            const bool sandyColumn = height <= kSandMaxHeight;
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                BlockType type;
                if (y == 0) {
                    type = BlockType::Bedrock;
                } else if (y < height - 4) {
                    type = BlockType::Stone;
                } else if (y < height) {
                    type = sandyColumn ? BlockType::Sand : BlockType::Dirt;
                } else if (y == height) {
                    type = sandyColumn ? BlockType::Sand : BlockType::Grass;
                } else {
                    type = BlockType::Air;
                }
                SetBlock(x, y, z, type);
            }
        }
    }

    GenerateTrees(seed);
    GenerateGrassDecoration(seed);
    GenerateFlowers(seed);
    GenerateClouds(seed);
}

namespace {
// Craft's world.c places trees via a `simplex2(x, z, 6, 0.5, 2) > 0.84`
// trigger on grass columns (verified against the real checkout at
// /rv/data/development/github.com/other/Craft/src/world.c): a 7-tall Wood
// trunk at (x, h..h+7, z), plus a Leaves canopy blob for y in [h+3, h+8) and
// (ox, oz) in [-3, 3] wherever ox*ox + oz*oz + (y-(h+4))*(y-(h+4)) < 11.
// Ported with the same trigger/shape constants. Not ported: Craft's
// per-chunk `dx-4 < 0` edge-margin check — that exists only because Craft
// generates chunks independently and needs a safety margin so canopy
// geometry never reaches into an ungenerated neighboring chunk; this
// project generates the whole bounded world in one deterministic pass, and
// SetBlock/GetBlock already treat out-of-world-bounds coordinates as a safe
// no-op/Air, so the margin has no equivalent purpose here.
constexpr int kTreeNoiseOctaves = 6;
constexpr float kTreeNoisePersistence = 0.5f;
constexpr float kTreeNoiseLacunarity = 2.0f;
constexpr float kTreeThreshold = 0.84f;
constexpr int kCanopyRadius = 3;
constexpr int kCanopyDistanceSquaredMax = 11;
constexpr int kTrunkHeight = 7;
}

void World::GenerateTrees(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int h = NoiseGenerator::Height(seed, x, z);
            if (GetBlock(x, h, z) != BlockType::Grass) continue; // Craft: only on its `w == 1` grass columns

            const float density = NoiseGenerator::Simplex2(seed, static_cast<float>(x), static_cast<float>(z),
                                                             kTreeNoiseOctaves, kTreeNoisePersistence,
                                                             kTreeNoiseLacunarity);
            if (density <= kTreeThreshold) continue;

            for (int y = h + 3; y < h + 8; ++y) {
                const int dy = y - (h + 4);
                for (int ox = -kCanopyRadius; ox <= kCanopyRadius; ++ox) {
                    for (int oz = -kCanopyRadius; oz <= kCanopyRadius; ++oz) {
                        const int d = ox * ox + oz * oz + dy * dy;
                        // Deliberate deviation from Craft's unconditional write: only
                        // fill Air, so one tree's canopy never overwrites a
                        // neighboring tree's already-placed trunk/canopy.
                        if (d < kCanopyDistanceSquaredMax && GetBlock(x + ox, y, z + oz) == BlockType::Air) {
                            SetBlock(x + ox, y, z + oz, BlockType::Leaves);
                        }
                    }
                }
            }
            for (int y = h; y < h + kTrunkHeight; ++y) {
                SetBlock(x, y, z, BlockType::Wood); // unconditional, same as Craft — trunk wins over canopy
            }
        }
    }
}

namespace {
// Craft's world.c places TallGrass (item id 17) via a `simplex2(-x*0.1,
// z*0.1, 4, 0.8, 2) > 0.6` trigger, gated the same as trees on `w == 1`
// grass columns, placed directly on top of the surface block (verified
// against the real checkout). Ported with the same trigger constants.
constexpr float kGrassNoiseScale = 0.1f;
constexpr int kGrassNoiseOctaves = 4;
constexpr float kGrassNoisePersistence = 0.8f;
constexpr float kGrassNoiseLacunarity = 2.0f;
constexpr float kGrassThreshold = 0.6f;
}

void World::GenerateGrassDecoration(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int h = NoiseGenerator::Height(seed, x, z);
            if (GetBlock(x, h, z) != BlockType::Grass) continue; // Craft: only on its `w == 1` grass columns
            if (h + 1 >= WORLD_SIZE_Y) continue;
            if (GetBlock(x, h + 1, z) != BlockType::Air) continue; // don't overwrite a tree/cloud already there

            const float density = NoiseGenerator::Simplex2(seed, static_cast<float>(-x) * kGrassNoiseScale,
                                                             static_cast<float>(z) * kGrassNoiseScale,
                                                             kGrassNoiseOctaves, kGrassNoisePersistence,
                                                             kGrassNoiseLacunarity);
            if (density > kGrassThreshold) {
                SetBlock(x, h + 1, z, BlockType::TallGrass);
            }
        }
    }
}

namespace {
// Craft's world.c places a flower via `simplex2(x*0.05, -z*0.05, 4, 0.8, 2)
// > 0.7`, then picks one of 7 flower colors with a second noise sample
// (`18 + simplex2(x*0.1, z*0.1, 4, 0.8, 2) * 7`) — verified against the
// real checkout. This project has one representative Flower type
// (BlockType.hpp), so only the placement trigger is ported; the color-pick
// sample has nothing to select between and is skipped.
constexpr float kFlowerNoiseScale = 0.05f;
constexpr int kFlowerNoiseOctaves = 4;
constexpr float kFlowerNoisePersistence = 0.8f;
constexpr float kFlowerNoiseLacunarity = 2.0f;
constexpr float kFlowerThreshold = 0.7f;
}

void World::GenerateFlowers(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int h = NoiseGenerator::Height(seed, x, z);
            if (GetBlock(x, h, z) != BlockType::Grass) continue; // Craft: only on its `w == 1` grass columns
            if (h + 1 >= WORLD_SIZE_Y) continue;
            if (GetBlock(x, h + 1, z) != BlockType::Air) continue; // don't overwrite grass/tree/cloud already there

            const float density = NoiseGenerator::Simplex2(seed, static_cast<float>(x) * kFlowerNoiseScale,
                                                             static_cast<float>(-z) * kFlowerNoiseScale,
                                                             kFlowerNoiseOctaves, kFlowerNoisePersistence,
                                                             kFlowerNoiseLacunarity);
            if (density > kFlowerThreshold) {
                SetBlock(x, h + 1, z, BlockType::Flower);
            }
        }
    }
}

namespace {
// Craft's world.c places CLOUD blocks via a 3D density check:
// `simplex3(x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2) > 0.75`, in a y=64..72
// band near its own world's ceiling (verified against the real checkout at
// /rv/data/development/github.com/other/Craft/src/world.c — frequency
// scale, octave count, and threshold are ported as-is; only the y-band
// changes, since this project's WORLD_SIZE_Y=64 is far shorter than Craft's).
// This project's own NoiseGenerator::Height() clamps terrain to
// kMaxHeight=56, so the band below is placed just above that, near this
// world's own ceiling (y=63).
constexpr int kCloudBandStartY = 58;
constexpr int kCloudBandEndY = 62; // inclusive
constexpr float kCloudNoiseScaleXZ = 0.01f;
constexpr float kCloudNoiseScaleY = 0.1f;
constexpr int kCloudNoiseOctaves = 8;
constexpr float kCloudNoisePersistence = 0.5f;
constexpr float kCloudNoiseLacunarity = 2.0f;
constexpr float kCloudThreshold = 0.75f;
}

void World::GenerateClouds(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            for (int y = kCloudBandStartY; y <= kCloudBandEndY; ++y) {
                if (GetBlock(x, y, z) != BlockType::Air) continue; // never overwrite terrain
                const float density = NoiseGenerator::Simplex3(
                    seed, static_cast<float>(x) * kCloudNoiseScaleXZ, static_cast<float>(y) * kCloudNoiseScaleY,
                    static_cast<float>(z) * kCloudNoiseScaleXZ, kCloudNoiseOctaves, kCloudNoisePersistence,
                    kCloudNoiseLacunarity);
                if (density > kCloudThreshold) {
                    SetBlock(x, y, z, BlockType::Cloud);
                }
            }
        }
    }
}

}
