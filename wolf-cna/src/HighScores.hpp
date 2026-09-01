#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace WolfCna
{
    inline constexpr std::size_t MaximumHighScoreEntries = 8;
    inline constexpr int MaximumHighScoreValue = 999999999;

    struct HighScoreEntry final
    {
        std::string initials;
        int score = 0;

        bool operator==(const HighScoreEntry&) const = default;
    };

    [[nodiscard]] inline bool AreValidInitials(std::string_view initials)
    {
        return initials.size() == 3 && std::all_of(
            initials.begin(),
            initials.end(),
            [](char character) { return character >= 'A' && character <= 'Z'; });
    }

    [[nodiscard]] inline std::vector<HighScoreEntry> NormalizeHighScores(
        std::vector<HighScoreEntry> entries)
    {
        std::erase_if(
            entries,
            [](const HighScoreEntry& entry)
            {
                return !AreValidInitials(entry.initials) || entry.score <= 0 ||
                    entry.score > MaximumHighScoreValue;
            });
        std::stable_sort(
            entries.begin(),
            entries.end(),
            [](const HighScoreEntry& left, const HighScoreEntry& right)
            {
                return left.score > right.score;
            });
        if (entries.size() > MaximumHighScoreEntries)
            entries.resize(MaximumHighScoreEntries);
        return entries;
    }

    [[nodiscard]] inline bool QualifiesForHighScores(
        const std::vector<HighScoreEntry>& entries,
        int score)
    {
        if (score <= 0 || score > MaximumHighScoreValue)
            return false;
        const std::vector<HighScoreEntry> normalized = NormalizeHighScores(entries);
        return normalized.size() < MaximumHighScoreEntries ||
            score > normalized.back().score;
    }

    [[nodiscard]] inline std::vector<HighScoreEntry> InsertHighScore(
        std::vector<HighScoreEntry> entries,
        HighScoreEntry entry)
    {
        if (AreValidInitials(entry.initials) && entry.score > 0)
            entries.push_back(std::move(entry));
        return NormalizeHighScores(std::move(entries));
    }
}
