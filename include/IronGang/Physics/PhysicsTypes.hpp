#pragma once

#include "Microsoft/Xna/Framework/Vector3.hpp"

#include <cstdint>

namespace IronGang::Physics
{
    using Microsoft::Xna::Framework::Vector3;

    // Opaque handles. Never expose the underlying physics library's own types (JPH::BodyID, ...)
    // outside PhysicsWorld.cpp -- mission/gameplay/asset code only ever sees these.
    struct RigidBodyHandle
    {
        static constexpr std::uint32_t kInvalidValue = 0xFFFFFFFFU;
        std::uint32_t value = kInvalidValue;

        [[nodiscard]] bool IsValid() const noexcept { return value != kInvalidValue; }
    };

    struct CharacterHandle
    {
        static constexpr std::uint32_t kInvalidValue = 0xFFFFFFFFU;
        std::uint32_t value = kInvalidValue;

        [[nodiscard]] bool IsValid() const noexcept { return value != kInvalidValue; }
    };

    struct VehicleHandle
    {
        static constexpr std::uint32_t kInvalidValue = 0xFFFFFFFFU;
        std::uint32_t value = kInvalidValue;

        [[nodiscard]] bool IsValid() const noexcept { return value != kInvalidValue; }
    };

    enum class ShapeKind
    {
        Box,
        Capsule,
    };

    // Box: halfExtents used. Capsule: radius + cylinderHalfHeight used (total capsule height is
    // 2 * (radius + cylinderHalfHeight), matching Jolt's CapsuleShape convention).
    struct ShapeDesc
    {
        ShapeKind kind = ShapeKind::Box;
        Vector3 halfExtents{0.5F, 0.5F, 0.5F};
        float radius = 0.5F;
        float cylinderHalfHeight = 0.5F;

        [[nodiscard]] static ShapeDesc Box(const Vector3& halfExtents) noexcept
        {
            ShapeDesc desc;
            desc.kind = ShapeKind::Box;
            desc.halfExtents = halfExtents;
            return desc;
        }

        [[nodiscard]] static ShapeDesc Capsule(float radius, float cylinderHalfHeight) noexcept
        {
            ShapeDesc desc;
            desc.kind = ShapeKind::Capsule;
            desc.radius = radius;
            desc.cylinderHalfHeight = cylinderHalfHeight;
            return desc;
        }
    };

    struct RaycastHit
    {
        bool hit = false;
        Vector3 point{};
        Vector3 normal{};
        float fraction = 1.0F;
        RigidBodyHandle body{};
    };

    struct TriggerEvent
    {
        RigidBodyHandle trigger{};
        RigidBodyHandle other{};
        bool entered = true; // false = the other body left the trigger volume
    };

    struct VehicleWheelState
    {
        Vector3 contactPoint{};
        float suspensionLength = 0.0F;
        float rotationAngle = 0.0F;
        float steerAngle = 0.0F;
        bool hasContact = false;
    };
}
