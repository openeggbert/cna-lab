#include "IronShadows/Gameplay/PoliceSystem.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace IronShadows
{
    namespace
    {
        constexpr float kWitnessRadius = 15.0F;
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

    void PoliceSystem::TriggerChase(const Vector3& spawnPosition)
    {
        state_ = PoliceState::Dispatched;
        dispatchTimer_ = kDispatchDelaySeconds;
        chaseTimer_ = 0.0F;
        resolveTimer_ = 0.0F;
        activePatrolCount_ = 1;
        patrolPositions_[0] = spawnPosition;
        patrolYaws_[0] = 0.0F;
    }

    void PoliceSystem::Update(float deltaSeconds,
                              bool playerDriving,
                              const Vector3& playerVehiclePosition,
                              float playerVehicleSpeedKph,
                              const std::vector<Vector3>& witnessPositions,
                              const Vector3& spawnPosition)
    {
        switch (state_)
        {
        case PoliceState::Clear:
        {
            if (!playerDriving)
            {
                break;
            }

            bool witnessedSpeeding = false;
            bool witnessedCrime = false;
            for (const Vector3& witness : witnessPositions)
            {
                const float distanceSquared = DistanceSquaredXZ(witness, playerVehiclePosition);
                if (distanceSquared <= kWitnessRadius * kWitnessRadius)
                {
                    if (playerVehicleSpeedKph > kOffenseSpeedThresholdKph)
                    {
                        witnessedSpeeding = true;
                    }
                    if (distanceSquared <= kCrimeContactDistance * kCrimeContactDistance)
                    {
                        witnessedCrime = true;
                    }
                }
            }

            if (witnessedSpeeding || witnessedCrime)
            {
                TriggerChase(spawnPosition);
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
    }
}
