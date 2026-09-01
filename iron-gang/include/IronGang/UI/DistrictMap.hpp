#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"

#include <span>

namespace IronGang
{
    // Pure world-to-screen projection shared by the live district-map overlay and deterministic
    // layout tests. World -Z is north/up, matching the default player forward direction.
    struct DistrictMapProjection
    {
        float minimumX{-1.0F};
        float maximumX{1.0F};
        float minimumZ{-1.0F};
        float maximumZ{1.0F};
        Microsoft::Xna::Framework::Rectangle screenBounds{};

        [[nodiscard]] Microsoft::Xna::Framework::Vector2 ProjectPoint(const Vector3& position) const noexcept;
        [[nodiscard]] Microsoft::Xna::Framework::Rectangle ProjectBox(const WorldBox& box) const noexcept;
    };

    [[nodiscard]] DistrictMapProjection BuildDistrictMapProjection(
        std::span<const WorldBox> boxes,
        const Microsoft::Xna::Framework::Rectangle& screenBounds) noexcept;
}
