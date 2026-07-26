#pragma once

#include "IronShadows/Core/WorldTypes.hpp"

#include <string>

namespace IronShadows
{
    enum class PrototypeMissionState : int
    {
        Introduction = 0,
        ReachVehicle = 1,
        EnterVehicle = 2,
        DriveToWarehouse = 3,
        Completed = 4
    };

    class PrototypeMission final
    {
    public:
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
        PrototypeMissionState state_{PrototypeMissionState::Introduction};
    };
}
