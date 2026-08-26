#include "IronGang/Input/InputBindings.hpp"

#include <array>

namespace IronGang
{
    using Microsoft::Xna::Framework::Input::Keys;

    namespace
    {
        struct ActionDescriptor
        {
            GameAction action;
            ActionGroup group;
            const char* id;
            Keys primary;
            Keys secondary;
        };

        // The single source of truth for what exists, where it belongs, and what it starts as.
        // Adding an action means adding one row here.
        constexpr std::array<ActionDescriptor, static_cast<std::size_t>(GameAction::Count)> kActions = {{
            {GameAction::MoveForward, ActionGroup::OnFoot, "move_forward", Keys::W, Keys::Up},
            {GameAction::MoveBack, ActionGroup::OnFoot, "move_back", Keys::S, Keys::Down},
            {GameAction::StrafeLeft, ActionGroup::OnFoot, "strafe_left", Keys::A, Keys::None},
            {GameAction::StrafeRight, ActionGroup::OnFoot, "strafe_right", Keys::D, Keys::None},
            {GameAction::TurnLeft, ActionGroup::OnFoot, "turn_left", Keys::Left, Keys::None},
            {GameAction::TurnRight, ActionGroup::OnFoot, "turn_right", Keys::Right, Keys::None},
            {GameAction::Sprint, ActionGroup::OnFoot, "sprint", Keys::LeftShift, Keys::RightShift},
            {GameAction::Interact, ActionGroup::Global, "interact", Keys::E, Keys::None},
            {GameAction::Horn, ActionGroup::Vehicle, "horn", Keys::H, Keys::None},
            {GameAction::Handbrake, ActionGroup::Vehicle, "handbrake", Keys::Space, Keys::None},
            {GameAction::ToggleMap, ActionGroup::Global, "toggle_map", Keys::Tab, Keys::None},
            {GameAction::Pause, ActionGroup::Global, "pause", Keys::Escape, Keys::None},
            {GameAction::QuickSave, ActionGroup::Global, "quick_save", Keys::F5, Keys::None},
            {GameAction::QuickLoad, ActionGroup::Global, "quick_load", Keys::F9, Keys::None},
            {GameAction::Restart, ActionGroup::Global, "restart", Keys::R, Keys::None},
            {GameAction::Confirm, ActionGroup::Menu, "confirm", Keys::Enter, Keys::Space},
        }};

        const ActionDescriptor& Descriptor(GameAction action)
        {
            return kActions[static_cast<std::size_t>(action)];
        }

        // Two groups conflict when the game could be listening for both at once: Global is read
        // everywhere, and every group conflicts with itself. OnFoot, Vehicle, and Menu are
        // mutually exclusive contexts, so they may share keys.
        bool GroupsConflict(ActionGroup a, ActionGroup b) noexcept
        {
            return a == b || a == ActionGroup::Global || b == ActionGroup::Global;
        }

        struct NamedKey
        {
            Keys key;
            const char* name;
        };

        constexpr std::array<NamedKey, 61> kNamedKeys = {{
            {Keys::A, "A"}, {Keys::B, "B"}, {Keys::C, "C"}, {Keys::D, "D"}, {Keys::E, "E"},
            {Keys::F, "F"}, {Keys::G, "G"}, {Keys::H, "H"}, {Keys::I, "I"}, {Keys::J, "J"},
            {Keys::K, "K"}, {Keys::L, "L"}, {Keys::M, "M"}, {Keys::N, "N"}, {Keys::O, "O"},
            {Keys::P, "P"}, {Keys::Q, "Q"}, {Keys::R, "R"}, {Keys::S, "S"}, {Keys::T, "T"},
            {Keys::U, "U"}, {Keys::V, "V"}, {Keys::W, "W"}, {Keys::X, "X"}, {Keys::Y, "Y"},
            {Keys::Z, "Z"},
            {Keys::D0, "0"}, {Keys::D1, "1"}, {Keys::D2, "2"}, {Keys::D3, "3"}, {Keys::D4, "4"},
            {Keys::D5, "5"}, {Keys::D6, "6"}, {Keys::D7, "7"}, {Keys::D8, "8"}, {Keys::D9, "9"},
            {Keys::F1, "F1"}, {Keys::F2, "F2"}, {Keys::F3, "F3"}, {Keys::F4, "F4"},
            {Keys::F5, "F5"}, {Keys::F6, "F6"}, {Keys::F7, "F7"}, {Keys::F8, "F8"},
            {Keys::F9, "F9"}, {Keys::F10, "F10"}, {Keys::F11, "F11"}, {Keys::F12, "F12"},
            {Keys::Up, "Up"}, {Keys::Down, "Down"}, {Keys::Left, "Left"}, {Keys::Right, "Right"},
            {Keys::Space, "Space"}, {Keys::Enter, "Enter"}, {Keys::Tab, "Tab"},
            {Keys::Escape, "Escape"}, {Keys::Back, "Backspace"},
            {Keys::LeftShift, "LeftShift"}, {Keys::RightShift, "RightShift"},
            {Keys::LeftControl, "LeftControl"}, {Keys::RightControl, "RightControl"},
        }};
    }

