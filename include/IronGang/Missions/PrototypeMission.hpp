#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // The engine facts this prototype exposes to mission expressions (plan_24 IG-24-006), each at
    // a neutral initial value:
    //     dialogue_finished                 bool   the opening dialogue has been read to the end
    //     player_driving                    bool   the player is currently driving the sedan
    //     player_vehicle_distance           float  XZ distance from the player to the sedan, metres
    //     player_near_vehicle               bool   player_vehicle_distance <= 3
    //     player_in_warehouse_goal          bool   the player stands inside the delivery trigger
    //     vehicle_in_warehouse_goal         bool   the sedan is inside the delivery trigger
    //     player_driving_in_warehouse_goal  bool   both of the last two at once
    //     police_alerted                    bool   the police have been dispatched or are chasing
    //     police_chasing                    bool   a patrol car is actively chasing the player
    //     police_chase_seconds              float  how long the current chase has run, seconds
    //     vehicle_integrity                 float  1 for an undamaged sedan, 0 for a wreck
    //     vehicle_disabled                  bool   the sedan is wrecked (integrity 0)
    //     current_district                  string "warehouse_block" or "countryside"
    //     player_in_delivery_goal           bool   the player stands in this district's delivery zone
    //     vehicle_in_delivery_goal          bool   the sedan is in this district's delivery zone
    //
    // The ..._delivery_goal pair is the district-neutral spelling of the two ..._warehouse_goal
    // facts, which are kept because gate M7's mission file uses them. New missions should use the
    // neutral names: the countryside's delivery zone is a farmhouse yard, not a warehouse.
    //
    // The police and vehicle-damage facts are pushed by the game through SetFact() rather than
    // derived from Update()'s arguments, because PoliceSystem and VehicleController are the
    // game's, not the mission's.
    //
    // The three composite facts (player_near_vehicle and the two ..._in_warehouse_goal spellings
    // gate M7 shipped) are kept so every version-1 mission file still loads unchanged; new
    // missions should prefer the primitives, e.g. "player_driving && vehicle_in_warehouse_goal".
    [[nodiscard]] MissionContext CreatePrototypeMissionFacts();

    // plan_24 IG-24-010/044: everything a retry needs to put a mission back at its last
    // checkpoint -- which state it was, and what the mission's variables held once that state had
    // been entered and its entry actions had run. An empty stateId means no checkpoint has been
    // reached yet, which is also how a save from a mission with no checkpoints round-trips.
    struct MissionCheckpointSnapshot
    {
        std::string stateId;
        std::vector<MissionVariableSnapshot> variables;
    };

    // Gate M7: a data-driven mission runtime for the prototype's one delivery mission. Ships with
    // a hardcoded default (constructor) reproducing the original hand-coded flow exactly, so
    // existing callers/tests that never call LoadMission() keep working unchanged; LoadMission()
    // replaces that default with a real mission file (assets/missions/prologue.mission.json) when
    // available, falling back to the hardcoded default (matching
    // DialogueSystem::LoadFallbackPrologue()'s convention) on any load/validation failure.
    class PrototypeMission final
    {
    public:
        PrototypeMission();

        // Loads and validates a mission definition from path (see LoadMissionDefinition). On
        // success, replaces the current definition (the hardcoded default, or a previously loaded
        // one) and its variables; on failure, errorMessage is set and the current definition is
        // left unchanged. Call Reset() afterwards to start the loaded mission.
        [[nodiscard]] bool LoadMission(const std::string& path, std::string& errorMessage);

        // Returns to the initial state, restores every mission variable to its declared value,
        // discards any recorded checkpoint, and runs the initial state's entry actions -- i.e. a
        // restart from the very beginning.
        void Reset();

        // Retry after a failure (IG-24-009/042). Under the mission's default `checkpoint` policy
        // this returns to the last checkpoint state with the variables it recorded, without
        // re-running that state's entry actions -- their effects are already part of what was
        // recorded. Under `mission_start`, or before any checkpoint has been reached, it is
        // exactly Reset().
        void Retry();
        [[nodiscard]] MissionRetryPolicy GetRetryPolicy() const noexcept { return definition_.retryPolicy; }
        // districtId names the district the goal belongs to, so a mission can require being in one
        // (the `current_district` fact). Empty keeps whatever was last set.
        void Update(bool dialogueFinished,
                    const Vector3& playerPosition,
                    const Vector3& vehiclePosition,
                    bool playerDriving,
                    const TriggerZone& deliveryGoal,
                    const std::string& districtId = {});

        // The current state's id. This is the authoritative state -- there is no fixed enum of
        // allowed states any more, so a mission file may name its states whatever it likes
        // (plan_24 IG-24-018 lifted the five-id restriction the int-based save format imposed).
        [[nodiscard]] const std::string& GetStateId() const noexcept { return stateId_; }
        [[nodiscard]] bool IsInState(const std::string& stateId) const { return stateId_ == stateId; }
        [[nodiscard]] std::string GetObjectiveText() const;

        // What reaching the current state means for the run (IG-24-002/009). A mission is over
        // when GetOutcome() is not None; which of the two it is decides success from failure.
        [[nodiscard]] MissionOutcome GetOutcome() const;
        [[nodiscard]] bool IsCompleted() const { return GetOutcome() == MissionOutcome::Completed; }
        [[nodiscard]] bool IsFailed() const { return GetOutcome() == MissionOutcome::Failed; }
        [[nodiscard]] bool IsFinished() const { return GetOutcome() != MissionOutcome::None; }
        // The failing state's own explanation of what went wrong, or empty when the mission has
        // not failed. Mission-authored text, shown to the player as-is.
        [[nodiscard]] std::string GetFailureReason() const;

        // Restores a state without re-running its entry actions: loading a save resumes a mission
        // that already ran them, and running them again would double every counter they touch.
        // Returns false, leaving the mission untouched, for an id the loaded mission does not
        // define -- a save written against a different mission file must not strand the mission in
        // a state that has no objective, condition, or way out (IG-24-019).
        [[nodiscard]] bool SetStateId(const std::string& stateId);

        // Mission variables (IG-24-005/029/039). CaptureVariables() is what SaveGame writes;
        // ApplyVariables() restores it, ignoring -- and reporting through @p warnings, when given
        // -- any name this mission no longer declares or whose type changed, so an older save
        // still loads against an edited mission file (IG-24-019).
        [[nodiscard]] std::vector<MissionVariableSnapshot> CaptureVariables() const;
        void ApplyVariables(const std::vector<MissionVariableSnapshot>& variables,
                            std::vector<std::string>* warnings = nullptr);
        // Pushes a value into one of the declared engine facts. This is how a caller supplies a
        // fact the mission runtime cannot derive from Update()'s own arguments -- the police facts
        // above, and whatever later subsystems declare. Fails for an unknown fact, for a mission
        // variable (those are the mission's to write), or for a value of the wrong type.
        [[nodiscard]] bool SetFact(const std::string& name, const MissionValue& value,
                                   std::string& errorMessage);
        [[nodiscard]] bool TryGetVariable(const std::string& name, MissionValue& out) const;

        // Checkpoint state (IG-24-044). GetCheckpoint() is what SaveGame writes; ApplyCheckpoint()
        // restores it, dropping -- and reporting through @p warnings, when given -- a state id the
        // loaded mission no longer defines or a variable that no longer matches, exactly as
        // ApplyVariables() does.
        [[nodiscard]] bool HasCheckpoint() const noexcept { return !checkpoint_.stateId.empty(); }
        [[nodiscard]] const MissionCheckpointSnapshot& GetCheckpoint() const noexcept { return checkpoint_; }
        void ApplyCheckpoint(const MissionCheckpointSnapshot& checkpoint,
                             std::vector<std::string>* warnings = nullptr);
        [[nodiscard]] const MissionContext& GetContext() const noexcept { return context_; }
        [[nodiscard]] const MissionDefinition& GetDefinition() const noexcept { return definition_; }

    private:
        void EnterState(const std::string& stateId);
        void RefreshFacts(bool dialogueFinished,
                          const Vector3& playerPosition,
                          const Vector3& vehiclePosition,
                          bool playerDriving,
                          const TriggerZone& deliveryGoal,
                          const std::string& districtId);

        MissionDefinition definition_;
        // Set once a state's condition has already reported a runtime failure, so a fault that
        // repeats every frame (a divide by zero in a condition, say) is logged once per state
        // entry instead of sixty times a second. Cleared on every state change.
        bool conditionFaultLogged_{false};
        // Live symbol values for this run: a copy of definition_.declaredContext whose facts are
        // refreshed every Update() and whose variables the mission's own actions write.
        MissionContext context_;
        std::string stateId_;
        MissionCheckpointSnapshot checkpoint_;
    };
}
