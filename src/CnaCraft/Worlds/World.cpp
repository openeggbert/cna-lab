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

void World::Generate(std::uint32_t seed) {
    for (int x = 0; x < WORLD_SIZE_X; ++x) {
        for (int z = 0; z < WORLD_SIZE_Z; ++z) {
            const int height = NoiseGenerator::Height(seed, x, z);
            for (int y = 0; y < WORLD_SIZE_Y; ++y) {
                BlockType type;
                if (y == 0) {
                    type = BlockType::Bedrock;
                } else if (y < height - 4) {
                    type = BlockType::Stone;
                } else if (y < height) {
                    type = BlockType::Dirt;
                } else if (y == height) {
                    type = BlockType::Grass;
                } else {
                    type = BlockType::Air;
                }
                SetBlock(x, y, z, type);
            }
        }
    }
}

}
