#pragma once

#include <array>
#include <cstddef>

#include "BlockType.hpp"

namespace CnaCraft::Worlds {

constexpr int CHUNK_SIZE = 16;

// Floor-divides a world-space block coordinate by CHUNK_SIZE to get its
// chunk coordinate -- NOT plain integer division (which truncates toward
// zero), since the streamed world (plan.md §12.1 item 19) extends in every
// direction from spawn and coordinates can be negative. Matches Craft's own
// `chunked()` (main.c: `floorf(roundf(x) / CHUNK_SIZE)`) semantics for
// negative inputs.
constexpr int ChunkCoordOf(int blockCoord) {
    return (blockCoord >= 0) ? blockCoord / CHUNK_SIZE : (blockCoord - CHUNK_SIZE + 1) / CHUNK_SIZE;
}

// The chunk-local coordinate (always in [0, CHUNK_SIZE)) for a world-space
// block coordinate -- NOT plain `%`, which can return a negative remainder
// for negative inputs in C++.
constexpr int ChunkLocalCoordOf(int blockCoord) {
    return blockCoord - ChunkCoordOf(blockCoord) * CHUNK_SIZE;
}

// A CHUNK_SIZE^3 cube of blocks. Coordinates passed to GetBlock/SetBlock are
// chunk-local (each in [0, CHUNK_SIZE)); see World for world-space access.
class Chunk {
public:
    Chunk();

    [[nodiscard]] BlockType GetBlock(int x, int y, int z) const;
    void SetBlock(int x, int y, int z, BlockType type);

    static bool InBounds(int x, int y, int z);

    [[nodiscard]] bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

private:
    static int Index(int x, int y, int z);

    std::array<BlockType, static_cast<std::size_t>(CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE> blocks_;
    bool dirty_ = true;
};

}
