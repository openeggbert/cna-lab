#pragma once

#include <cstdint>

namespace CnaCraft::Worlds {

// Roster expanded toward Craft's item.h (plan.md §11.2) with the plain
// solid/opaque cube blocks, plus Glass (transparent but still solid/
// collidable — see "Transparency for glass" below). Leaves and non-solid
// decoration (Cloud) remain deferred, since Cloud needs World::IsSolid to
// stop doubling as "is this block rendered as an opaque cube" for
// *collision* too (Glass didn't need that: it's collidable, just not
// opaque). Plants (tall grass, flowers) and Chest are deferred to
// "Non-cubic plant geometry" — they need their own mesh shape, not just a
// new tile.
enum class BlockType : std::uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Sand,
    Stone,
    Cobblestone,
    Brick,
    Plank,
    Wood,
    Cement,
    LightStone,
    DarkStone,
    Snow,
    Glass,
    Bedrock,
    Count
};

// Atlas tile indices are placeholders (§5/§6 of plan.md): a real texture atlas
// is a later milestone (M6/§11.2). Most blocks reuse one tile for all six
// faces; Grass/Wood/Snow have distinct top/side/bottom tiles like Craft's own
// blocks[256][6] table.
//
// `transparent` (plan.md §11.2 "Transparency for glass"): true only for
// Glass. `World::IsOpaque` is `solid && !transparent` — this is the flag
// ChunkMesher's neighbor-face check uses (Craft: `opaque[cell] =
// !is_transparent(w)` in src/main.c), so a solid-but-transparent block like
// Glass still gets meshed and still blocks the player (`World::IsSolid`
// stays keyed on `solid` alone), it just doesn't occlude its neighbors'
// faces — and its own faces against a genuinely opaque neighbor are culled,
// same as Craft.
struct BlockDef {
    bool solid;
    int topTile;
    int sideTile;
    int bottomTile;
    bool transparent = false;
};

constexpr BlockDef GetBlockDef(BlockType type) {
    switch (type) {
        case BlockType::Grass:       return BlockDef{true, 0, 1, 2};
        case BlockType::Dirt:        return BlockDef{true, 2, 2, 2};
        case BlockType::Sand:        return BlockDef{true, 4, 4, 4};
        case BlockType::Stone:       return BlockDef{true, 3, 3, 3};
        case BlockType::Cobblestone: return BlockDef{true, 13, 13, 13};
        case BlockType::Brick:       return BlockDef{true, 6, 6, 6};
        case BlockType::Plank:       return BlockDef{true, 10, 10, 10};
        case BlockType::Wood:        return BlockDef{true, 8, 7, 8};
        case BlockType::Cement:      return BlockDef{true, 9, 9, 9};
        case BlockType::LightStone:  return BlockDef{true, 14, 14, 14};
        case BlockType::DarkStone:   return BlockDef{true, 15, 15, 15};
        case BlockType::Snow:        return BlockDef{true, 11, 12, 2};
        case BlockType::Glass:       return BlockDef{true, 16, 16, 16, true};
        case BlockType::Bedrock:     return BlockDef{true, 5, 5, 5};
        case BlockType::Air:
        case BlockType::Count:
        default:
            return BlockDef{false, 0, 0, 0};
    }
}

constexpr const char* GetBlockName(BlockType type) {
    switch (type) {
        case BlockType::Air:         return "Air";
        case BlockType::Grass:       return "Grass";
        case BlockType::Dirt:        return "Dirt";
        case BlockType::Sand:        return "Sand";
        case BlockType::Stone:       return "Stone";
        case BlockType::Cobblestone: return "Cobblestone";
        case BlockType::Brick:       return "Brick";
        case BlockType::Plank:       return "Plank";
        case BlockType::Wood:        return "Wood";
        case BlockType::Cement:      return "Cement";
        case BlockType::LightStone:  return "LightStone";
        case BlockType::DarkStone:   return "DarkStone";
        case BlockType::Snow:        return "Snow";
        case BlockType::Glass:       return "Glass";
        case BlockType::Bedrock:     return "Bedrock";
        default:                     return "Unknown";
    }
}

}
