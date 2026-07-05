#pragma once

#include <array>
#include <cstddef>

#include "BlockType.hpp"

namespace CnaCraft::Worlds {

constexpr int CHUNK_SIZE = 16;

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
