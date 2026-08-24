#include "CampaignProgress.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>

namespace WolfCna
{
    namespace
    {
        constexpr std::string_view LegacyHeader = "WOLF-CNA-PROGRESS-1";
        constexpr std::string_view BooleanSoundHeader = "WOLF-CNA-PROGRESS-2";
        constexpr std::string_view VolumeHeader = "WOLF-CNA-PROGRESS-3";
        constexpr std::string_view Header = "WOLF-CNA-PROGRESS-4";
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
        else if (header == Header)
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
        return std::string(Header) + "\n" +
            std::to_string(std::clamp(profile.highestUnlocked, 0, maximum)) + "\n" +
            std::to_string(std::clamp(profile.soundVolume, 0, 4)) + "\n" +
            std::to_string(std::clamp(profile.difficulty, 0, 2)) + "\n" +
            std::to_string(fieldOfView) + "\n";
    }
}
