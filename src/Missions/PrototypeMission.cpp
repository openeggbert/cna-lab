#include "IronShadows/Missions/PrototypeMission.hpp"

namespace IronShadows
{
    void PrototypeMission::Reset()
    {
        state_ = PrototypeMissionState::Introduction;
    }

    void PrototypeMission::Update(bool dialogueFinished,
                                  const Vector3& playerPosition,
                                  const Vector3& vehiclePosition,
                                  bool playerDriving,
                                  const TriggerZone& warehouseGoal)
    {
        switch (state_)
        {
            case PrototypeMissionState::Introduction:
                if (dialogueFinished)
                {
                    state_ = PrototypeMissionState::ReachVehicle;
                }
                break;
            case PrototypeMissionState::ReachVehicle:
                if (DistanceSquaredXZ(playerPosition, vehiclePosition) <= 9.0F)
                {
                    state_ = PrototypeMissionState::EnterVehicle;
                }
                break;
            case PrototypeMissionState::EnterVehicle:
                if (playerDriving)
                {
                    state_ = PrototypeMissionState::DriveToWarehouse;
                }
                break;
            case PrototypeMissionState::DriveToWarehouse:
                if (playerDriving && warehouseGoal.bounds.ContainsXZ(vehiclePosition))
                {
                    state_ = PrototypeMissionState::Completed;
                }
                break;
            case PrototypeMissionState::Completed:
                break;
        }
    }

    std::string PrototypeMission::GetObjectiveText() const
    {
        switch (state_)
        {
            case PrototypeMissionState::Introduction:
                return "Listen to Mara (Enter advances dialogue)";
            case PrototypeMissionState::ReachVehicle:
                return "Walk to the sedan";
            case PrototypeMissionState::EnterVehicle:
                return "Press E to enter the sedan";
            case PrototypeMissionState::DriveToWarehouse:
                return "Drive into the green warehouse marker";
            case PrototypeMissionState::Completed:
                return "Prototype mission complete";
        }
        return "Unknown objective";
    }
}
