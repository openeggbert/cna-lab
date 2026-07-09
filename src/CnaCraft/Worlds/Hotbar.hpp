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
// E cycles through *all* slots, wrapping — so the remaining slots are still
// reachable. Bedrock is intentionally excluded (world-boundary block, not
// meant to be placed by the player, same as Craft never lists it in `items`).
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

    BlockType Selected() const { return kSlots[static_cast<std::size_t>(selectedIndex_)]; }
    int SelectedIndex() const { return selectedIndex_; }

private:
    int selectedIndex_ = 0;
};

}
