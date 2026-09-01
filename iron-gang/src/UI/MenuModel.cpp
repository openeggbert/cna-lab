#include "IronGang/UI/MenuModel.hpp"

namespace IronGang
{
    void MenuModel::SetItems(std::vector<MenuItem> items)
    {
        items_ = std::move(items);
        selected_ = 0;
        for (std::size_t index = 0; index < items_.size(); ++index)
        {
            if (items_[index].enabled)
            {
                selected_ = index;
                return;
            }
        }
    }

    const MenuItem* MenuModel::GetSelected() const noexcept
    {
        if (selected_ >= items_.size())
        {
            return nullptr;
        }
        return &items_[selected_];
    }

    bool MenuModel::HasEnabledItem() const noexcept
    {
        for (const MenuItem& item : items_)
        {
            if (item.enabled)
            {
                return true;
            }
        }
        return false;
    }

    void MenuModel::MoveSelection(int delta) noexcept
    {
        if (items_.empty() || delta == 0 || !HasEnabledItem())
        {
            // Nothing selectable: leave the selection alone rather than looping forever looking
            // for an enabled entry that does not exist.
            return;
        }

        const int count = static_cast<int>(items_.size());
        const int step = delta > 0 ? 1 : -1;
        int remaining = delta > 0 ? delta : -delta;
        int index = static_cast<int>(selected_);
        while (remaining > 0)
        {
            // At most `count` hops to find the next enabled entry; HasEnabledItem() above
            // guarantees one exists, so this cannot spin.
            for (int hop = 0; hop < count; ++hop)
            {
                index = (index + step + count) % count;
                if (items_[static_cast<std::size_t>(index)].enabled)
                {
                    break;
                }
            }
            --remaining;
        }
        selected_ = static_cast<std::size_t>(index);
    }

    MenuAction MenuModel::Activate() const noexcept
    {
        const MenuItem* item = GetSelected();
        if (item == nullptr || !item->enabled)
        {
            return MenuAction::None;
        }
        return item->action;
    }
}
