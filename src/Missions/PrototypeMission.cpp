#include "IronShadows/Missions/PrototypeMission.hpp"

namespace IronShadows
{
    namespace
    {
        MissionDefinition BuildFallbackMissionDefinition()
        {
            MissionDefinition definition;
            definition.id = "prototype_delivery";
            definition.version = 1;
            definition.initialState = "introduction";
            definition.states = {
                {"introduction", "Listen to Mara (Enter advances dialogue)",
                 MissionCondition::DialogueFinished, "reach_vehicle"},
                {"reach_vehicle", "Walk to the sedan",
                 MissionCondition::PlayerNearVehicle, "enter_vehicle"},
                {"enter_vehicle", "Press E to enter the sedan",
                 MissionCondition::PlayerDriving, "drive_to_warehouse"},
                {"drive_to_warehouse", "Drive into the green warehouse marker",
                 MissionCondition::PlayerDrivingInWarehouseGoal, "completed"},
                {"completed", "Prototype mission complete", MissionCondition::None, ""},
            };
            return definition;
        }

        // Bidirectional mapping between PrototypeMissionState (kept for SaveGame's int-based
        // mission_state field, see PrototypeMissionState's own header comment) and the state ids
        // a mission JSON file uses. Fixed to exactly these 5 names -- LoadMission() rejects any
        // mission file using a different state id.
        const char* EnumToStateId(PrototypeMissionState state)
        {
            switch (state)
            {
                case PrototypeMissionState::Introduction: return "introduction";
                case PrototypeMissionState::ReachVehicle: return "reach_vehicle";
                case PrototypeMissionState::EnterVehicle: return "enter_vehicle";
                case PrototypeMissionState::DriveToWarehouse: return "drive_to_warehouse";
                case PrototypeMissionState::Completed: return "completed";
            }
            return "introduction";
        }

        bool StateIdToEnum(const std::string& id, PrototypeMissionState& out)
        {
            if (id == "introduction") { out = PrototypeMissionState::Introduction; return true; }
            if (id == "reach_vehicle") { out = PrototypeMissionState::ReachVehicle; return true; }
            if (id == "enter_vehicle") { out = PrototypeMissionState::EnterVehicle; return true; }
            if (id == "drive_to_warehouse") { out = PrototypeMissionState::DriveToWarehouse; return true; }
            if (id == "completed") { out = PrototypeMissionState::Completed; return true; }
            return false;
        }
    }

    PrototypeMission::PrototypeMission() : definition_(BuildFallbackMissionDefinition()) {}

    bool PrototypeMission::LoadMission(const std::string& path, std::string& errorMessage)
    {
        MissionDefinition loaded;
        if (!LoadMissionDefinition(path, loaded, errorMessage))
        {
            return false;
        }

        // This prototype's save format is the fixed int enum above, not a free-form string --
        // reject a mission file that introduces a state id outside that fixed set rather than
        // silently losing save compatibility (see PrototypeMissionState's own header comment).
        for (const MissionStateDefinition& state : loaded.states)
        {
            PrototypeMissionState ignored;
            if (!StateIdToEnum(state.id, ignored))
            {
                errorMessage = "Mission state id \"" + state.id +
                               "\" is not one of the ids PrototypeMission's save format supports "
                               "(introduction/reach_vehicle/enter_vehicle/drive_to_warehouse/completed): " +
                               path;
                return false;
            }
        }

        definition_ = std::move(loaded);
        return true;
    }

    void PrototypeMission::Reset()
    {
        if (!StateIdToEnum(definition_.initialState, state_))
        {
            state_ = PrototypeMissionState::Introduction; // unreachable post-LoadMission validation
        }
    }

    void PrototypeMission::Update(bool dialogueFinished,
                                  const Vector3& playerPosition,
                                  const Vector3& vehiclePosition,
                                  bool playerDriving,
                                  const TriggerZone& warehouseGoal)
    {
        const MissionStateDefinition* current = definition_.FindState(EnumToStateId(state_));
        if (current == nullptr || current->condition == MissionCondition::None || current->next.empty())
        {
            return;
        }

        bool conditionMet = false;
        switch (current->condition)
        {
            case MissionCondition::DialogueFinished:
                conditionMet = dialogueFinished;
                break;
            case MissionCondition::PlayerNearVehicle:
                conditionMet = DistanceSquaredXZ(playerPosition, vehiclePosition) <= 9.0F;
                break;
            case MissionCondition::PlayerDriving:
                conditionMet = playerDriving;
                break;
            case MissionCondition::PlayerDrivingInWarehouseGoal:
                conditionMet = playerDriving && warehouseGoal.bounds.ContainsXZ(vehiclePosition);
                break;
            case MissionCondition::None:
                break;
        }

        if (conditionMet)
        {
            PrototypeMissionState nextState;
            if (StateIdToEnum(current->next, nextState))
            {
                state_ = nextState;
            }
        }
    }

    std::string PrototypeMission::GetObjectiveText() const
    {
        const MissionStateDefinition* current = definition_.FindState(EnumToStateId(state_));
        return current != nullptr ? current->objective : "Unknown objective";
    }
}
