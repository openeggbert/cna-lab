#pragma once

#include <cstdint>

namespace WolfCna
{
    enum class PlayerWeapon : int
    {
        Knife = 0,
        Sidearm = 1,
        Repeater = 2,
        HeavyAutomatic = 3
    };

    struct WeaponSpec
    {
        float range = 0.9f;
        int nearDamage = 2;
        int farDamage = 2;
        float falloffStart = 0.9f;
        float standingSpreadRadians = 0.0f;
        float movingSpreadRadians = 0.0f;
        float cadenceSeconds = 0.36f;
        bool automatic = false;
        bool emitsNoise = false;
    };

    struct FirearmShot
    {
        bool emitted = false;
        int ammunitionAfter = 0;
        std::uint32_t sequenceAfter = 0;
        float yawOffsetRadians = 0.0f;
    };

    [[nodiscard]] constexpr WeaponSpec GetWeaponSpec(PlayerWeapon weapon)
    {
        switch (weapon)
        {
        case PlayerWeapon::Knife:
            return {};
        case PlayerWeapon::Sidearm:
            return {
                .range = 12.0f,
                .nearDamage = 2,
                .farDamage = 1,
                .falloffStart = 4.5f,
                .standingSpreadRadians = 0.008f,
                .movingSpreadRadians = 0.035f,
                .cadenceSeconds = 0.32f,
                .automatic = false,
                .emitsNoise = true};
        case PlayerWeapon::Repeater:
            return {
                .range = 11.0f,
                .nearDamage = 2,
                .farDamage = 1,
                .falloffStart = 3.5f,
                .standingSpreadRadians = 0.018f,
                .movingSpreadRadians = 0.055f,
                .cadenceSeconds = 0.12f,
                .automatic = true,
                .emitsNoise = true};
        case PlayerWeapon::HeavyAutomatic:
            return {
                .range = 10.0f,
                .nearDamage = 3,
                .farDamage = 1,
                .falloffStart = 3.0f,
                .standingSpreadRadians = 0.035f,
                .movingSpreadRadians = 0.085f,
                .cadenceSeconds = 0.085f,
                .automatic = true,
                .emitsNoise = true};
        }
        return {};
    }

    [[nodiscard]] constexpr std::uint32_t CombatSeedForSector(
        int levelIndex,
        int difficulty)
    {
        return 0x57434E41u ^
            (static_cast<std::uint32_t>(levelIndex + 1) * 0x9E3779B9u) ^
            (static_cast<std::uint32_t>(difficulty + 1) * 0x85EBCA6Bu);
    }

    [[nodiscard]] FirearmShot ResolveFirearmShot(
        PlayerWeapon weapon,
        int ammunition,
        std::uint32_t seed,
        std::uint32_t sequence,
        bool moving);
}
