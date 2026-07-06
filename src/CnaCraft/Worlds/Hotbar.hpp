#pragma once

#include <array>

#include "BlockType.hpp"

namespace CnaCraft::Worlds {

// Selected block type for placing (plan.md §11.4 "Hotbar"). Kept
// engine-agnostic like the rest of Worlds/ so slot selection/cycling is
// unit-testable without CNA (see tests/worlds_smoke_test.cpp) — CnaCraftGame
// only maps number keys / E to the methods below and reads Selected().
class Hotbar {
public:
    static constexpr std::array<BlockType, 4> kSlots = {
        BlockType::Grass, BlockType::Dirt, BlockType::Stone, BlockType::Sand};

    static constexpr int SlotCount() { return static_cast<int>(kSlots.size()); }

    // 1-based slot number (matches keyboard keys 1..SlotCount()); out-of-range
    // numbers are ignored so a stray key press can't corrupt selection.
    void SelectSlot(int oneBasedSlot);
    void CycleNext();

    BlockType Selected() const { return kSlots[static_cast<std::size_t>(selectedIndex_)]; }
    int SelectedIndex() const { return selectedIndex_; }

private:
    int selectedIndex_ = 0;
};

}
