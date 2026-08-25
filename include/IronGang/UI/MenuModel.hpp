#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace IronGang
{
    // plan_28 IG-28-003/004: what a menu *is*, with no drawing and no input handling in it -- so
    // the rules that are easy to get subtly wrong (skipping disabled entries, wrapping at both
    // ends, a menu where nothing is selectable) are testable without a window.
    enum class MenuAction
    {
        None,
        Resume,
        Save,
        Load,
        RestartMission,
        Quit,
    };

    struct MenuItem
    {
        MenuAction action{MenuAction::None};
        std::string label;
        // A disabled item stays visible and keeps its place: hiding it would move everything else
        // under the player's fingers between two frames.
        bool enabled{true};
        // Shown beside a disabled item, so "why can't I press this?" is answered on the screen
        // rather than in a manual.
        std::string disabledReason;
    };

    class MenuModel final
    {
    public:
        // Replaces the items and selects the first enabled one, so a menu is never opened with an
        // unusable entry highlighted.
        void SetItems(std::vector<MenuItem> items);
        [[nodiscard]] const std::vector<MenuItem>& GetItems() const noexcept { return items_; }
        [[nodiscard]] std::size_t GetSelectedIndex() const noexcept { return selected_; }
        [[nodiscard]] const MenuItem* GetSelected() const noexcept;

        // Moves by @p delta entries, **skipping disabled ones** and wrapping at both ends. A menu
        // with nothing enabled leaves the selection where it is rather than looping forever.
        void MoveSelection(int delta) noexcept;

        // The selected item's action, or None when it is disabled or the menu is empty. Returning
        // None rather than the action is what stops a disabled entry from doing its thing anyway.
        [[nodiscard]] MenuAction Activate() const noexcept;

    private:
        [[nodiscard]] bool HasEnabledItem() const noexcept;

        std::vector<MenuItem> items_;
        std::size_t selected_{0};
    };
}
