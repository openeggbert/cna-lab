#pragma once

namespace WolfCna
{
    enum class Difficulty
    {
        Scout,
        Operative,
        Veteran
    };

    struct DifficultyProfile
    {
        float incomingDamageMultiplier = 1.0f;
        float enemyHealthMultiplier = 1.0f;
        float enemySpeedMultiplier = 1.0f;
        float enemyAttackIntervalMultiplier = 1.0f;
        float ammunitionMultiplier = 1.0f;
        int maximumEnemySpawnTier = 1;
        int startingAmmunition = 12;
    };

    [[nodiscard]] constexpr DifficultyProfile GetDifficultyProfile(Difficulty difficulty)
    {
        switch (difficulty)
        {
        case Difficulty::Scout:
            return {
                .incomingDamageMultiplier = 0.7f,
                .enemyHealthMultiplier = 0.75f,
                .enemySpeedMultiplier = 0.85f,
                .enemyAttackIntervalMultiplier = 1.25f,
                .ammunitionMultiplier = 1.6f,
                .maximumEnemySpawnTier = 0,
                .startingAmmunition = 16};
        case Difficulty::Operative:
            return {};
        case Difficulty::Veteran:
            return {
                .incomingDamageMultiplier = 1.3f,
                .enemyHealthMultiplier = 1.25f,
                .enemySpeedMultiplier = 1.15f,
                .enemyAttackIntervalMultiplier = 0.8f,
                .ammunitionMultiplier = 0.7f,
                .maximumEnemySpawnTier = 2,
                .startingAmmunition = 8};
        }
        return {};
    }
}
