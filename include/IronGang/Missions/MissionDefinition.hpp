#pragma once

#include "IronGang/Missions/MissionContext.hpp"
#include "IronGang/Missions/MissionExpression.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // The mission-file schema versions this loader understands (IG-24-018).
    //   1 -- gate M7's original shape: states with a "condition" naming one fixed engine signal.
    //   2 -- adds typed "variables", expression conditions ("when"), and "onEnter" actions.
    // Version 1 files keep loading unchanged, because every condition name gate M7 accepted is
    // still a declared bool fact and therefore a valid version-2 expression on its own.
    inline constexpr int kMinMissionFileVersion = 1;
    inline constexpr int kMaxMissionFileVersion = 2;

    // Upper bound on entry actions per state (IG-24-014, same reasoning as kMaxMissionVariables).
    inline constexpr std::size_t kMaxMissionStateActions = 16;

    // plan_24 IG-24-007: one thing a mission does when it enters a state. Deliberately a small
    // closed set of engine-executed actions, not a script:
    //   Set -- assign an expression's value to one of this mission's declared variables.
    //   Log -- write a fixed message through the game's existing logging path (IG-24-016).
    // The remaining IG-24-007 verbs (spawn/despawn/enable/disable/move/play/wait/branch) need
    // entity and timer concepts this prototype does not have yet and are still open.
    struct MissionAction
    {
        enum class Kind
        {
            Set,
            Log,
        };

        Kind kind{Kind::Log};
        std::string variable;    // Set: the target variable's name
        MissionExpression value; // Set: what to assign; its type must match the variable's
        std::string message;     // Log: the text to write
    };

    // plan_24 IG-24-002: what reaching a state means for the mission as a whole. A state with an
    // outcome other than None is terminal -- it ends the run, successfully or not -- which is why
    // the loader requires such a state to declare no condition and no "next".
    enum class MissionOutcome
    {
        None,
        Completed,
        Failed,
    };

    [[nodiscard]] const char* MissionOutcomeName(MissionOutcome outcome) noexcept;
    [[nodiscard]] bool ParseMissionOutcome(const std::string& name, MissionOutcome& out);

    // plan_24 IG-24-009: where a retry puts the player after a failure. `Checkpoint` falls back to
    // the mission start until a checkpoint has actually been reached, so a mission that declares no
    // checkpoint behaves identically under either policy.
    enum class MissionRetryPolicy
    {
        Checkpoint,
        MissionStart,
    };

    [[nodiscard]] const char* MissionRetryPolicyName(MissionRetryPolicy policy) noexcept;
    [[nodiscard]] bool ParseMissionRetryPolicy(const std::string& name, MissionRetryPolicy& out);

    // One state in a mission's graph: what to show the player, what to do on arrival, what to
    // wait for, and where to go next.
    struct MissionStateDefinition
    {
        std::string id;
        std::string objective;
        MissionOutcome outcome{MissionOutcome::None};
        // Shown to the player when this state ends the mission in failure; load-rejected on any
        // other state, where nothing would ever read it.
        std::string reason;
        // plan_24 IG-24-010: entering this state records a checkpoint (its id plus the mission's
        // variables as they stand once its entry actions have run) for a later retry to return to.
        bool checkpoint{false};
        // Bool expression evaluated every frame while this state is current. Empty means a
        // terminal state with no automatic transition.
        MissionExpression condition;
        // Next state's id, or empty for a terminal state.
        std::string next;
        std::vector<MissionAction> onEnter;
    };

    // A whole mission: a named, versioned state graph, the typed variables it owns, and the
    // symbol table (engine facts plus those variables) its expressions were compiled against.
    struct MissionDefinition
    {
        std::string id;
        std::string title;
        int version{kMaxMissionFileVersion};
        std::string initialState;
        MissionRetryPolicy retryPolicy{MissionRetryPolicy::Checkpoint};
        std::vector<MissionStateDefinition> states;
        // Facts supplied by the caller plus this file's own variables, each at its declared
        // initial value. PrototypeMission copies this to build its live, per-run context.
        MissionContext declaredContext;

        // Returns the state with the given id, or nullptr if none matches.
        [[nodiscard]] const MissionStateDefinition* FindState(const std::string& stateId) const;
        // The outcome of the named state, or None for an unknown one.
        [[nodiscard]] MissionOutcome GetOutcome(const std::string& stateId) const;
    };

    // Parses and validates a mission definition from @p path.
    //
    // @p factSchema supplies the engine facts (already declared, at any values) that this
    // mission's expressions may read; the loader copies it, adds the file's own variables, and
    // compiles every condition/action expression against the result -- so an expression naming
    // something the game does not provide fails at load time, not mid-mission.
    //
    // Validation: supported schema version; unique non-empty state ids; initialState and every
    // non-empty "next" refer to a real state; each variable has a known type and a value of that
    // type; each condition is a bool expression; a terminal state (empty "next") declares no
    // condition; an outcome state is terminal; at least one state ends the mission; a failure
    // reason belongs only to a failing state; a checkpoint state is not itself an outcome; each
    // action is well formed and assigns a declared variable a matching type.
    // Returns false with errorMessage set on any failure -- callers should fall back to a known
    // good default (see PrototypeMission) rather than run with a partially-valid mission.
    [[nodiscard]] bool LoadMissionDefinition(const std::string& path,
                                             const MissionContext& factSchema,
                                             MissionDefinition& out,
                                             std::string& errorMessage);
}
