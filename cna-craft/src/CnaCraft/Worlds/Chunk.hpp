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

    // Light-source overlay (CRAFT_PARITY.md §2.7/§4.3, plan.md §12.1 item 17
    // follow-up "light toggle") -- mirrors Craft's own separate `lights` Map
    // alongside its block-type `map` (src/main.c's `Chunk` struct): whether
    // a light has been toggled onto this cell is independent of what block
    // type occupies it, so this is its own parallel array, not a BlockDef
    // flag (a light is a property of a specific placed block instance, not
    // of a block type -- toggling Stone at one spot doesn't make all Stone
    // glow). A plain bool (not Craft's 0/15 int) is a deliberate
    // simplification: this project's light doesn't propagate/attenuate with
    // distance (see World.hpp's IsLightSource doc comment for why), so
    // there's no intensity value to represent, only on/off.
    [[nodiscard]] bool IsLightSource(int x, int y, int z) const;
    void SetLightSource(int x, int y, int z, bool on);

    static bool InBounds(int x, int y, int z);

    [[nodiscard]] bool IsDirty() const { return dirty_; }
    void MarkDirty() { dirty_ = true; }
    void ClearDirty() { dirty_ = false; }

private:
    static int Index(int x, int y, int z);

    std::array<BlockType, static_cast<std::size_t>(CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE> blocks_;
    std::array<bool, static_cast<std::size_t>(CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE> lightSources_{};
    bool dirty_ = true;
};

}
