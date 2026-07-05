#pragma once

#include <cstdint>

namespace CnaCraft::Worlds {

enum class BlockType : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Bedrock,
    Count
};

// Atlas tile indices are placeholders (§5/§6 of plan.md): a real texture atlas
// is a later milestone (M6). Grass is the only block with three distinct
// faces; everything else reuses one tile for all six faces.
struct BlockDef {
    bool solid;
    int topTile;
    int sideTile;
    int bottomTile;
};

constexpr BlockDef GetBlockDef(BlockType type) {
    switch (type) {
        case BlockType::Grass:   return BlockDef{true, 0, 1, 2};
        case BlockType::Dirt:    return BlockDef{true, 2, 2, 2};
        case BlockType::Stone:   return BlockDef{true, 3, 3, 3};
        case BlockType::Sand:    return BlockDef{true, 4, 4, 4};
        case BlockType::Bedrock: return BlockDef{true, 5, 5, 5};
        case BlockType::Air:
        case BlockType::Count:
        default:
            return BlockDef{false, 0, 0, 0};
    }
}

}
