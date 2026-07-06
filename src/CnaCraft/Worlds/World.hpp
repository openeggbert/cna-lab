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

    Chunk& ChunkAt(int cx, int cy, int cz);
    [[nodiscard]] const Chunk& ChunkAt(int cx, int cy, int cz) const;

    static bool ChunkInBounds(int cx, int cy, int cz);
    static bool InBounds(int x, int y, int z);

private:
    [[nodiscard]] int ChunkIndex(int cx, int cy, int cz) const;

    std::vector<std::unique_ptr<Chunk>> chunks_;
};

}
