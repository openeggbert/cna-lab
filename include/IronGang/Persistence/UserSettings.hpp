#pragma once

#include "IronGang/Input/InputBindings.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_29 IG-29-005: what the **player** changes, kept entirely apart from campaign save data.
    //
    // Two files, two lifetimes, two owners. A save is progress through the story and is written
    // when the player saves; this is preferences, is written the moment one changes, and must
    // survive deleting every save. `assets/config/game.json` is the third thing again -- developer
    // tuning, read-only, shipped with the game (see docs/configuration.md).
    inline constexpr int kUserSettingsVersion = 1;

    struct UserSettings
    {
        int version{kUserSettingsVersion};
        // 0 silences the game, 1 is full volume. Applied as a multiplier over every sound the
        // game plays, so it is one setting rather than a per-sound scatter.
        float masterVolume{1.0F};
        // The HUD can be turned off for a clean look; the pause menu stays visible regardless,
        // since a hidden menu is how a player gets stuck in a paused game.
        bool showHud{true};
        // plan_28 IG-28-007. Written out in full rather than as a diff against the defaults, so
        // the file shows a player every action and the key it is on -- which, until a rebinding
        // screen exists, is the only way to rebind at all.
        InputBindings bindings;
    };

    // Loads @p path. A missing file is **not** a failure -- the defaults above are what a player
    // who has never touched settings should get. Per-field problems (wrong type, out of range) are
    // warnings that keep the default; only malformed JSON, a non-object root, or an unsupported
    // version fails, and then @p out is untouched.
    [[nodiscard]] bool LoadUserSettings(const std::string& path,
                                        UserSettings& out,
                                        std::string& errorMessage,
                                        std::vector<std::string>* warnings = nullptr);

    // Writes atomically with a backup, so a crash while saving preferences cannot leave a file
    // that fails to parse next launch.
    [[nodiscard]] bool SaveUserSettings(const std::string& path,
                                        const UserSettings& settings,
                                        std::string& errorMessage);
}
