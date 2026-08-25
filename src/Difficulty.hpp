#pragma once

namespace WolfCna
{
    enum class Difficulty
    {
        Scout,
        Operative,
        Veteran,
        Phantom
    };

    inline constexpr int DifficultyCount = 4;

    [[nodiscard]] constexpr bool IsValidDifficultyValue(int value)
    {
        return value >= 0 && value < DifficultyCount;
    }

    [[nodiscard]] constexpr const char* DifficultyName(Difficulty difficulty)
    {
        switch (difficulty)
        {
        case Difficulty::Scout: return "SCOUT";
        case Difficulty::Operative: return "OPERATIVE";
        case Difficulty::Veteran: return "VETERAN";
        case Difficulty::Phantom: return "PHANTOM";
        }
        return "OPERATIVE";
    }

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

        // Player resilience, and how far an enemy will engage from. Neither scaled before,
        // which is why the top rungs did not feel harder in real corridors: the extra
        // shooters the limit below allows only matter if that many enemies actually engage.
        int startingLives = 3;
        float healthPickupMultiplier = 1.0f;
        float wakeRangeMultiplier = 1.0f;

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
                .wakeRangeMultiplier = 1.15f,
                .maximumRangedAttackers = 2};
        case Difficulty::Phantom:
            // Veteran already spawns every authored encounter tier, so a fourth rung
            // cannot add enemies without new level authoring. Phantom therefore keeps
            // Veteran's exact roster, health and ammunition -- which also keeps the
            // campaign clear-budget audits meaningful -- and escalates only behaviour:
            // it hits harder, reacts sooner, hears further and fields a third shooter.
            // Ammunition in particular cannot be reduced: Veteran's supply already equals
            // its clear budget exactly, with zero slack, so taking rounds away would make
            // the campaign unclearable rather than merely harder.
            return {
                .incomingDamageMultiplier = 1.6f,
                .enemyHealthMultiplier = 1.25f,
                .enemySpeedMultiplier = 1.25f,
                .enemyAttackIntervalMultiplier = 0.65f,
                .ammunitionMultiplier = 0.7f,
                .maximumEnemySpawnTier = 3,
                .startingAmmunition = 8,
                .reactionDelayMultiplier = 0.45f,
                .hearingRangeMultiplier = 1.5f,
                .startingLives = 2,
                .healthPickupMultiplier = 0.7f,
                .wakeRangeMultiplier = 1.4f,
                .maximumRangedAttackers = 3};
        }
        return {};
    }
}
