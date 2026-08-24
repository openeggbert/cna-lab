#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace IronGang
{
    enum class PoliceState
    {
        Clear,
        Dispatched,
        Chasing,
    };

    struct PoliceUpdateWorkload
    {
        std::size_t witnessChecks{0};
        std::size_t patrolUpdates{0};
    };

    // Gate M9 (plan_22-police-witnesses-crime-and-wanted-response.md IG-22-001/002/003/004/010):
    // a minimal witnessed-offense -> chase -> resolve state machine, with exactly one escalation
    // level (a second patrol car joins if the chase runs long) -- matching the locked scope
    // decision "a witnessed offense triggers a chase with one escalation level; no multi-tier
    // wanted stars, search-area decay, roadblocks, or detective persistence". Witness perception
    // is deliberately a fixed-radius proximity check, not a real vision cone/line-of-sight test,
    // and patrol cars drive straight toward the player rather than following road waypoints --
    // both documented simplifications for this first pass, not oversights.
    class PoliceSystem final
    {
    public:
        void Reset();

        // witnessPositions is every traffic vehicle's and pedestrian's current position this
        // frame (anyone who could "witness" an offense while Clear). spawnPosition is where a
        // newly dispatched patrol car appears; it is only read at the moment a chase starts (or
        // escalates), not every frame.
        PoliceUpdateWorkload Update(float deltaSeconds,
                                    bool playerDriving,
                                    const Vector3& playerVehiclePosition,
                                    float playerVehicleSpeedKph,
                                    const std::vector<Vector3>& witnessPositions,
                                    const Vector3& spawnPosition);

        [[nodiscard]] PoliceState GetState() const noexcept { return state_; }
        [[nodiscard]] int GetActivePatrolCount() const noexcept { return activePatrolCount_; }
        [[nodiscard]] const Vector3& GetPatrolPosition(int index) const
        {
            return patrolPositions_[static_cast<std::size_t>(index)];
        }
        [[nodiscard]] float GetPatrolYaw(int index) const
        {
            return patrolYaws_[static_cast<std::size_t>(index)];
        }

    private:
        void TriggerChase(const Vector3& spawnPosition);

        PoliceState state_{PoliceState::Clear};
        float dispatchTimer_{0.0F};
        float chaseTimer_{0.0F};
        float resolveTimer_{0.0F};
        int activePatrolCount_{0};
        std::array<Vector3, 2> patrolPositions_{};
        std::array<float, 2> patrolYaws_{};
    };
}
