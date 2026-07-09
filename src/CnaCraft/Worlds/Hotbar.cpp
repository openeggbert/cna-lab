#include "Hotbar.hpp"

namespace CnaCraft::Worlds {

void Hotbar::SelectSlot(int oneBasedSlot) {
    const int index = oneBasedSlot - 1;
    if (index < 0 || index >= SlotCount()) return;
    selectedIndex_ = index;
}

void Hotbar::CycleNext() {
    selectedIndex_ = (selectedIndex_ + 1) % SlotCount();
}

void Hotbar::CyclePrev() {
    selectedIndex_ = (selectedIndex_ - 1 + SlotCount()) % SlotCount();
}

}
