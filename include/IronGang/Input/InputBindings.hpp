#pragma once

#include "Microsoft/Xna/Framework/Input/Keys.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace IronGang
{
    // plan_28 IG-28-007: the actions a player can rebind. Deliberately the primary gameplay set
    // the task asks for, not every key the game reads -- the debug and profiling keys stay fixed.
    enum class GameAction
    {
        MoveForward,
        MoveBack,
        StrafeLeft,
        StrafeRight,
        TurnLeft,
        TurnRight,
        Sprint,
        Interact,
        Horn,
        Handbrake,
        ToggleMap,
        Pause,
        QuickSave,
        QuickLoad,
        Restart,
        Confirm,
        Count,
    };

    // Which input context an action belongs to (see Gameplay/InputContext.hpp). This is what makes
    // conflict detection useful rather than obstructive: Space is the handbrake **while driving**
    // and confirms **in a menu**, and those two can share a key because the game is never
    // listening for both at once. Global actions conflict with everything, since they are read in
    // every context.
    enum class ActionGroup
    {
        Global,
        OnFoot,
        Vehicle,
        Menu,
    };

    struct ActionBinding
    {
        // Keys::None means "unbound", which is what a displaced action gets.
        Microsoft::Xna::Framework::Input::Keys primary{Microsoft::Xna::Framework::Input::Keys::None};
        Microsoft::Xna::Framework::Input::Keys secondary{Microsoft::Xna::Framework::Input::Keys::None};
    };

    // Stable identifiers used in the settings file, so renaming the enum never invalidates a
    // player's bindings.
    [[nodiscard]] const char* GameActionId(GameAction action) noexcept;
    [[nodiscard]] bool ParseGameActionId(const std::string& id, GameAction& out);
    [[nodiscard]] ActionGroup GameActionGroup(GameAction action) noexcept;
    // Player-facing name for a key, and its inverse. Covers the keys this game can bind; an
    // unsupported key has no name and cannot be stored, which is deliberate -- a settings file
    // that round-trips is worth more than one that can hold every key on a keyboard.
    [[nodiscard]] std::string KeyName(Microsoft::Xna::Framework::Input::Keys key);
    [[nodiscard]] bool ParseKeyName(const std::string& name, Microsoft::Xna::Framework::Input::Keys& out);

    class InputBindings final
    {
    public:
        // Starts at the keys the game shipped with.
        InputBindings();

        [[nodiscard]] const ActionBinding& Get(GameAction action) const;
        // Binds @p key as @p action's primary key. If another action **in a conflicting group**
        // already uses that key, that action loses it and is returned, so the caller can say so
        // rather than leaving the player with a key that quietly stopped working somewhere else.
        std::optional<GameAction> Rebind(GameAction action, Microsoft::Xna::Framework::Input::Keys key);
        // Which action would lose @p key if @p action took it, without changing anything.
        [[nodiscard]] std::optional<GameAction> FindConflict(
            GameAction action, Microsoft::Xna::Framework::Input::Keys key) const;
        void ResetToDefaults();
        // Applies a stored binding as-is, conflicts and all: a settings file is what the player
        // asked for, and refusing half of it on load would be worse than honouring a file they can
        // see and fix. Rebind() is the interactive path that reports conflicts.
        void Set(GameAction action, const ActionBinding& binding);

        // True when @p key is either binding for @p action.
        [[nodiscard]] bool Matches(GameAction action, Microsoft::Xna::Framework::Input::Keys key) const;

    private:
        std::vector<ActionBinding> bindings_;
    };
}