    const char* GameActionId(GameAction action) noexcept
    {
        if (action >= GameAction::Count)
        {
            return "";
        }
        return Descriptor(action).id;
    }

    bool ParseGameActionId(const std::string& id, GameAction& out)
    {
        for (const ActionDescriptor& descriptor : kActions)
        {
            if (id == descriptor.id)
            {
                out = descriptor.action;
                return true;
            }
        }
        return false;
    }

    ActionGroup GameActionGroup(GameAction action) noexcept
    {
        if (action >= GameAction::Count)
        {
            return ActionGroup::Global;
        }
        return Descriptor(action).group;
    }

    std::string KeyName(Keys key)
    {
        for (const NamedKey& named : kNamedKeys)
        {
            if (named.key == key)
            {
                return named.name;
            }
        }
        return {};
    }

    bool ParseKeyName(const std::string& name, Keys& out)
    {
        for (const NamedKey& named : kNamedKeys)
        {
            if (name == named.name)
            {
                out = named.key;
                return true;
            }
        }
        return false;
    }

    InputBindings::InputBindings()
    {
        ResetToDefaults();
    }

    void InputBindings::ResetToDefaults()
    {
        bindings_.assign(kActions.size(), ActionBinding{});
        for (const ActionDescriptor& descriptor : kActions)
        {
            ActionBinding& binding = bindings_[static_cast<std::size_t>(descriptor.action)];
            binding.primary = descriptor.primary;
            binding.secondary = descriptor.secondary;
        }
    }

    void InputBindings::Set(GameAction action, const ActionBinding& binding)
    {
        bindings_[static_cast<std::size_t>(action)] = binding;
    }

    const ActionBinding& InputBindings::Get(GameAction action) const
    {
        return bindings_[static_cast<std::size_t>(action)];
    }

    bool InputBindings::Matches(GameAction action, Keys key) const
    {
        if (key == Keys::None)
        {
            return false;
        }
        const ActionBinding& binding = Get(action);
        return binding.primary == key || binding.secondary == key;
    }

    std::optional<GameAction> InputBindings::FindConflict(GameAction action, Keys key) const
    {
        if (key == Keys::None)
        {
            return std::nullopt;
        }
        const ActionGroup group = GameActionGroup(action);
        for (const ActionDescriptor& descriptor : kActions)
        {
            if (descriptor.action == action || !GroupsConflict(group, descriptor.group))
            {
                continue;
            }
            if (Matches(descriptor.action, key))
            {
                return descriptor.action;
            }
        }
        return std::nullopt;
    }

    std::optional<GameAction> InputBindings::Rebind(GameAction action, Keys key)
    {
        const std::optional<GameAction> conflict = FindConflict(action, key);
        if (conflict.has_value())
        {
            // The displaced action loses **only** the key it lost, keeping its other binding: a
            // rebind should cost as little as it can.
            ActionBinding& displaced = bindings_[static_cast<std::size_t>(*conflict)];
            if (displaced.primary == key)
            {
                displaced.primary = displaced.secondary;
                displaced.secondary = Keys::None;
            }
            else if (displaced.secondary == key)
            {
                displaced.secondary = Keys::None;
            }
        }
        bindings_[static_cast<std::size_t>(action)].primary = key;
        return conflict;
    }
}
