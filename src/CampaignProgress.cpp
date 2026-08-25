#include "CampaignProgress.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <utility>

namespace WolfCna
{
    namespace
    {
        constexpr std::string_view LegacyHeader = "WOLF-CNA-PROGRESS-1";
        constexpr std::string_view BooleanSoundHeader = "WOLF-CNA-PROGRESS-2";
        constexpr std::string_view VolumeHeader = "WOLF-CNA-PROGRESS-3";
        constexpr std::string_view FieldOfViewHeader = "WOLF-CNA-PROGRESS-4";
        constexpr std::string_view HighScoreHeader = "WOLF-CNA-PROGRESS-5";
        constexpr std::string_view ControlHeader = "WOLF-CNA-PROGRESS-6";
        constexpr std::string_view Header = "WOLF-CNA-PROGRESS-7";
        constexpr std::array SupportedFieldOfView = {60, 72, 84, 96};

        bool IsSupportedFieldOfView(int fieldOfView)
        {
            return std::find(SupportedFieldOfView.begin(), SupportedFieldOfView.end(), fieldOfView) !=
                SupportedFieldOfView.end();
        }
    }

    CampaignProfile CampaignProgress::Load(
        const std::filesystem::path& path,
        int levelCount)
    {
        std::ifstream input(path);
        if (!input)
            return {};

        std::ostringstream text;
        text << input.rdbuf();
        if (input.bad())
            return {};
        return Parse(text.str(), levelCount);
    }

    void CampaignProgress::Save(
        const std::filesystem::path& path,
        const CampaignProfile& profile,
        int levelCount)
    {
        std::ofstream output(path, std::ios::trunc);
        if (!output)
            return;
        output << Serialize(profile, levelCount);
    }

