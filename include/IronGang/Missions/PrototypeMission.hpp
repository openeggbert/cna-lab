#pragma once

#include "IronGang/Core/WorldTypes.hpp"
#include "IronGang/Missions/MissionDefinition.hpp"

#include <string>

namespace IronGang
{
    // Kept as a fixed enum (rather than a free-form string) purely for SaveGame's existing
    // int-based mission_state field and public-API compatibility (gate M7,
    // plan_24-mission-framework-and-scripting.md IG-24-001 -- "preserve current behavior"). The
    // states themselves, their objective text, transition conditions, and the state graph are
    // now data (see MissionDefinition) -- this enum only names the fixed set of state ids a
    // mission file loaded by PrototypeMission is allowed to use; see PrototypeMission::LoadMission.
    enum class PrototypeMissionState : int
    {
        Introduction = 0,
        ReachVehicle = 1,
        EnterVehicle = 2,
        DriveToWarehouse = 3,
        Completed = 4
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

        // Loads and validates a mission definition from path (see LoadMissionDefinition), and
        // additionally requires every state id in it to be one of PrototypeMissionState's fixed
        // values (this prototype's save format is int-enum-based, see SaveGame). On success,
        // replaces the current definition (the hardcoded default, or a previously loaded one);
        // on failure, errorMessage is set and the current definition is left unchanged.
        [[nodiscard]] bool LoadMission(const std::string& path, std::string& errorMessage);

        void Reset();
        void Update(bool dialogueFinished,
                    const Vector3& playerPosition,
                    const Vector3& vehiclePosition,
                    bool playerDriving,
                    const TriggerZone& warehouseGoal);

        [[nodiscard]] PrototypeMissionState GetState() const noexcept { return state_; }
        [[nodiscard]] std::string GetObjectiveText() const;
        [[nodiscard]] bool IsCompleted() const noexcept { return state_ == PrototypeMissionState::Completed; }
        void SetState(PrototypeMissionState state) noexcept { state_ = state; }

    private:
        MissionDefinition definition_;
        PrototypeMissionState state_{PrototypeMissionState::Introduction};
    };
}
