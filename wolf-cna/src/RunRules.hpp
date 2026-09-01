#pragma once

#include <algorithm>

namespace WolfCna
{
    struct LifeLossResult
    {
        int remainingLives = 0;
        int score = 0;
        int nextExtraLifeScore = 40000;
        bool restartSector = false;
    };

    [[nodiscard]] constexpr LifeLossResult ResolveLifeLoss(
        int currentLives,
        int currentScore,
        int currentNextExtraLifeScore,
        int sectorEntryScore,
        int sectorEntryNextExtraLifeScore)
    {
        const int remainingLives = std::max(0, currentLives - 1);
        const bool restart = remainingLives > 0;
        return {
            .remainingLives = remainingLives,
            .score = restart ? sectorEntryScore : currentScore,
            .nextExtraLifeScore = restart
                ? sectorEntryNextExtraLifeScore
                : currentNextExtraLifeScore,
            .restartSector = restart};
    }
}
