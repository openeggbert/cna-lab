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

    // plan_22 IG-22-001/011: what started the chase. Recorded so the player can be told why they
    // are being chased -- "WANTED" with no reason is the single most common complaint about
    // systems like this, and the information already exists at the moment of detection.
    enum class PoliceOffence
    {
        None,
        Speeding,
        Collision,
        RanRedLight,
    };

    // Short player-facing text: "speeding", "hit someone", "ran a red light".
    [[nodiscard]] const char* PoliceOffenceName(PoliceOffence offence) noexcept;

    // What the player did this frame that a witness could report. The game decides these, because
    // signals and collisions are its knowledge, not the police system's.
    struct PoliceObservation
    {
        bool driving{false};
        float vehicleSpeedKph{0.0F};
        // True on the frame the player's vehicle crossed a stop line whose signal said stop.
        bool ranRedLight{false};
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
                                    const PoliceObservation& observation,
                                    const Vector3& playerVehiclePosition,
                                    const std::vector<Vector3>& witnessPositions,
                                    const Vector3& spawnPosition);

        [[nodiscard]] PoliceState GetState() const noexcept { return state_; }
        [[nodiscard]] int GetActivePatrolCount() const noexcept { return activePatrolCount_; }
        // Seconds the current chase has been running; 0 while Clear or Dispatched, and reset when
        // a chase resolves. Missions read this through the police_chase_seconds fact (plan_24
        // IG-24-006), which is how a mission can fail for staying wanted too long.
        [[nodiscard]] float GetChaseSeconds() const noexcept { return chaseTimer_; }
        // What the current response is for; None while Clear.
        [[nodiscard]] PoliceOffence GetOffence() const noexcept { return offence_; }
        [[nodiscard]] const Vector3& GetPatrolPosition(int index) const
        {
            return patrolPositions_[static_cast<std::size_t>(index)];
        }
        [[nodiscard]] float GetPatrolYaw(int index) const
        {
            return patrolYaws_[static_cast<std::size_t>(index)];
        }

    private:
        void TriggerChase(const Vector3& spawnPosition, PoliceOffence offence);

        PoliceState state_{PoliceState::Clear};
        PoliceOffence offence_{PoliceOffence::None};
        float dispatchTimer_{0.0F};
        float chaseTimer_{0.0F};
        float resolveTimer_{0.0F};
        int activePatrolCount_{0};
        std::array<Vector3, 2> patrolPositions_{};
        std::array<float, 2> patrolYaws_{};
    };
}
