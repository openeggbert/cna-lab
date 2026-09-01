#pragma once

#include "IronGang/Persistence/AutosavePolicy.hpp"

namespace IronGang
{
    // plan_28 IG-28-008: what the game is currently listening to. Before this, the answer was
    // spread across half a dozen `!dialogue_.IsActive() && !cutscene_.IsActive() && !transitioning`
    // conditions, each written out again at every site that needed it -- and each an opportunity
    // for one of them to disagree with the others.
    //
    // The order below is the precedence order, from the most overriding to the most ordinary:
    // being paused beats everything, a district load beats a cutscene (the world it played in is
    // being unloaded), and driving beats being on foot.
    enum class InputContext
    {
        Paused,
        DistrictTransition,
        Cutscene,
        Dialogue,
        // Getting into or out of the car: a short clip during which the player is neither on foot
        // nor driving, and controls belong to the animation rather than to them.
        VehicleTransition,
        Driving,
        OnFoot,
    };

    struct GameplaySignals
    {
        bool paused{false};
        bool districtTransitioning{false};
        bool cutsceneActive{false};
        bool dialogueActive{false};
        bool vehicleTransitionActive{false};
        bool driving{false};
    };

    [[nodiscard]] InputContext ResolveInputContext(const GameplaySignals& signals) noexcept;
    [[nodiscard]] const char* InputContextName(InputContext context) noexcept;

    // Whether the world advances at all. Only Paused stops it: a cutscene, a conversation, and a
    // district load all keep the simulation running, which is deliberate -- ambient traffic and
    // the police do not politely wait for a conversation to end.
    [[nodiscard]] bool ContextAdvancesWorld(InputContext context) noexcept;
    // Whether the player's own movement input is read.
    [[nodiscard]] bool ContextAllowsMovement(InputContext context) noexcept;
    // Whether "use" actions (entering the car, honking, mission interactions) are read.
    [[nodiscard]] bool ContextAllowsInteraction(InputContext context) noexcept;

    // Why saving is unsafe in this context, or None. Pausing is a **safe** moment -- the world is
    // frozen and consistent -- so the pause menu can offer a save. This replaces the separate
    // SaveConditions struct: the context already knows everything that answer needs.
    [[nodiscard]] SaveBlockReason SaveBlockReasonForContext(InputContext context) noexcept;
}
