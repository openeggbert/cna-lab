#pragma once

#include <cstdint>

namespace CnaCraft::Worlds {

// Roster expanded toward Craft's item.h (plan.md §11.2) with the plain
// solid/opaque cube blocks, Glass (transparent but still solid/collidable —
// see "Transparency for glass"), Cloud (opaque but NOT collidable — see
// "Clouds" below), Leaves (transparent like Glass — used by
// World::GenerateTrees, plan.md §11.1 "Trees and plants"), and TallGrass
// (CRAFT_PARITY.md §3.7 "Non-cubic plant geometry" — the first plant-shaped
// block, a cross-quad billboard instead of a cube; see BlockDef.plant
// below). Other plant colors (flowers) and Chest remain deferred — same
// geometry path once picked up, just a new tile/BlockType.
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
    Cloud,
    Leaves,
    TallGrass,
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
//
// `collidable` (plan.md §11.2 "Clouds"): false only for Cloud — the inverse
// situation from Glass. Craft's `is_obstacle(CLOUD)` returns 0 (walk
// through it) while `is_transparent(CLOUD)` is *not* set (it still occludes
// neighbors and gets hit-tested/broken like any other block — Craft's
// `_hit_test` treats any `map_get() > 0` cell as hittable, clouds included).
// So Cloud is `solid=true` (meshed, hittable via `World::IsSolid`),
// `transparent=false` (opaque, occludes neighbors via `World::IsOpaque`),
// `collidable=false` (the player passes through it — `World::IsCollidable`,
// used only by `PlayerController::CollidesAt`).
//
// `breakable` (CRAFT_PARITY.md §2.5): false only for Bedrock. Craft itself
// has no Bedrock/unbreakable-boundary block at all — its `is_destructable`
// only excludes EMPTY and CLOUD. cna-craft added Bedrock as a
// "world-boundary block, not meant to be placed by the player" (see
// Hotbar.hpp), but until this flag existed it *could* still be mined away,
// defeating that stated purpose. `World::IsBreakable` used only by
// CnaCraftGame's left-click handler; meshing/occlusion/collision
// (IsSolid/IsOpaque/IsCollidable) are unaffected — Bedrock still meshes,
// occludes, and blocks the player exactly like any other solid block.
//
// `plant` (CRAFT_PARITY.md §3.7): true only for TallGrass. Ports Craft's
// `is_plant(w)` (item.c) — a plant block is `solid=true` (still meshed/
// hit-testable via World::IsSolid, still breakable) but `collidable=false`
// (Craft's `is_obstacle` returns 0 for plants — the player walks through
// them, same non-collidable pattern as Cloud, just for the opposite
// "walk through this decoration" reason rather than Cloud's "walk through
// this sky element") and `transparent=true` (plants don't occlude
// neighboring faces, matching Craft's `is_transparent(w)` also covering
// plants). `ChunkMesher` reads this flag to switch a block's mesh emission
// from 6 cube faces to a cross-quad billboard (`topTile` doubles as "the"
// single plant texture — sideTile/bottomTile are unused for plants).
struct BlockDef {
    bool solid;
    int topTile;
    int sideTile;
    int bottomTile;
    bool transparent = false;
    bool collidable = true;
    bool breakable = true;
    bool plant = false;
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
        case BlockType::Glass:       return BlockDef{true, 16, 16, 16, /*transparent=*/true};
        case BlockType::Cloud:       return BlockDef{true, 17, 17, 17, /*transparent=*/false, /*collidable=*/false};
        case BlockType::Leaves:      return BlockDef{true, 18, 18, 18, /*transparent=*/true};
        case BlockType::TallGrass:
            return BlockDef{true, 19, 19, 19, /*transparent=*/true, /*collidable=*/false,
                             /*breakable=*/true, /*plant=*/true};
        case BlockType::Bedrock:
            return BlockDef{true, 5, 5, 5, /*transparent=*/false, /*collidable=*/true, /*breakable=*/false};
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
        case BlockType::Cloud:       return "Cloud";
        case BlockType::Leaves:      return "Leaves";
        case BlockType::TallGrass:   return "TallGrass";
        case BlockType::Bedrock:     return "Bedrock";
        default:                     return "Unknown";
    }
}

}
