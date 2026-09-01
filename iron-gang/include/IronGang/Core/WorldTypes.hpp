#pragma once

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace IronGang
{
    using Microsoft::Xna::Framework::Color;
    using Microsoft::Xna::Framework::Vector3;

    struct Aabb
    {
        Vector3 center{};
        Vector3 halfExtents{};

        [[nodiscard]] bool IntersectsCircleXZ(const Vector3& position, float radius) const
        {
            const float closestX = std::clamp(position.X,
                                              center.X - halfExtents.X,
                                              center.X + halfExtents.X);
            const float closestZ = std::clamp(position.Z,
                                              center.Z - halfExtents.Z,
                                              center.Z + halfExtents.Z);
            const float dx = position.X - closestX;
            const float dz = position.Z - closestZ;
            return dx * dx + dz * dz < radius * radius;
        }

        [[nodiscard]] bool ContainsXZ(const Vector3& position) const
        {
            return position.X >= center.X - halfExtents.X &&
                   position.X <= center.X + halfExtents.X &&
                   position.Z >= center.Z - halfExtents.Z &&
                   position.Z <= center.Z + halfExtents.Z;
        }
    };

    struct WorldBox
    {
        std::string name;
        Vector3 center{};
        Vector3 size{1.0F, 1.0F, 1.0F};
        Color color{255, 255, 255, 255};
        bool collidable{true};
    };

    struct TriggerZone
    {
        std::string id;
        Aabb bounds{};
    };

    // Iron Gang uses discrete districts connected by loading screens (Mafia-1 style), not
    // seamless open-world streaming -- see plan/plan_13-world-partitioning-and-streaming.md.
    // Two districts is the minimum needed to prove the transition mechanism (gate M5); real
    // story/content districts are added the same way once this mechanism is proven.
    enum class DistrictId
    {
        WarehouseBlock,
        Countryside,
    };

    // A one-way link from the current district to another: a trigger volume that, when entered,
    // requests a transition landing the player/vehicle at targetEntryPosition/targetEntryYaw in
    // targetDistrict.
    struct DistrictExit
    {
        TriggerZone trigger;
        DistrictId targetDistrict{DistrictId::WarehouseBlock};
        Vector3 targetEntryPosition{};
        float targetEntryYaw{0.0F};
    };

    [[nodiscard]] inline float DistanceSquaredXZ(const Vector3& a, const Vector3& b)
    {
        const float dx = a.X - b.X;
        const float dz = a.Z - b.Z;
        return dx * dx + dz * dz;
    }

    [[nodiscard]] inline Vector3 ForwardFromYaw(float yaw)
    {
        return Vector3(std::sin(yaw), 0.0F, -std::cos(yaw));
    }

    [[nodiscard]] inline Vector3 RightFromYaw(float yaw)
    {
        const Vector3 forward = ForwardFromYaw(yaw);
        return Vector3(-forward.Z, 0.0F, forward.X);
    }
}
