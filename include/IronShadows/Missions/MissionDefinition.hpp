#pragma once

#include <string>
#include <vector>

namespace IronShadows
{
    // Gate M7 (plan_24-mission-framework-and-scripting.md IS-24-001/002): the small, fixed set of
    // signals a mission state's transition can depend on. Deliberately not a general expression
    // evaluator (IS-24-013 is explicit, separate, later work) -- every condition here is engine
    // logic evaluated by name against the same signals PrototypeMission::Update() always received.
    enum class MissionCondition
    {
        None, // terminal state: no automatic transition
        DialogueFinished,
        PlayerNearVehicle,
        PlayerDriving,
        PlayerDrivingInWarehouseGoal,
    };

    // Parses a mission JSON file's "condition" string (e.g. "dialogue_finished") into the
    // matching MissionCondition. Returns false (and does not modify @p out) for an unrecognized
    // name, so callers can report a validation error rather than silently misinterpreting it.
    [[nodiscard]] bool ParseMissionCondition(const std::string& name, MissionCondition& out);

    // One state in a mission's linear graph: what to show the player, what to wait for, and
    // where to go next.
    struct MissionStateDefinition
    {
        std::string id;
        std::string objective;
        MissionCondition condition{MissionCondition::None};
        // Next state's id, or empty for a terminal state (condition must be None when empty).
        std::string next;
    };

    // A whole mission: a named, versioned list of states plus which one to start in. Loaded from
    // a hand-written JSON file (assets/missions/*.mission.json) -- see LoadMissionDefinition.
    struct MissionDefinition
    {
        std::string id;
        int version{1};
        std::string initialState;
        std::vector<MissionStateDefinition> states;

        // Returns the state with the given id, or nullptr if none matches.
        [[nodiscard]] const MissionStateDefinition* FindState(const std::string& stateId) const;
    };

    // Parses and validates a mission definition from @p path: every state has a unique
    // non-empty id, initialState refers to an existing state, every non-empty `next` refers to
    // an existing state, and every condition string is a recognized MissionCondition. Returns
    // false with errorMessage set on any parse or validation failure -- callers should fall back
    // to a hardcoded default (see PrototypeMission's own fallback) rather than run with a
    // partially-invalid mission.
    [[nodiscard]] bool LoadMissionDefinition(const std::string& path,
                                             MissionDefinition& out,
                                             std::string& errorMessage);
}
