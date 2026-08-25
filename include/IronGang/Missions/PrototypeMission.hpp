#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // Kept as a fixed enum (rather than a free-form string) purely for SaveGame's existing
    // int-based mission_state field and public-API compatibility (gate M7,
    // plan_24-mission-framework-and-scripting.md IG-24-001 -- "preserve current behavior"). The
    // states themselves, their objective text, transition conditions, entry actions, and the
    // state graph are data (see MissionDefinition) -- this enum only names the fixed set of state
    // ids a mission file loaded by PrototypeMission is allowed to use; see LoadMission.
    enum class PrototypeMissionState : int
    {
        Introduction = 0,
        ReachVehicle = 1,
        EnterVehicle = 2,
        DriveToWarehouse = 3,
        Completed = 4
    };

    // The engine facts this prototype exposes to mission expressions (plan_24 IG-24-006), each at
    // a neutral initial value:
    //     dialogue_finished                 bool   the opening dialogue has been read to the end
    //     player_driving                    bool   the player is currently driving the sedan
    //     player_vehicle_distance           float  XZ distance from the player to the sedan, metres
    //     player_near_vehicle               bool   player_vehicle_distance <= 3
    //     player_in_warehouse_goal          bool   the player stands inside the delivery trigger
    //     vehicle_in_warehouse_goal         bool   the sedan is inside the delivery trigger
    //     player_driving_in_warehouse_goal  bool   both of the last two at once
    //
    // The three composite facts (player_near_vehicle and the two ..._in_warehouse_goal spellings
    // gate M7 shipped) are kept so every version-1 mission file still loads unchanged; new
    // missions should prefer the primitives, e.g. "player_driving && vehicle_in_warehouse_goal".
    [[nodiscard]] MissionContext CreatePrototypeMissionFacts();

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

        // Loads and validates a mission definition from path (see LoadMissionDefinition), and
        // additionally requires every state id in it to be one of PrototypeMissionState's fixed
        // values (this prototype's save format is int-enum-based, see SaveGame). On success,
        // replaces the current definition (the hardcoded default, or a previously loaded one) and
        // its variables; on failure, errorMessage is set and the current definition is left
        // unchanged. Call Reset() afterwards to start the loaded mission.
        [[nodiscard]] bool LoadMission(const std::string& path, std::string& errorMessage);

        // Returns to the initial state, restores every mission variable to its declared value, and
        // runs the initial state's entry actions -- i.e. a retry from the beginning (IG-24-009).
        void Reset();
        void Update(bool dialogueFinished,
                    const Vector3& playerPosition,
                    const Vector3& vehiclePosition,
                    bool playerDriving,
                    const TriggerZone& warehouseGoal);

        [[nodiscard]] PrototypeMissionState GetState() const noexcept { return state_; }
        [[nodiscard]] std::string GetObjectiveText() const;
        [[nodiscard]] bool IsCompleted() const noexcept { return state_ == PrototypeMissionState::Completed; }
        // Restores a state without re-running its entry actions: loading a save resumes a mission
        // that already ran them, and running them again would double every counter they touch.
        void SetState(PrototypeMissionState state) noexcept
        {
            state_ = state;
            conditionFaultLogged_ = false;
        }

        // Mission variables (IG-24-005/029/039). CaptureVariables() is what SaveGame writes;
        // ApplyVariables() restores it, ignoring -- and reporting through @p warnings, when given
        // -- any name this mission no longer declares or whose type changed, so an older save
        // still loads against an edited mission file (IG-24-019).
        [[nodiscard]] std::vector<MissionVariableSnapshot> CaptureVariables() const;
        void ApplyVariables(const std::vector<MissionVariableSnapshot>& variables,
                            std::vector<std::string>* warnings = nullptr);
        [[nodiscard]] bool TryGetVariable(const std::string& name, MissionValue& out) const;
        [[nodiscard]] const MissionContext& GetContext() const noexcept { return context_; }
        [[nodiscard]] const MissionDefinition& GetDefinition() const noexcept { return definition_; }

    private:
        void EnterState(PrototypeMissionState state);
        void RefreshFacts(bool dialogueFinished,
                          const Vector3& playerPosition,
                          const Vector3& vehiclePosition,
                          bool playerDriving,
                          const TriggerZone& warehouseGoal);

        MissionDefinition definition_;
        // Set once a state's condition has already reported a runtime failure, so a fault that
        // repeats every frame (a divide by zero in a condition, say) is logged once per state
        // entry instead of sixty times a second. Cleared on every state change.
        bool conditionFaultLogged_{false};
        // Live symbol values for this run: a copy of definition_.declaredContext whose facts are
        // refreshed every Update() and whose variables the mission's own actions write.
        MissionContext context_;
        PrototypeMissionState state_{PrototypeMissionState::Introduction};
    };
}
