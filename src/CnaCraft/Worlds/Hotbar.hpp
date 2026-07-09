#pragma once

#include <array>

#include "BlockType.hpp"

namespace CnaCraft::Worlds {

// Selected block type for placing (plan.md §11.4 "Hotbar"). Kept
// engine-agnostic like the rest of Worlds/ so slot selection/cycling is
// unit-testable without CNA (see tests/worlds_smoke_test.cpp) — CnaCraftGame
// only maps number keys / E to the methods below and reads Selected().
//
// Mirrors Craft's own item_index behavior (src/main.c): keys 1-9 jump
// directly to the first 9 slots (CnaCraftGame caps the direct-key mapping at
// kMaxNumberKeySlots since there's no numeric key for slots beyond 9), while
// E/R cycle through *all* slots forward/backward, wrapping — so the
// remaining slots are still reachable (CRAFT_PARITY.md §2.1: Craft's own
// `on_key` binds `E`=next, `R`=prev — `CyclePrev` was missing until this
// session, only `CycleNext`/E existed). Bedrock is intentionally excluded
// (world-boundary block, not meant to be placed by the player, same as
// Craft never lists it in `items`).
class Hotbar {
public:
    static constexpr std::array<BlockType, 15> kSlots = {
        BlockType::Grass,      BlockType::Dirt,     BlockType::Sand,
        BlockType::Stone,      BlockType::Cobblestone, BlockType::Brick,
        BlockType::Plank,      BlockType::Wood,     BlockType::Cement,
        BlockType::LightStone, BlockType::DarkStone, BlockType::Snow,
        BlockType::Glass,      BlockType::Cloud,    BlockType::Leaves};

    static constexpr int kMaxNumberKeySlots = 9;

    static constexpr int SlotCount() { return static_cast<int>(kSlots.size()); }

    // 1-based slot number (matches keyboard keys 1..min(9, SlotCount())); out
    // of range numbers are ignored so a stray key press can't corrupt
    // selection.
    void SelectSlot(int oneBasedSlot);
    void CycleNext();
    void CyclePrev();

    // Middle-click "eyedropper" (CRAFT_PARITY.md §2.7): selects the slot
    // holding `type`, if any. Ports Craft's `on_middle_click`, which
    // linear-scans `items[]` for the targeted block's type and selects that
    // slot if found, leaving selection unchanged otherwise (e.g. the
    // targeted block isn't in the placeable roster at all — Bedrock, Air).
    // Returns true if a matching slot was found and selected.
    bool SelectByBlockType(BlockType type);

    BlockType Selected() const { return kSlots[static_cast<std::size_t>(selectedIndex_)]; }
    int SelectedIndex() const { return selectedIndex_; }

private:
    int selectedIndex_ = 0;
};

}
