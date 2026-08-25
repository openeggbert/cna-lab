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

        // How long an enemy hesitates between noticing the player and committing to the
        // chase, and how far it hears gunfire. Without these, a Veteran guard reacted
        // exactly as slowly as a Scout guard, which is the part of "harder means more
        // aggressive" that the multipliers above never covered.
        float reactionDelayMultiplier = 1.0f;
        float hearingRangeMultiplier = 1.0f;

        // How many ranged enemies may fire at once. The single-attacker throttle is what
        // kept extra Veteran spawns from ever producing extra incoming fire, so the higher
        // tiers stopped feeling harder however many enemies they added.
        int maximumRangedAttackers = 1;
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
                .startingAmmunition = 16,
                .reactionDelayMultiplier = 1.4f,
                .hearingRangeMultiplier = 0.75f,
                .maximumRangedAttackers = 1};
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
                .startingAmmunition = 8,
                .reactionDelayMultiplier = 0.65f,
                .hearingRangeMultiplier = 1.3f,
                .maximumRangedAttackers = 2};
        }
        return {};
    }
}