    CampaignProfile CampaignProgress::Parse(std::string_view text, int levelCount)
    {
        if (levelCount <= 0)
            return {};

        std::istringstream input{std::string(text)};
        std::string header;
        CampaignProfile profile;
        std::string trailing;
        if (!(input >> header >> profile.highestUnlocked))
            return {};

        if (header == LegacyHeader)
        {
            if (input >> trailing)
                return {};
        }
        else if (header == BooleanSoundHeader)
        {
            int soundEnabled = 1;
            if (!(input >> soundEnabled >> profile.difficulty) ||
                (soundEnabled != 0 && soundEnabled != 1) ||
                profile.difficulty < 0 || profile.difficulty > 2 ||
                (input >> trailing))
            {
                return {};
            }
            profile.soundVolume = soundEnabled != 0 ? 4 : 0;
        }
        else if (header == VolumeHeader)
        {
            if (!(input >> profile.soundVolume >> profile.difficulty) ||
                profile.soundVolume < 0 || profile.soundVolume > 4 ||
                profile.difficulty < 0 || profile.difficulty > 2 ||
                (input >> trailing))
            {
                return {};
            }
        }
        else if (header == FieldOfViewHeader)
        {
            if (!(input >> profile.soundVolume >> profile.difficulty >> profile.fieldOfView) ||
                profile.soundVolume < 0 || profile.soundVolume > 4 ||
                profile.difficulty < 0 || profile.difficulty > 2 ||
                !IsSupportedFieldOfView(profile.fieldOfView) ||
                (input >> trailing))
            {
                return {};
            }
        }
        else if (header == HighScoreHeader)
        {
            std::size_t highScoreCount = 0;
            if (!(input >> profile.soundVolume >> profile.difficulty >>
                    profile.fieldOfView >> highScoreCount) ||
                profile.soundVolume < 0 || profile.soundVolume > 4 ||
                profile.difficulty < 0 || profile.difficulty > 2 ||
                !IsSupportedFieldOfView(profile.fieldOfView) ||
                highScoreCount > MaximumHighScoreEntries)
            {
                return {};
            }

            profile.highScores.reserve(highScoreCount);
            for (std::size_t index = 0; index < highScoreCount; ++index)
            {
                HighScoreEntry entry;
                if (!(input >> entry.initials >> entry.score) ||
                    !AreValidInitials(entry.initials) ||
                    entry.score <= 0 || entry.score > MaximumHighScoreValue)
                {
                    return {};
                }
                profile.highScores.push_back(std::move(entry));
            }
            if (input >> trailing)
                return {};
            profile.highScores = NormalizeHighScores(std::move(profile.highScores));
        }
        else if (header == ControlHeader || header == Header)
        {
            // Version 6 shares every field with version 7 except the mouse settings, which
            // migrate to the classic-feel defaults rather than disabling mouse control.
            const bool hasMouseSettings = header == Header;
            if (!(input >> profile.soundVolume >> profile.difficulty >>
                    profile.fieldOfView >> profile.controls.turnSensitivityStep) ||
                profile.soundVolume < 0 || profile.soundVolume > 4 ||
                profile.difficulty < 0 || profile.difficulty > 2 ||
                !IsSupportedFieldOfView(profile.fieldOfView))
            {
                return {};
            }

            if (hasMouseSettings)
            {
                int mouseEnabled = 1;
                if (!(input >> mouseEnabled >> profile.controls.mouseSensitivityStep) ||
                    (mouseEnabled != 0 && mouseEnabled != 1))
                {
                    return {};
                }
                profile.controls.mouseEnabled = mouseEnabled != 0;
            }

            std::size_t bindingCount = 0;
            if (!(input >> bindingCount) || bindingCount != ControlActionCount)
                return {};

            for (std::size_t index = 0; index < bindingCount; ++index)
            {
                int actionValue = -1;
                int keyValue = 0;
                if (!(input >> actionValue >> keyValue) ||
                    actionValue != static_cast<int>(index))
                {
                    return {};
                }
                profile.controls.bindings[index] = static_cast<Keys>(keyValue);
            }
            if (!AreValidControlSettings(profile.controls))
                return {};

            std::size_t highScoreCount = 0;
            if (!(input >> highScoreCount) || highScoreCount > MaximumHighScoreEntries)
                return {};
            profile.highScores.reserve(highScoreCount);
            for (std::size_t index = 0; index < highScoreCount; ++index)
            {
                HighScoreEntry entry;
                if (!(input >> entry.initials >> entry.score) ||
                    !AreValidInitials(entry.initials) ||
                    entry.score <= 0 || entry.score > MaximumHighScoreValue)
                {
                    return {};
                }
                profile.highScores.push_back(std::move(entry));
            }
            if (input >> trailing)
                return {};
            profile.highScores = NormalizeHighScores(std::move(profile.highScores));
        }
        else
        {
            return {};
        }

        profile.highestUnlocked = std::clamp(profile.highestUnlocked, 0, levelCount - 1);
        return profile;
    }

    std::string CampaignProgress::Serialize(
        const CampaignProfile& profile,
        int levelCount)
    {
        const int maximum = std::max(0, levelCount - 1);
        const int fieldOfView = IsSupportedFieldOfView(profile.fieldOfView)
            ? profile.fieldOfView
            : 72;
        const ControlSettings controls = AreValidControlSettings(profile.controls)
            ? profile.controls
            : ControlSettings{};
        const std::vector<HighScoreEntry> highScores = NormalizeHighScores(profile.highScores);
        std::ostringstream output;
        output << Header << '\n'
            << std::clamp(profile.highestUnlocked, 0, maximum) << '\n'
            << std::clamp(profile.soundVolume, 0, 4) << '\n'
            << std::clamp(profile.difficulty, 0, 2) << '\n'
            << fieldOfView << '\n'
            << controls.turnSensitivityStep << '\n'
            << (controls.mouseEnabled ? 1 : 0) << '\n'
            << controls.mouseSensitivityStep << '\n'
            << controls.bindings.size() << '\n';
        for (std::size_t index = 0; index < controls.bindings.size(); ++index)
            output << index << ' ' << static_cast<int>(controls.bindings[index]) << '\n';
        output << highScores.size() << '\n';
        for (const HighScoreEntry& entry : highScores)
            output << entry.initials << ' ' << entry.score << '\n';
        return output.str();
    }
}
