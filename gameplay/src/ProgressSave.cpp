#include "CopperBoots/ProgressSave.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace CopperBoots
{
    namespace
    {
        [[nodiscard]] std::uint64_t Checksum(const std::string_view text) noexcept
        {
            std::uint64_t value = 14'695'981'039'346'656'037ULL;
            for (const unsigned char byte : text) {
                value ^= byte;
                value *= 1'099'511'628'211ULL;
            }
            return value;
        }

        [[nodiscard]] std::vector<std::string_view> SplitLines(
            std::string_view text)
        {
            std::vector<std::string_view> lines;
            while (!text.empty()) {
                const std::size_t end = text.find('\n');
                std::string_view line = text.substr(0, end);
                if (!line.empty() && line.back() == '\r')
                    line.remove_suffix(1);
                lines.push_back(line);
                if (end == std::string_view::npos)
                    break;
                text.remove_prefix(end + 1);
            }
            if (!lines.empty() && lines.back().empty())
                lines.pop_back();
            return lines;
        }

        [[nodiscard]] std::string_view After(
            const std::string_view line, const std::string_view prefix)
        {
            if (!line.starts_with(prefix))
                throw std::runtime_error("unexpected progress directive");
            return line.substr(prefix.size());
        }

        template <typename Value>
        [[nodiscard]] Value ParseInteger(const std::string_view text,
                                         const int base = 10)
        {
            Value value{};
            const auto result = std::from_chars(
                text.data(), text.data() + text.size(), value, base);
            if (text.empty() || result.ec != std::errc{} ||
                result.ptr != text.data() + text.size()) {
                throw std::runtime_error("invalid progress number");
            }
            return value;
        }

        [[nodiscard]] std::string Hex(const std::uint64_t value)
        {
            std::array<char, 16> buffer{};
            const auto result = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value, 16);
            if (result.ec != std::errc{})
                throw std::runtime_error("cannot serialize progress checksum");
            return std::string(buffer.data(), result.ptr);
        }

        [[nodiscard]] std::string Payload(const ProgressData& data,
                                          const std::uint64_t generation)
        {
            return "copper-boots-progress 1\n"
                   "generation " + std::to_string(generation) + "\n" +
                   "unlocked-stage " +
                       std::to_string(data.HighestUnlockedStage) + "\n" +
                   "best-score " + std::to_string(data.BestScore) + "\n" +
                   "best-completion-ticks " +
                       std::to_string(data.BestCompletionTicks) + "\n";
        }
    }

    std::string EncodeProgressSlot(const ProgressData& data,
                                   const std::uint64_t generation)
    {
        if (generation == 0 || data.HighestUnlockedStage < 1 ||
            data.HighestUnlockedStage > 1'000 || data.BestScore < 0) {
            throw std::runtime_error("progress values are out of range");
        }
        const std::string payload = Payload(data, generation);
        return payload + "checksum " + Hex(Checksum(payload)) + '\n';
    }

    DecodedProgressSlot DecodeProgressSlot(
        const std::optional<std::string_view> document) noexcept
    {
        DecodedProgressSlot decoded;
        if (!document.has_value())
            return decoded;
        try {
            const std::vector<std::string_view> lines = SplitLines(*document);
            if (lines.size() != 6U ||
                lines[0] != "copper-boots-progress 1") {
                return decoded;
            }
            decoded.Generation = ParseInteger<std::uint64_t>(
                After(lines[1], "generation "));
            decoded.Data.HighestUnlockedStage = ParseInteger<int>(
                After(lines[2], "unlocked-stage "));
            decoded.Data.BestScore = ParseInteger<int>(
                After(lines[3], "best-score "));
            decoded.Data.BestCompletionTicks = ParseInteger<std::uint64_t>(
                After(lines[4], "best-completion-ticks "));
            const std::uint64_t storedChecksum = ParseInteger<std::uint64_t>(
                After(lines[5], "checksum "), 16);
            if (decoded.Generation == 0 ||
                decoded.Data.HighestUnlockedStage < 1 ||
                decoded.Data.HighestUnlockedStage > 1'000 ||
                decoded.Data.BestScore < 0) {
                return decoded;
            }
            decoded.Valid = storedChecksum == Checksum(Payload(
                decoded.Data, decoded.Generation));
        }
        catch (...) {
        }
        return decoded;
    }

    ProgressLoadResult ChooseProgressSlot(
        const std::optional<std::string_view> slotA,
        const std::optional<std::string_view> slotB) noexcept
    {
        const DecodedProgressSlot a = DecodeProgressSlot(slotA);
        const DecodedProgressSlot b = DecodeProgressSlot(slotB);
        ProgressLoadResult result;
        if (a.Valid && (!b.Valid || a.Generation >= b.Generation)) {
            result = {a.Data, a.Generation, ProgressSlot::A,
                      !b.Valid && b.Generation > a.Generation};
        }
        else if (b.Valid) {
            result = {b.Data, b.Generation, ProgressSlot::B,
                      !a.Valid && a.Generation > b.Generation};
        }
        return result;
    }

    ProgressSlot NextProgressWriteSlot(
        const ProgressLoadResult& current) noexcept
    {
        return current.Source == ProgressSlot::A
            ? ProgressSlot::B
            : ProgressSlot::A;
    }

    bool RecordLevelCompletion(ProgressData& progress,
                               const int completedStage,
                               const int score,
                               const std::uint64_t completionTicks) noexcept
    {
        bool changed = false;
        const int unlockedStage = completedStage >= 999
            ? 1'000
            : std::max(1, completedStage + 1);
        if (unlockedStage > progress.HighestUnlockedStage) {
            progress.HighestUnlockedStage = unlockedStage;
            changed = true;
        }
        if (score > progress.BestScore) {
            progress.BestScore = score;
            changed = true;
        }
        if (completionTicks > 0 &&
            (progress.BestCompletionTicks == 0 ||
             completionTicks < progress.BestCompletionTicks)) {
            progress.BestCompletionTicks = completionTicks;
            changed = true;
        }
        return changed;
    }
}
