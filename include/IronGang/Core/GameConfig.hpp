#pragma once

#include "IronGang/Core/Log.hpp"
#include "IronGang/Persistence/AutosavePolicy.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_04 IG-04-001: the game's tunable values, loaded from assets/config/game.json.
    //
    // Every member's initializer is the default the game runs on when the file is absent, when a
    // value is missing, or when a value is unusable -- so a broken or partial config never stops
    // the game, it only costs the tuning. The autosave defaults deliberately come from
    // AutosaveScheduler's own constants rather than being repeated here, so the two cannot drift.
    //
    // This is developer tuning, not user settings: nothing here is written back, and the player
    // changes none of it from inside the game (user settings are plan_29 IG-29-005's own work).
    struct GameConfig
    {
        // Shown in the window title.
        std::string projectName{"Iron Shadows"};
        // Shown on the district map panel, with the year.
        std::string cityName{"Iron City"};
        int prototypeYear{1932};

        // Seconds of unblocked play between periodic autosaves; 0 disables them without
        // disabling checkpoint/district autosaves.
        float autosaveIntervalSeconds{AutosaveScheduler::kDefaultIntervalSeconds};
        // Shortest gap between two autosaves, so triggers landing together write one file.
        float autosaveMinimumSpacingSeconds{AutosaveScheduler::kDefaultMinimumSpacingSeconds};

        // plan_20 IG-20-003: how many of the nearest pedestrians are drawn as the skinned
        // character; the rest stay coloured boxes. Each skinned instance costs a bone-palette
        // upload and a draw, which a CPU rasterizer feels acutely -- twelve of them made this
        // project's software-backend profiling runs about ten times slower. 0 disables skinned
        // pedestrians entirely, which is what a software-backend profiling run wants.
        int maxSkinnedPedestrians{6};

        // Lowest severity that reaches the log: "debug", "info", "warning", or "error".
        // --log-level on the command line overrides this for a single run.
        LogSeverity logSeverity{LogSeverity::Info};
    };

    // Loads @p path into @p out, leaving every field the file does not usably specify at its
    // default.
    //
    // Returns false only when the file exists but cannot be understood at all (malformed JSON, or
    // a root that is not an object) -- the caller then keeps its defaults. A **missing file is not
    // a failure**: the game is fully playable without one, which is also why every per-field
    // problem is a warning rather than an error.
    //
    // @p warnings, when given, collects one line per problem: an unrecognized key (which catches
    // typos, the most common config mistake), a value of the wrong JSON type, a value outside its
    // usable range, and the combination of a minimum spacing longer than the interval. "notes" is
    // accepted and ignored, so the file can carry a comment.
    [[nodiscard]] bool LoadGameConfig(const std::string& path,
                                      GameConfig& out,
                                      std::string& errorMessage,
                                      std::vector<std::string>* warnings = nullptr);
}
