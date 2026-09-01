#pragma once

#include <algorithm>
#include <cmath>

namespace WolfCna
{
    struct CompletionScore final
    {
        int killPercentage = 0;
        int treasurePercentage = 0;
        int secretPercentage = 0;
        int elapsedSeconds = 0;
        int targetSeconds = 0;
        int baseBonus = 0;
        int timeBonus = 0;
        int killPerfectBonus = 0;
        int treasurePerfectBonus = 0;
        int secretPerfectBonus = 0;
        int totalBonus = 0;
    };

    [[nodiscard]] constexpr int CompletionPercentage(int completed, int total)
    {
        if (total <= 0)
            return 100;
        return std::clamp(completed, 0, total) * 100 / total;
    }

    [[nodiscard]] inline CompletionScore CalculateCompletionScore(
        int defeatedEnemies,
        int totalEnemies,
        int collectedTreasure,
        int totalTreasure,
        int foundSecrets,
        int totalSecrets,
        float elapsedSeconds,
        int targetSeconds)
    {
        constexpr int BaseClearBonus = 1000;
        constexpr int PerfectCategoryBonus = 1500;
        constexpr int TimeBonusPerSecond = 20;

        CompletionScore result;
        result.killPercentage = CompletionPercentage(defeatedEnemies, totalEnemies);
        result.treasurePercentage = CompletionPercentage(collectedTreasure, totalTreasure);
        result.secretPercentage = CompletionPercentage(foundSecrets, totalSecrets);
        result.elapsedSeconds = std::max(0, static_cast<int>(std::ceil(elapsedSeconds)));
        result.targetSeconds = std::max(0, targetSeconds);
        result.baseBonus = BaseClearBonus;
        result.timeBonus = std::max(0, result.targetSeconds - result.elapsedSeconds) *
            TimeBonusPerSecond;
        result.killPerfectBonus = result.killPercentage == 100 ? PerfectCategoryBonus : 0;
        result.treasurePerfectBonus = result.treasurePercentage == 100
            ? PerfectCategoryBonus
            : 0;
        result.secretPerfectBonus = result.secretPercentage == 100
            ? PerfectCategoryBonus
            : 0;
        result.totalBonus = result.baseBonus + result.timeBonus +
            result.killPerfectBonus + result.treasurePerfectBonus +
            result.secretPerfectBonus;
        return result;
    }
}
