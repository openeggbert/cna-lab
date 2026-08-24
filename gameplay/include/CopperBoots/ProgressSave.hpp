#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace CopperBoots
{
    struct ProgressData
    {
        int HighestUnlockedStage = 1;
        int BestScore = 0;
        std::uint64_t BestCompletionTicks = 0;

        bool operator==(const ProgressData&) const = default;
    };

    enum class ProgressSlot
    {
        None,
        A,
        B,
    };

    struct DecodedProgressSlot
    {
        ProgressData Data;
        std::uint64_t Generation = 0;
        bool Valid = false;
    };

    struct ProgressLoadResult
    {
        ProgressData Data;
        std::uint64_t Generation = 0;
        ProgressSlot Source = ProgressSlot::None;
        bool RecoveredOlderSlot = false;
    };

    [[nodiscard]] std::string EncodeProgressSlot(
        const ProgressData& data, std::uint64_t generation);
    [[nodiscard]] DecodedProgressSlot DecodeProgressSlot(
        std::optional<std::string_view> document) noexcept;
    [[nodiscard]] ProgressLoadResult ChooseProgressSlot(
        std::optional<std::string_view> slotA,
        std::optional<std::string_view> slotB) noexcept;
    [[nodiscard]] ProgressSlot NextProgressWriteSlot(
        const ProgressLoadResult& current) noexcept;
    [[nodiscard]] bool RecordLevelCompletion(
        ProgressData& progress, int completedStage, int score,
        std::uint64_t completionTicks) noexcept;
}
