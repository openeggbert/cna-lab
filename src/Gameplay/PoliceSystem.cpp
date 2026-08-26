#include "IronGang/Gameplay/PoliceSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace IronGang
{
    namespace
    {
        constexpr float kCrimeContactDistance = 2.5F;
        constexpr float kOffenseSpeedThresholdKph = 70.0F;
        constexpr float kDispatchDelaySeconds = 2.0F;
        constexpr float kEscalationSeconds = 20.0F;
        constexpr float kResolveDistance = 40.0F;
        constexpr float kResolveSustainSeconds = 3.0F;
        constexpr float kPatrolSpeed = 9.0F;
    }

    void PoliceSystem::Reset()
    {
        *this = PoliceSystem{};
    }

    const char* PoliceOffenceName(PoliceOffence offence) noexcept
    {
        switch (offence)
        {
            case PoliceOffence::None: return "";
            case PoliceOffence::Speeding: return "speeding";
            case PoliceOffence::Collision: return "hit someone";
            case PoliceOffence::RanRedLight: return "ran a red light";
        }
        return "";
    }

    void PoliceSystem::TriggerChase(const Vector3& spawnPosition, PoliceOffence offence)
    {
        state_ = PoliceState::Dispatched;
        offence_ = offence;
        dispatchTimer_ = kDispatchDelaySeconds;
        chaseTimer_ = 0.0F;
        resolveTimer_ = 0.0F;
        activePatrolCount_ = 1;
        patrolPositions_[0] = spawnPosition;
        patrolYaws_[0] = 0.0F;
    }

    PoliceUpdateWorkload PoliceSystem::Update(float deltaSeconds,
                                               const PoliceObservation& observation,
                                               const Vector3& playerVehiclePosition,
                                               const std::vector<Vector3>& witnessPositions,
                                               const Vector3& spawnPosition)
    {
        PoliceUpdateWorkload workload;
        switch (state_)
        {
        case PoliceState::Clear:
        {
            if (!observation.driving)
            {
                break;
            }

            bool witnessedSpeeding = false;
            bool witnessedCrime = false;
            bool witnessedRedLight = false;
            for (const Vector3& witness : witnessPositions)
            {
                ++workload.witnessChecks;
                const float distanceSquared = DistanceSquaredXZ(witness, playerVehiclePosition);
                if (distanceSquared <= PoliceSystem::kWitnessRadius * PoliceSystem::kWitnessRadius)
                {
                    if (observation.vehicleSpeedKph > kOffenseSpeedThresholdKph)
                    {
                        witnessedSpeeding = true;
                    }
                    if (observation.ranRedLight)
                    {
                        witnessedRedLight = true;
                    }
                    if (distanceSquared <= kCrimeContactDistance * kCrimeContactDistance)
                    {
                        witnessedCrime = true;
                    }
                }
            }

            // Ordered by how serious it is, so the reason the player is told is the worst thing
            // they did rather than whichever check happened to run first.
            if (witnessedCrime)
            {
                TriggerChase(spawnPosition, PoliceOffence::Collision);
            }
            else if (witnessedRedLight)
            {
                TriggerChase(spawnPosition, PoliceOffence::RanRedLight);
            }
            else if (witnessedSpeeding)
            {
                TriggerChase(spawnPosition, PoliceOffence::Speeding);
            }
            break;
        }
        case PoliceState::Dispatched:
        {
            dispatchTimer_ -= deltaSeconds;
            if (dispatchTimer_ <= 0.0F)
            {
                state_ = PoliceState::Chasing;
            }
            break;
        }
        case PoliceState::Chasing:
        {
            chaseTimer_ += deltaSeconds;
            if (activePatrolCount_ == 1 && chaseTimer_ >= kEscalationSeconds)
            {
                activePatrolCount_ = 2;
                patrolPositions_[1] = spawnPosition;
                patrolYaws_[1] = 0.0F;
            }

            float closestDistance = std::numeric_limits<float>::max();
            for (int i = 0; i < activePatrolCount_; ++i)
            {
                ++workload.patrolUpdates;
                const std::size_t index = static_cast<std::size_t>(i);
                Vector3 toPlayer = playerVehiclePosition - patrolPositions_[index];
                toPlayer.Y = 0.0F;
                const float distance = toPlayer.Length();
                closestDistance = std::min(closestDistance, distance);
                if (distance > 1e-4F)
                {
                    const Vector3 direction = toPlayer / distance;
                    const float step = std::min(distance, kPatrolSpeed * deltaSeconds);
                    patrolPositions_[index] += direction * step;
                    patrolYaws_[index] = std::atan2(direction.X, -direction.Z);
                }
            }

            if (closestDistance > kResolveDistance)
            {
                resolveTimer_ += deltaSeconds;
                if (resolveTimer_ >= kResolveSustainSeconds)
                {
                    state_ = PoliceState::Clear;
                    offence_ = PoliceOffence::None;
                    activePatrolCount_ = 0;
                    chaseTimer_ = 0.0F;
                    resolveTimer_ = 0.0F;
                }
            }
            else
            {
                resolveTimer_ = 0.0F;
            }
            break;
        }
        }
        return workload;
    }
}
