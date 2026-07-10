#include "World.hpp"

#include "NoiseGenerator.hpp"

namespace CnaCraft::Worlds {

World::World() = default;

ColumnKey World::PackColumnKey(int cx, int cz) {
    return (static_cast<ColumnKey>(cx) << 32) | static_cast<std::uint32_t>(cz);
}

void World::UnpackColumnKey(ColumnKey key, int& cx, int& cz) {
    cx = static_cast<int>(key >> 32);
    cz = static_cast<int>(static_cast<std::uint32_t>(key & 0xffffffffLL));
}

bool World::InBounds(int x, int y, int z) {
    // Only Y is a real, enforced bound now (plan.md §12.1 item 19) -- x,z
    // are unbounded, "in range" is decided by whether their column is
    // loaded (TryChunkAt returning nullptr), not by a fixed extent.
    (void)x;
    (void)z;
    return y >= 0 && y < WORLD_SIZE_Y;
}

Chunk* World::TryChunkAt(int cx, int cy, int cz) {
    if (cy < 0 || cy >= WORLD_CHUNKS_Y) return nullptr;
    const auto it = columns_.find(PackColumnKey(cx, cz));
    if (it == columns_.end()) return nullptr;
    return it->second[static_cast<std::size_t>(cy)].get();
}

const Chunk* World::TryChunkAt(int cx, int cy, int cz) const {
    if (cy < 0 || cy >= WORLD_CHUNKS_Y) return nullptr;
    const auto it = columns_.find(PackColumnKey(cx, cz));
    if (it == columns_.end()) return nullptr;
    return it->second[static_cast<std::size_t>(cy)].get();
}

Chunk& World::ChunkAt(int cx, int cy, int cz) { return *TryChunkAt(cx, cy, cz); }
const Chunk& World::ChunkAt(int cx, int cy, int cz) const { return *TryChunkAt(cx, cy, cz); }

bool World::IsColumnLoaded(int cx, int cz) const {
    return columns_.find(PackColumnKey(cx, cz)) != columns_.end();
}

void World::AllocateColumn(int cx, int cz) {
    const ColumnKey key = PackColumnKey(cx, cz);
    if (columns_.find(key) != columns_.end()) return; // already loaded, idempotent no-op
    auto& column = columns_[key];
    for (auto& chunkPtr : column) chunkPtr = std::make_unique<Chunk>();
}

void World::UnloadColumn(int cx, int cz) { columns_.erase(PackColumnKey(cx, cz)); }

void World::InstallChunkCopy(int cx, int cy, int cz, const Chunk& chunk) {
    AllocateColumn(cx, cz);
    if (Chunk* target = TryChunkAt(cx, cy, cz)) *target = chunk;
}

std::array<Chunk, WORLD_CHUNKS_Y> World::CopyColumn(int cx, int cz) const {
    std::array<Chunk, WORLD_CHUNKS_Y> result;
    for (int cy = 0; cy < WORLD_CHUNKS_Y; ++cy) {
        if (const Chunk* chunk = TryChunkAt(cx, cy, cz)) result[static_cast<std::size_t>(cy)] = *chunk;
    }
    return result;
}

void World::AdoptColumnCopy(int cx, int cz, const std::array<Chunk, WORLD_CHUNKS_Y>& chunks) {
    for (int cy = 0; cy < WORLD_CHUNKS_Y; ++cy) {
        InstallChunkCopy(cx, cy, cz, chunks[static_cast<std::size_t>(cy)]);
    }
}

BlockType World::GetBlock(int x, int y, int z) const {
    if (!InBounds(x, y, z)) return BlockType::Air;
    const Chunk* chunk = TryChunkAt(ChunkCoordOf(x), y / CHUNK_SIZE, ChunkCoordOf(z));
    if (!chunk) return BlockType::Air;
    return chunk->GetBlock(ChunkLocalCoordOf(x), y % CHUNK_SIZE, ChunkLocalCoordOf(z));
}

void World::SetBlock(int x, int y, int z, BlockType type) {
    if (!InBounds(x, y, z)) return;
    const int cx = ChunkCoordOf(x), cy = y / CHUNK_SIZE, cz = ChunkCoordOf(z);
    Chunk* chunk = TryChunkAt(cx, cy, cz);
    if (!chunk) return; // column not loaded -- same graceful-degradation pattern as an out-of-range write before
    const int lx = ChunkLocalCoordOf(x), ly = y % CHUNK_SIZE, lz = ChunkLocalCoordOf(z);
    chunk->SetBlock(lx, ly, lz, type);

    // A block sitting on a chunk boundary affects the neighbor chunk's face
    // culling too (plan.md §0/§4 — Craft's "one-block neighbor overlap").
    // Neighbor chunks may not be loaded (unlike the old fixed grid, where
    // every neighbor within bounds always existed) -- TryChunkAt handles
    // that gracefully, same as everywhere else.
    if (lx == 0) { if (Chunk* n = TryChunkAt(cx - 1, cy, cz)) n->MarkDirty(); }
    if (lx == CHUNK_SIZE - 1) { if (Chunk* n = TryChunkAt(cx + 1, cy, cz)) n->MarkDirty(); }
    if (ly == 0) { if (Chunk* n = TryChunkAt(cx, cy - 1, cz)) n->MarkDirty(); }
    if (ly == CHUNK_SIZE - 1) { if (Chunk* n = TryChunkAt(cx, cy + 1, cz)) n->MarkDirty(); }
    if (lz == 0) { if (Chunk* n = TryChunkAt(cx, cy, cz - 1)) n->MarkDirty(); }
    if (lz == CHUNK_SIZE - 1) { if (Chunk* n = TryChunkAt(cx, cy, cz + 1)) n->MarkDirty(); }
}

bool World::IsLightSource(int x, int y, int z) const {
    if (!InBounds(x, y, z)) return false;
    const Chunk* chunk = TryChunkAt(ChunkCoordOf(x), y / CHUNK_SIZE, ChunkCoordOf(z));
    if (!chunk) return false;
    return chunk->IsLightSource(ChunkLocalCoordOf(x), y % CHUNK_SIZE, ChunkLocalCoordOf(z));
}

void World::SetLightSource(int x, int y, int z, bool on) {
    if (!InBounds(x, y, z)) return;
    Chunk* chunk = TryChunkAt(ChunkCoordOf(x), y / CHUNK_SIZE, ChunkCoordOf(z));
    if (!chunk) return; // column not loaded -- same graceful-degradation pattern as SetBlock
    chunk->SetLightSource(ChunkLocalCoordOf(x), y % CHUNK_SIZE, ChunkLocalCoordOf(z), on);
}

void World::SetBlockAndRecordEdit(int x, int y, int z, BlockType type) {
    SetBlock(x, y, z, type);
    if (!InBounds(x, y, z)) return; // matches SetBlock's own no-op-out-of-bounds guard
    recordedEdits_.push_back(BlockEdit{x, y, z, type});
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

int World::HighestCollidableY(int x, int z) const {
    for (int y = WORLD_SIZE_Y - 1; y >= 0; --y) {
        if (IsCollidable(x, y, z)) return y;
    }
    return -1;
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
// underneath unchanged. kSandMaxHeight reuses Craft's literal t=12 directly.
constexpr int kSandMaxHeight = 12;
}

void World::Generate(std::uint32_t seed) {
    for (int cx = 0; cx < WORLD_CHUNKS_X; ++cx) {
        for (int cz = 0; cz < WORLD_CHUNKS_Z; ++cz) {
            GenerateColumn(cx, cz, seed);
        }
    }
}

void World::GenerateColumn(int cx, int cz, std::uint32_t seed) {
    AllocateColumn(cx, cz);
    GenerateTerrainColumn(cx, cz, seed);
    GenerateTreesColumn(cx, cz, seed);
    GenerateGrassDecorationColumn(cx, cz, seed);
    GenerateFlowersColumn(cx, cz, seed);
    GenerateCloudsColumn(cx, cz, seed);
}

void World::GenerateTerrainColumn(int cx, int cz, std::uint32_t seed) {
    const int originX = cx * CHUNK_SIZE, originZ = cz * CHUNK_SIZE;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = originX + lx, z = originZ + lz;
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
}

namespace {
// Craft's world.c places trees via a `simplex2(x, z, 6, 0.5, 2) > 0.84`
// trigger on grass columns (verified against the real checkout at
// /rv/data/development/github.com/other/Craft/src/world.c): a 7-tall Wood
// trunk at (x, h..h+7, z), plus a Leaves canopy blob for y in [h+3, h+8) and
// (ox, oz) in [-3, 3] wherever ox*ox + oz*oz + (y-(h+4))*(y-(h+4)) < 11.
// Ported with the same trigger/shape constants, plus (plan.md §12.1 item 19)
// Craft's real per-chunk margin check (world.c: `dx-4<0 || dz-4<0 ||
// dx+4>=CHUNK_SIZE || dz+4>=CHUNK_SIZE`), adapted to this project's own
// kCanopyRadius=3 rather than Craft's literal 4 -- skips placing a tree
// whose canopy would cross this column's own local x/z boundary, since
// under streaming a neighboring column may not be loaded yet (this margin
// check has no equivalent purpose in the old whole-world-at-once
// generation pass this replaces, where SetBlock/GetBlock across the whole
// bounded world were always a safe no-op/Air either way).
constexpr int kTreeNoiseOctaves = 6;
constexpr float kTreeNoisePersistence = 0.5f;
constexpr float kTreeNoiseLacunarity = 2.0f;
constexpr float kTreeThreshold = 0.84f;
constexpr int kCanopyRadius = 3;
constexpr int kCanopyDistanceSquaredMax = 11;
constexpr int kTrunkHeight = 7;
}

void World::GenerateTreesColumn(int cx, int cz, std::uint32_t seed) {
    const int originX = cx * CHUNK_SIZE, originZ = cz * CHUNK_SIZE;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            // Margin check (see the comment above GenerateTreesColumn's
            // declaration): a tree here could never place its canopy
            // without crossing into a neighboring, possibly-unloaded column.
            if (lx < kCanopyRadius || lx >= CHUNK_SIZE - kCanopyRadius ||
                lz < kCanopyRadius || lz >= CHUNK_SIZE - kCanopyRadius) {
                continue;
            }

            const int x = originX + lx, z = originZ + lz;
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

void World::GenerateGrassDecorationColumn(int cx, int cz, std::uint32_t seed) {
    const int originX = cx * CHUNK_SIZE, originZ = cz * CHUNK_SIZE;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = originX + lx, z = originZ + lz;
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
// > 0.7`, then picks one of its 6 flower colors with a second noise sample
// (`18 + simplex2(x*0.1, z*0.1, 4, 0.8, 2) * 7`) — verified against the
// real checkout. Now that all 6 colors exist as BlockTypes (CRAFT_PARITY.md
// §2.2/§3.7, added 2026-07-10), both passes are ported.
constexpr float kFlowerNoiseScale = 0.05f;
constexpr int kFlowerNoiseOctaves = 4;
constexpr float kFlowerNoisePersistence = 0.8f;
constexpr float kFlowerNoiseLacunarity = 2.0f;
constexpr float kFlowerThreshold = 0.7f;

constexpr float kFlowerColorNoiseScale = 0.1f;
constexpr int kFlowerColorNoiseOctaves = 4;
constexpr float kFlowerColorNoisePersistence = 0.8f;
constexpr float kFlowerColorNoiseLacunarity = 2.0f;

// Craft's real tile order (item.h: YELLOW_FLOWER=18 .. BLUE_FLOWER=23).
constexpr BlockType kFlowerColors[6] = {BlockType::Flower,       BlockType::RedFlower, BlockType::PurpleFlower,
                                         BlockType::SunFlower,    BlockType::WhiteFlower, BlockType::BlueFlower};

// Craft's literal `18 + noise*7` can overshoot past its own 6 valid color
// IDs (18-23) when the noise sample drifts slightly above 1.0 — an upstream
// quirk (an out-of-range block ID silently renders as nothing), not
// something worth reproducing. Clamped here to always land on one of the 6
// real colors.
BlockType PickFlowerColor(float noise) {
    int index = static_cast<int>(noise * 6.0f);
    if (index < 0) index = 0;
    if (index >= 6) index = 5;
    return kFlowerColors[index];
}
}

void World::GenerateFlowersColumn(int cx, int cz, std::uint32_t seed) {
    const int originX = cx * CHUNK_SIZE, originZ = cz * CHUNK_SIZE;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = originX + lx, z = originZ + lz;
            const int h = NoiseGenerator::Height(seed, x, z);
            if (GetBlock(x, h, z) != BlockType::Grass) continue; // Craft: only on its `w == 1` grass columns
            if (h + 1 >= WORLD_SIZE_Y) continue;
            if (GetBlock(x, h + 1, z) != BlockType::Air) continue; // don't overwrite grass/tree/cloud already there

            const float density = NoiseGenerator::Simplex2(seed, static_cast<float>(x) * kFlowerNoiseScale,
                                                             static_cast<float>(-z) * kFlowerNoiseScale,
                                                             kFlowerNoiseOctaves, kFlowerNoisePersistence,
                                                             kFlowerNoiseLacunarity);
            if (density > kFlowerThreshold) {
                const float colorNoise =
                    NoiseGenerator::Simplex2(seed, static_cast<float>(x) * kFlowerColorNoiseScale,
                                              static_cast<float>(z) * kFlowerColorNoiseScale,
                                              kFlowerColorNoiseOctaves, kFlowerColorNoisePersistence,
                                              kFlowerColorNoiseLacunarity);
                SetBlock(x, h + 1, z, PickFlowerColor(colorNoise));
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

void World::GenerateCloudsColumn(int cx, int cz, std::uint32_t seed) {
    const int originX = cx * CHUNK_SIZE, originZ = cz * CHUNK_SIZE;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            const int x = originX + lx, z = originZ + lz;
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
